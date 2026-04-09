//---------------------------------------------------------------------------
// Modbus TCP test slave — pure WinSock, no external libraries.
//
// Maintains a simple register bank with a deterministic initial state:
//   holdingRegs[i] = i          (FC3 read / FC6, FC16, FC22 write)
//   inputRegs[i]   = 0x1000 + i (FC4 read-only)
//
// Default port: 5020 (no admin rights required, unlike port 502).
// Override: ModbusTestServer.exe <port>
//
// Single-threaded: handles one client at a time, keeps connection alive
// until the client disconnects, then waits for the next one.
//---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#pragma comment(lib, "ws2_32")

static const uint16_t DEFAULT_PORT = 5020;
static const int      REG_COUNT    = 256;   // addresses 0x0000..0x00FF

//---------------------------------------------------------------------------
// Register bank
//---------------------------------------------------------------------------
static uint16_t holdingRegs[REG_COUNT];
static uint16_t inputRegs[REG_COUNT];

static void initRegisters()
{
    for (int i = 0; i < REG_COUNT; ++i) {
        holdingRegs[i] = static_cast<uint16_t>(i);
        inputRegs[i]   = static_cast<uint16_t>(0x1000 + i);
    }
}

//---------------------------------------------------------------------------
// Socket helpers
//---------------------------------------------------------------------------
static bool recvAll(SOCKET s, uint8_t* buf, int len)
{
    int received = 0;
    while (received < len) {
        int r = recv(s, reinterpret_cast<char*>(buf + received), len - received, 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

static bool sendAll(SOCKET s, const uint8_t* buf, int len)
{
    int sent = 0;
    while (sent < len) {
        int r = send(s, reinterpret_cast<const char*>(buf + sent), len - sent, 0);
        if (r == SOCKET_ERROR) return false;
        sent += r;
    }
    return true;
}

//---------------------------------------------------------------------------
// MBAP helpers
//---------------------------------------------------------------------------
static uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
static void     put16(uint8_t* p, uint16_t v) { p[0] = static_cast<uint8_t>(v >> 8); p[1] = static_cast<uint8_t>(v & 0xFF); }

// Wraps a PDU in a full MBAP frame.
// MBAP layout: TID(2) + PID(2=0) + LEN(2) + UnitID(1) + PDU(n)
// LEN = 1 (UnitID) + PDU size
static std::vector<uint8_t> buildFrame(uint16_t tid, uint8_t unitId,
                                        const std::vector<uint8_t>& pdu)
{
    uint16_t len = static_cast<uint16_t>(1 + pdu.size());
    std::vector<uint8_t> frame(6 + 1 + pdu.size());
    put16(frame.data() + 0, tid);
    put16(frame.data() + 2, 0);
    put16(frame.data() + 4, len);
    frame[6] = unitId;
    memcpy(frame.data() + 7, pdu.data(), pdu.size());
    return frame;
}

static std::vector<uint8_t> errorPdu(uint8_t fc, uint8_t exCode)
{
    return { static_cast<uint8_t>(fc | 0x80), exCode };
}

//---------------------------------------------------------------------------
// FC handlers (receive data = bytes after the function code byte)
//---------------------------------------------------------------------------

// FC03: Read Holding Registers
static std::vector<uint8_t> handleFC03(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x03, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    if (count == 0 || count > 125)       return errorPdu(0x03, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x03, 0x02);

    std::vector<uint8_t> pdu;
    pdu.push_back(0x03);
    pdu.push_back(static_cast<uint8_t>(count * 2));
    for (int i = 0; i < count; ++i) {
        uint16_t v = holdingRegs[addr + i];
        pdu.push_back(static_cast<uint8_t>(v >> 8));
        pdu.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    return pdu;
}

// FC04: Read Input Registers
static std::vector<uint8_t> handleFC04(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x04, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    if (count == 0 || count > 125)       return errorPdu(0x04, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x04, 0x02);

    std::vector<uint8_t> pdu;
    pdu.push_back(0x04);
    pdu.push_back(static_cast<uint8_t>(count * 2));
    for (int i = 0; i < count; ++i) {
        uint16_t v = inputRegs[addr + i];
        pdu.push_back(static_cast<uint8_t>(v >> 8));
        pdu.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    return pdu;
}

// FC01: Read Coil Status
static std::vector<uint8_t> handleFC01(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x01, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    if (count == 0 || count > 2000)      return errorPdu(0x01, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x01, 0x02);

    const uint8_t byteCount = static_cast<uint8_t>((count + 7) / 8);
    std::vector<uint8_t> pdu { 0x01, byteCount };
    pdu.resize(static_cast<size_t>(2 + byteCount), 0);

    for (uint16_t i = 0; i < count; ++i) {
        // coil pattern: coilRegs[i] = i & 1
        if (static_cast<uint16_t>(addr + i) & 1) {
            pdu[2 + i / 8] |= static_cast<uint8_t>(1u << (i % 8));
        }
    }
    return pdu;
}

// FC02: Read Input Status
static std::vector<uint8_t> handleFC02(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x02, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    if (count == 0 || count > 2000)      return errorPdu(0x02, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x02, 0x02);

    const uint8_t byteCount = static_cast<uint8_t>((count + 7) / 8);
    std::vector<uint8_t> pdu { 0x02, byteCount };
    pdu.resize(static_cast<size_t>(2 + byteCount), 0);

    for (uint16_t i = 0; i < count; ++i) {
        if (((addr + i) % 3) == 0) {
            pdu[2 + i / 8] |= static_cast<uint8_t>(1u << (i % 8));
        }
    }
    return pdu;
}

// FC05: Force Single Coil — echoes the request on success
static std::vector<uint8_t> handleFC05(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x05, 0x03);
    uint16_t addr  = get16(d);
    uint16_t value = get16(d + 2);
    if (value != 0xFF00 && value != 0x0000) return errorPdu(0x05, 0x03);
    if (addr >= REG_COUNT) return errorPdu(0x05, 0x02);

    holdingRegs[addr] = (value == 0xFF00) ? 1 : 0; // store coil state in holdingRegs for simplicity
    return { 0x05, d[0], d[1], d[2], d[3] };
}

// FC06: Preset Single Register — echoes the request on success
static std::vector<uint8_t> handleFC06(const uint8_t* d, int len)
{
    if (len < 4) return errorPdu(0x06, 0x03);
    uint16_t addr  = get16(d);
    uint16_t value = get16(d + 2);
    if (addr >= REG_COUNT) return errorPdu(0x06, 0x02);

    holdingRegs[addr] = value;
    return { 0x06, d[0], d[1], d[2], d[3] };
}

// FC16 (0x10): Preset Multiple Registers
// Request data: StartAddr(2) + PointCount(2) + ByteCount(1) + Values(ByteCount)
// Response PDU: FC(1) + StartAddr(2) + PointCount(2)
static std::vector<uint8_t> handleFC16(const uint8_t* d, int len)
{
    if (len < 5)                         return errorPdu(0x10, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    uint8_t  bytes = d[4];
    if (count == 0 || count > 123)       return errorPdu(0x10, 0x03);
    if (bytes != count * 2)              return errorPdu(0x10, 0x03);
    if (len < 5 + bytes)                 return errorPdu(0x10, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x10, 0x02);

    for (int i = 0; i < count; ++i)
        holdingRegs[addr + i] = get16(d + 5 + i * 2);

    return { 0x10, d[0], d[1], d[2], d[3] };
}

// FC15 (0x0F): Force Multiple Coils
static std::vector<uint8_t> handleFC15(const uint8_t* d, int len)
{
    if (len < 5)                         return errorPdu(0x0F, 0x03);
    uint16_t addr  = get16(d);
    uint16_t count = get16(d + 2);
    uint8_t  bytes = d[4];
    if (count == 0 || count > 1968)      return errorPdu(0x0F, 0x03);
    if (bytes != (count + 7) / 8)        return errorPdu(0x0F, 0x03);
    if (len < 5 + bytes)                 return errorPdu(0x0F, 0x03);
    if (addr + count > REG_COUNT)        return errorPdu(0x0F, 0x02);

    // Store coil values in holdingRegs for simplicity
    for (uint16_t i = 0; i < count; ++i)
        holdingRegs[addr + i] = (d[5 + i / 8] >> (i % 8)) & 1;

    return { 0x0F, d[0], d[1], d[2], d[3] };
}

// FC22 (0x16): Mask Write 4X Register
// Result = (Current AND AndMask) OR (OrMask AND NOT AndMask)
// Request data: Addr(2) + AndMask(2) + OrMask(2)
// Response PDU: echoes request
static std::vector<uint8_t> handleFC22(const uint8_t* d, int len)
{
    if (len < 6) return errorPdu(0x16, 0x03);
    uint16_t addr    = get16(d);
    uint16_t andMask = get16(d + 2);
    uint16_t orMask  = get16(d + 4);
    if (addr >= REG_COUNT) return errorPdu(0x16, 0x02);

    holdingRegs[addr] = static_cast<uint16_t>(
        (holdingRegs[addr] & andMask) | (orMask & ~andMask));

    return { 0x16, d[0], d[1], d[2], d[3], d[4], d[5] };
}

// FC23 (0x17): Read/Write Multiple Registers
// Write is performed before read (per Modbus spec).
static std::vector<uint8_t> handleFC23(const uint8_t* d, int len)
{
    if (len < 9) return errorPdu(0x17, 0x03);
    uint16_t rAddr  = get16(d);
    uint16_t rCount = get16(d + 2);
    uint16_t wAddr  = get16(d + 4);
    uint16_t wCount = get16(d + 6);
    uint8_t  wBytes = d[8];
    if (rCount == 0 || rCount > 125)     return errorPdu(0x17, 0x03);
    if (wCount == 0 || wCount > 121)     return errorPdu(0x17, 0x03);
    if (wBytes != wCount * 2)            return errorPdu(0x17, 0x03);
    if (len < 9 + wBytes)               return errorPdu(0x17, 0x03);
    if (rAddr + rCount > REG_COUNT)      return errorPdu(0x17, 0x02);
    if (wAddr + wCount > REG_COUNT)      return errorPdu(0x17, 0x02);

    for (int i = 0; i < wCount; ++i)
        holdingRegs[wAddr + i] = get16(d + 9 + i * 2);

    std::vector<uint8_t> pdu { 0x17, static_cast<uint8_t>(rCount * 2) };
    for (int i = 0; i < rCount; ++i) {
        uint16_t v = holdingRegs[rAddr + i];
        pdu.push_back(static_cast<uint8_t>(v >> 8));
        pdu.push_back(static_cast<uint8_t>(v & 0xFF));
    }
    return pdu;
}

//---------------------------------------------------------------------------
// Request dispatcher — reads one MBAP request, sends one response.
// Returns false when the connection should be closed.
//---------------------------------------------------------------------------
static bool handleRequest(SOCKET s)
{
    // MBAP: TID(2) + PID(2) + LEN(2) — 6 bytes before the payload
    uint8_t header[6];
    if (!recvAll(s, header, 6)) return false;

    uint16_t tid       = get16(header + 0);
    uint16_t remaining = get16(header + 4); // UnitID + FC + data

    if (remaining < 2) return false;

    std::vector<uint8_t> body(remaining);
    if (!recvAll(s, body.data(), remaining)) return false;

    uint8_t        unitId     = body[0];
    uint8_t        fc         = body[1];
    const uint8_t* data       = body.data() + 2;
    int            dataLen    = static_cast<int>(remaining) - 2;

    std::vector<uint8_t> responsePdu;
    switch (fc) {
        case 0x01: responsePdu = handleFC01(data, dataLen); break;
        case 0x02: responsePdu = handleFC02(data, dataLen); break;
        case 0x03: responsePdu = handleFC03(data, dataLen); break;
        case 0x04: responsePdu = handleFC04(data, dataLen); break;
        case 0x05: responsePdu = handleFC05(data, dataLen); break;
        case 0x06: responsePdu = handleFC06(data, dataLen); break;
        case 0x0F: responsePdu = handleFC15(data, dataLen); break;
        case 0x10: responsePdu = handleFC16(data, dataLen); break;
        case 0x16: responsePdu = handleFC22(data, dataLen); break;
        case 0x17: responsePdu = handleFC23(data, dataLen); break;
        default:
            responsePdu = errorPdu(fc, 0x01); // Illegal function
            break;
    }

    printf("  FC=0x%02X  unit=%3u  tid=%5u  -> %d byte(s) PDU\n",
           fc, unitId, tid, static_cast<int>(responsePdu.size()));

    auto frame = buildFrame(tid, unitId, responsePdu);
    return sendAll(s, frame.data(), static_cast<int>(frame.size()));
}

//---------------------------------------------------------------------------
// Per-client loop
//---------------------------------------------------------------------------
static void serveClient(SOCKET s, const char* addr)
{
    printf("[+] Connected:    %s\n", addr);
    while (handleRequest(s)) { /* keep-alive: loop until client disconnects */ }
    printf("[-] Disconnected: %s\n\n", addr);
    closesocket(s);
}

//---------------------------------------------------------------------------
// Entry point
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    uint16_t port = DEFAULT_PORT;
    if (argc >= 2) port = static_cast<uint16_t>(atoi(argv[1]));

    printf("Modbus TCP Test Server\n");
    printf("  Port         : %u\n", port);
    printf("  Register bank: %d holding + %d input (addresses 0x0000..0x%04X)\n",
           REG_COUNT, REG_COUNT, REG_COUNT - 1);
    printf("  Initial state: holdingRegs[i]=i, inputRegs[i]=0x1000+i\n\n");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    initRegisters();

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Allow immediate reuse of the port after the server is restarted
    int reuseAddr = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddr), sizeof(reuseAddr));

    sockaddr_in addr4 = {};
    addr4.sin_family      = AF_INET;
    addr4.sin_port        = htons(port);
    addr4.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only — not exposed on the network

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr4), sizeof(addr4)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
        closesocket(listenSock); WSACleanup(); return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "listen() failed: %d\n", WSAGetLastError());
        closesocket(listenSock); WSACleanup(); return 1;
    }

    printf("Listening on 127.0.0.1:%u  (Ctrl+C to stop)\n\n", port);

    for (;;) {
        sockaddr_in clientAddr = {};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock,
                                   reinterpret_cast<sockaddr*>(&clientAddr),
                                   &addrLen);
        if (clientSock == INVALID_SOCKET) {
            fprintf(stderr, "accept() failed: %d\n", WSAGetLastError());
            break;
        }

        char addrBuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrBuf, sizeof(addrBuf));

        serveClient(clientSock, addrBuf);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
