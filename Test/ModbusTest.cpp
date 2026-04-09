//---------------------------------------------------------------------------
// Modbus TCP test suite — Boost.Test 1.89, static library variant.
//
// A Modbus TCP slave runs in a background std::thread on 127.0.0.1:5020.
// ServerFixture (global fixture) starts it before any test runs and stops
// it cleanly after the last test completes.
//
// Server initial register state:
//   coilRegs[i]   = (i & 1)        (FC01)
//   inputBits[i]  = ((i % 3) == 0) (FC02)
//   holdingRegs[i] = i          (FC03 / FC06 / FC16 / FC22)
//   inputRegs[i]   = 0x1000 + i (FC04, read-only)
//---------------------------------------------------------------------------

#pragma hdrstop

#include <System.SysUtils.hpp>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <future>
#include <tchar.h>
#include <thread>
#include <vector>

#include "ModbusTCP_IP.h"
#include "ModbusTCP_WinSock.h"
#include "ModbusDummy.h"
#include "ModbusRTU.h"

// --- Boost.Test static-link -----------------------------------------------
// Keep Boost-provided main() and use a Unicode _tmain wrapper at the end
// of this file to forward wide argv to the narrow main entry point.
#define BOOST_TEST_MODULE ModbusTCP
#include <boost/test/included/unit_test.hpp>

#pragma comment(lib, "ws2_32")

using namespace Modbus;
using namespace Modbus::Master;

//---------------------------------------------------------------------------
// Embedded server
//---------------------------------------------------------------------------

static const uint16_t SERVER_PORT = 5020;
static const int      REG_COUNT   = 256;

static std::atomic<bool> gServerStop { false };
static uint8_t           coilRegs[REG_COUNT];
static uint8_t           inputBits[REG_COUNT];
static uint16_t          holdingRegs[REG_COUNT];
static uint16_t          inputRegs[REG_COUNT];

static void initRegisters()
{
    for ( int i = 0; i < REG_COUNT; ++i ) {
        coilRegs[i]    = static_cast<uint8_t>( i & 1 );          // 0,1,0,1,...
        inputBits[i]   = static_cast<uint8_t>( ( i % 3 ) == 0 ); // 1,0,0,1,0,0,...
        holdingRegs[i] = static_cast<uint16_t>( i );
        inputRegs[i]   = static_cast<uint16_t>( 0x1000 + i );
    }
}

static bool srvRecvAll( SOCKET s, uint8_t* buf, int len )
{
    int done = 0;
    while ( done < len ) {
        int r = recv( s, reinterpret_cast<char*>( buf + done ), len - done, 0 );
        if ( r <= 0 ) return false;
        done += r;
    }
    return true;
}

static bool srvSendAll( SOCKET s, const uint8_t* buf, int len )
{
    int done = 0;
    while ( done < len ) {
        int r = send( s, reinterpret_cast<const char*>( buf + done ), len - done, 0 );
        if ( r == SOCKET_ERROR ) return false;
        done += r;
    }
    return true;
}

static uint16_t get16( const uint8_t* p )
{
    return static_cast<uint16_t>( ( p[0] << 8 ) | p[1] );
}

static void put16( uint8_t* p, uint16_t v )
{
    p[0] = static_cast<uint8_t>( v >> 8 );
    p[1] = static_cast<uint8_t>( v & 0xFF );
}

static std::vector<uint8_t> buildFrame( uint16_t tid, uint8_t unitId,
                                        const std::vector<uint8_t>& pdu )
{
    uint16_t len = static_cast<uint16_t>( 1 + pdu.size() );
    std::vector<uint8_t> frame( 6 + 1 + pdu.size() );
    put16( frame.data() + 0, tid );
    put16( frame.data() + 2, 0 );
    put16( frame.data() + 4, len );
    frame[6] = unitId;
    memcpy( frame.data() + 7, pdu.data(), pdu.size() );
    return frame;
}

static std::vector<uint8_t> errorPdu( uint8_t fc, uint8_t exCode )
{
    return { static_cast<uint8_t>( fc | 0x80 ), exCode };
}

static std::vector<uint8_t> handleFC03( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x03, 0x03 );
    uint16_t addr = get16( d ), count = get16( d + 2 );
    if ( count == 0 || count > 125 )  return errorPdu( 0x03, 0x03 );
    if ( addr + count > REG_COUNT )   return errorPdu( 0x03, 0x02 );
    std::vector<uint8_t> pdu { 0x03, static_cast<uint8_t>( count * 2 ) };
    for ( int i = 0; i < count; ++i ) {
        uint16_t v = holdingRegs[addr + i];
        pdu.push_back( v >> 8 ); pdu.push_back( v & 0xFF );
    }
    return pdu;
}

static std::vector<uint8_t> handleFC01( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x01, 0x03 );
    uint16_t addr = get16( d ), count = get16( d + 2 );
    if ( count == 0 || count > 2000 ) return errorPdu( 0x01, 0x03 );
    if ( addr + count > REG_COUNT )   return errorPdu( 0x01, 0x02 );

    const uint8_t byteCount = static_cast<uint8_t>( ( count + 7 ) / 8 );
    std::vector<uint8_t> pdu { 0x01, byteCount };
    pdu.resize( static_cast<size_t>( 2 + byteCount ), 0 );

    for ( uint16_t i = 0; i < count; ++i ) {
        if ( coilRegs[addr + i] ) {
            pdu[2 + i / 8] |= static_cast<uint8_t>( 1u << ( i % 8 ) );
        }
    }
    return pdu;
}

static std::vector<uint8_t> handleFC02( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x02, 0x03 );
    uint16_t addr = get16( d ), count = get16( d + 2 );
    if ( count == 0 || count > 2000 ) return errorPdu( 0x02, 0x03 );
    if ( addr + count > REG_COUNT )   return errorPdu( 0x02, 0x02 );

    const uint8_t byteCount = static_cast<uint8_t>( ( count + 7 ) / 8 );
    std::vector<uint8_t> pdu { 0x02, byteCount };
    pdu.resize( static_cast<size_t>( 2 + byteCount ), 0 );

    for ( uint16_t i = 0; i < count; ++i ) {
        if ( inputBits[addr + i] ) {
            pdu[2 + i / 8] |= static_cast<uint8_t>( 1u << ( i % 8 ) );
        }
    }
    return pdu;
}

static std::vector<uint8_t> handleFC04( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x04, 0x03 );
    uint16_t addr = get16( d ), count = get16( d + 2 );
    if ( count == 0 || count > 125 )  return errorPdu( 0x04, 0x03 );
    if ( addr + count > REG_COUNT )   return errorPdu( 0x04, 0x02 );
    std::vector<uint8_t> pdu { 0x04, static_cast<uint8_t>( count * 2 ) };
    for ( int i = 0; i < count; ++i ) {
        uint16_t v = inputRegs[addr + i];
        pdu.push_back( v >> 8 ); pdu.push_back( v & 0xFF );
    }
    return pdu;
}

static std::vector<uint8_t> handleFC05( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x05, 0x03 );
    uint16_t addr  = get16( d );
    uint16_t value = get16( d + 2 );
    if ( value != 0xFF00 && value != 0x0000 ) return errorPdu( 0x05, 0x03 );
    if ( addr >= REG_COUNT ) return errorPdu( 0x05, 0x02 );
    coilRegs[addr] = ( value == 0xFF00 ) ? 1 : 0;
    return { 0x05, d[0], d[1], d[2], d[3] };
}

static std::vector<uint8_t> handleFC06( const uint8_t* d, int len )
{
    if ( len < 4 ) return errorPdu( 0x06, 0x03 );
    uint16_t addr = get16( d ), value = get16( d + 2 );
    if ( addr >= REG_COUNT ) return errorPdu( 0x06, 0x02 );
    holdingRegs[addr] = value;
    return { 0x06, d[0], d[1], d[2], d[3] };
}

static std::vector<uint8_t> handleFC15( const uint8_t* d, int len )
{
    if ( len < 5 ) return errorPdu( 0x0F, 0x03 );
    uint16_t addr  = get16( d );
    uint16_t count = get16( d + 2 );
    uint8_t  bytes = d[4];
    if ( count == 0 || count > 1968 )       return errorPdu( 0x0F, 0x03 );
    if ( bytes != ( count + 7 ) / 8 )       return errorPdu( 0x0F, 0x03 );
    if ( len < 5 + bytes )                  return errorPdu( 0x0F, 0x03 );
    if ( addr + count > REG_COUNT )         return errorPdu( 0x0F, 0x02 );
    for ( uint16_t i = 0; i < count; ++i ) {
        coilRegs[addr + i] =
            ( d[5 + i / 8] >> ( i % 8 ) ) & 1;
    }
    return { 0x0F, d[0], d[1], d[2], d[3] };
}

static std::vector<uint8_t> handleFC16( const uint8_t* d, int len )
{
    if ( len < 5 ) return errorPdu( 0x10, 0x03 );
    uint16_t addr = get16( d ), count = get16( d + 2 );
    uint8_t  bytes = d[4];
    if ( count == 0 || count > 123 ) return errorPdu( 0x10, 0x03 );
    if ( bytes != count * 2 )        return errorPdu( 0x10, 0x03 );
    if ( len < 5 + bytes )           return errorPdu( 0x10, 0x03 );
    if ( addr + count > REG_COUNT )  return errorPdu( 0x10, 0x02 );
    for ( int i = 0; i < count; ++i )
        holdingRegs[addr + i] = get16( d + 5 + i * 2 );
    return { 0x10, d[0], d[1], d[2], d[3] };
}

static std::vector<uint8_t> handleFC22( const uint8_t* d, int len )
{
    if ( len < 6 ) return errorPdu( 0x16, 0x03 );
    uint16_t addr = get16( d ), andMask = get16( d + 2 ), orMask = get16( d + 4 );
    if ( addr >= REG_COUNT ) return errorPdu( 0x16, 0x02 );
    holdingRegs[addr] = static_cast<uint16_t>(
        ( holdingRegs[addr] & andMask ) | ( orMask & ~andMask ) );
    return { 0x16, d[0], d[1], d[2], d[3], d[4], d[5] };
}

static std::vector<uint8_t> handleFC23( const uint8_t* d, int len )
{
    // ReadAddr(2) + ReadCount(2) + WriteAddr(2) + WriteCount(2) + WriteByteCnt(1) + WriteData
    if ( len < 9 ) return errorPdu( 0x17, 0x03 );
    uint16_t rAddr  = get16( d );
    uint16_t rCount = get16( d + 2 );
    uint16_t wAddr  = get16( d + 4 );
    uint16_t wCount = get16( d + 6 );
    uint8_t  wBytes = d[8];
    if ( rCount == 0 || rCount > 125 )       return errorPdu( 0x17, 0x03 );
    if ( wCount == 0 || wCount > 121 )       return errorPdu( 0x17, 0x03 );
    if ( wBytes != wCount * 2 )              return errorPdu( 0x17, 0x03 );
    if ( len < 9 + wBytes )                  return errorPdu( 0x17, 0x03 );
    if ( rAddr + rCount > REG_COUNT )        return errorPdu( 0x17, 0x02 );
    if ( wAddr + wCount > REG_COUNT )        return errorPdu( 0x17, 0x02 );

    // Write first (per Modbus spec, write is performed before read)
    for ( int i = 0; i < wCount; ++i )
        holdingRegs[wAddr + i] = get16( d + 9 + i * 2 );

    // Then read
    std::vector<uint8_t> pdu { 0x17, static_cast<uint8_t>( rCount * 2 ) };
    for ( int i = 0; i < rCount; ++i ) {
        uint16_t v = holdingRegs[rAddr + i];
        pdu.push_back( v >> 8 ); pdu.push_back( v & 0xFF );
    }
    return pdu;
}

static bool handleRequest( SOCKET s )
{
    uint8_t header[6];
    if ( !srvRecvAll( s, header, 6 ) ) return false;
    uint16_t tid       = get16( header + 0 );
    uint16_t remaining = get16( header + 4 );
    if ( remaining < 2 ) return false;
    std::vector<uint8_t> body( remaining );
    if ( !srvRecvAll( s, body.data(), remaining ) ) return false;

    uint8_t        unitId  = body[0];
    uint8_t        fc      = body[1];
    const uint8_t* data    = body.data() + 2;
    int            dataLen = static_cast<int>( remaining ) - 2;

    std::vector<uint8_t> pdu;
    switch ( fc ) {
        case 0x01: pdu = handleFC01( data, dataLen ); break;
        case 0x02: pdu = handleFC02( data, dataLen ); break;
        case 0x03: pdu = handleFC03( data, dataLen ); break;
        case 0x04: pdu = handleFC04( data, dataLen ); break;
        case 0x05: pdu = handleFC05( data, dataLen ); break;
        case 0x06: pdu = handleFC06( data, dataLen ); break;
        case 0x0F: pdu = handleFC15( data, dataLen ); break;
        case 0x10: pdu = handleFC16( data, dataLen ); break;
        case 0x16: pdu = handleFC22( data, dataLen ); break;
        case 0x17: pdu = handleFC23( data, dataLen ); break;
        default:   pdu = errorPdu( fc, 0x01 );        break;
    }

    auto frame = buildFrame( tid, unitId, pdu );
    return srvSendAll( s, frame.data(), static_cast<int>( frame.size() ) );
}

static void serverThread( std::promise<void> readyPromise )
{
    WSADATA wsaData;
    WSAStartup( MAKEWORD( 2, 2 ), &wsaData );

    SOCKET listenSock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    int reuseAddr = 1;
    setsockopt( listenSock, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>( &reuseAddr ), sizeof( reuseAddr ) );

    sockaddr_in addr4 = {};
    addr4.sin_family      = AF_INET;
    addr4.sin_port        = htons( SERVER_PORT );
    addr4.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    bind  ( listenSock, reinterpret_cast<sockaddr*>( &addr4 ), sizeof( addr4 ) );
    listen( listenSock, SOMAXCONN );

    readyPromise.set_value(); // port is open — tests may begin

    while ( !gServerStop ) {
        fd_set readSet;
        FD_ZERO( &readSet );
        FD_SET( listenSock, &readSet );
        timeval tv = { 0, 100000 }; // 100 ms
        if ( select( 0, &readSet, nullptr, nullptr, &tv ) > 0 ) {
            sockaddr_in clientAddr = {};
            int addrLen = sizeof( clientAddr );
            SOCKET clientSock = accept( listenSock,
                                        reinterpret_cast<sockaddr*>( &clientAddr ),
                                        &addrLen );
            if ( clientSock != INVALID_SOCKET ) {
                DWORD readTimeout = 2000;
                setsockopt( clientSock, SOL_SOCKET, SO_RCVTIMEO,
                            reinterpret_cast<const char*>( &readTimeout ),
                            sizeof( readTimeout ) );
                while ( handleRequest( clientSock ) ) {}
                closesocket( clientSock );
            }
        }
    }

    closesocket( listenSock );
    WSACleanup();
}

//---------------------------------------------------------------------------
// Global fixture — starts/stops the server around the entire test run
//---------------------------------------------------------------------------

struct ServerFixture {
    ServerFixture()
    {
        initRegisters();
        std::promise<void> ready;
        readyFuture_ = ready.get_future();
        thread_ = std::thread( serverThread, std::move( ready ) );
        readyFuture_.wait();
        BOOST_TEST_MESSAGE( "Embedded Modbus server listening on 127.0.0.1:" << SERVER_PORT );
    }
    ~ServerFixture()
    {
        try {
            gServerStop = true;
            thread_.join();
        }
        catch ( ... ) {
        }
    }
private:
    std::thread        thread_;
    std::future<void>  readyFuture_;
};

BOOST_TEST_GLOBAL_FIXTURE( ServerFixture );

//---------------------------------------------------------------------------
// Per-suite fixture — creates a fresh protocol connection for each suite
// and resets the register bank so suites are independent
//---------------------------------------------------------------------------

struct ProtoFixture {
    ProtoFixture()
        : proto_( _D( "127.0.0.1" ), SERVER_PORT )
        , session_( proto_ )
    {
        initRegisters(); // fresh state for every suite
    }
    TCPProtocolWinSock proto_;
    SessionManager     session_;
};

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------

static TCPIPContext ctx( uint8_t slave = 1, uint16_t tid = 1 )
{
    return TCPIPContext( slave, tid );
}

static uint16_t readH( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    RegDataType val = 0;
    p.ReadHoldingRegisters( ctx( slave ), addr, 1, &val );
    return val;
}

static uint16_t readI( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    RegDataType val = 0;
    p.ReadInputRegisters( ctx( slave ), addr, 1, &val );
    return val;
}

static uint8_t readC( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    CoilDataType packed = 0;
    p.ReadCoilStatus( ctx( slave ), addr, 1, &packed );
    return static_cast<uint8_t>( packed & 0x01u );
}

static uint8_t readD( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    CoilDataType packed = 0;
    p.ReadInputStatus( ctx( slave ), addr, 1, &packed );
    return static_cast<uint8_t>( packed & 0x01u );
}

//---------------------------------------------------------------------------
// Test suites
//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC01_ReadCoilStatus, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleCoilAlternatingPattern )
    {
        BOOST_TEST( readC( proto_, 0 ) == 0u );
        BOOST_TEST( readC( proto_, 1 ) == 1u );
        BOOST_TEST( readC( proto_, 2 ) == 0u );
        BOOST_TEST( readC( proto_, 3 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( PackedBitOrderLSBFirst )
    {
        CoilDataType buf[2] = { 0, 0 };
        proto_.ReadCoilStatus( ctx(), 0, 16, buf );
        BOOST_TEST( buf[0] == 0xAAu );
        BOOST_TEST( buf[1] == 0xAAu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto_.ReadCoilStatus( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC02_ReadInputStatus, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleInputModuloPattern )
    {
        BOOST_TEST( readD( proto_, 0 ) == 1u );
        BOOST_TEST( readD( proto_, 1 ) == 0u );
        BOOST_TEST( readD( proto_, 2 ) == 0u );
        BOOST_TEST( readD( proto_, 3 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( PackedBitOrderLSBFirst )
    {
        CoilDataType buf[2] = { 0, 0 };
        proto_.ReadInputStatus( ctx(), 0, 16, buf );
        BOOST_TEST( buf[0] == 0x49u );
        BOOST_TEST( buf[1] == 0x92u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto_.ReadInputStatus( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC03_ReadHoldingRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleRegisterEqualsAddress )
    {
        BOOST_TEST( readH( proto_, 0 )  == 0  );
        BOOST_TEST( readH( proto_, 1 )  == 1  );
        BOOST_TEST( readH( proto_, 10 ) == 10 );
    }

    BOOST_AUTO_TEST_CASE( BlockReadSequentialValues )
    {
        const uint16_t START = 20, COUNT = 8;
        std::vector<RegDataType> buf( COUNT );
        proto_.ReadHoldingRegisters( ctx(), START, COUNT, buf.data() );
        for ( int i = 0; i < COUNT; ++i )
            BOOST_TEST( buf[i] == START + i );
    }

    BOOST_AUTO_TEST_CASE( BoundaryAddress )
    {
        BOOST_TEST( readH( proto_, 255 ) == 255 );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2];
        BOOST_CHECK_THROW(
            proto_.ReadHoldingRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC04_ReadInputRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleRegisterEqualsBase )
    {
        BOOST_TEST( readI( proto_, 0 )   == 0x1000u );
        BOOST_TEST( readI( proto_, 1 )   == 0x1001u );
        BOOST_TEST( readI( proto_, 255 ) == 0x10FFu );
    }

    BOOST_AUTO_TEST_CASE( BlockReadSequentialValues )
    {
        const uint16_t START = 5, COUNT = 4;
        std::vector<RegDataType> buf( COUNT );
        proto_.ReadInputRegisters( ctx(), START, COUNT, buf.data() );
        for ( int i = 0; i < COUNT; ++i )
            BOOST_TEST( buf[i] == 0x1000u + START + i );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2];
        BOOST_CHECK_THROW(
            proto_.ReadInputRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC06_PresetSingleRegister, ProtoFixture )

    BOOST_AUTO_TEST_CASE( WriteAndReadBack )
    {
        proto_.PresetSingleRegister( ctx(), 50, 0xABCD );
        BOOST_TEST( readH( proto_, 50 ) == 0xABCDu );
    }

    BOOST_AUTO_TEST_CASE( OverwriteWithZero )
    {
        proto_.PresetSingleRegister( ctx(), 50, 0xABCD );
        proto_.PresetSingleRegister( ctx(), 50, 0x0000 );
        BOOST_TEST( readH( proto_, 50 ) == 0x0000u );
    }

    BOOST_AUTO_TEST_CASE( BoundaryAddress )
    {
        proto_.PresetSingleRegister( ctx(), 255, 0x1234 );
        BOOST_TEST( readH( proto_, 255 ) == 0x1234u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        BOOST_CHECK_THROW(
            proto_.PresetSingleRegister( ctx(), 256, 0xFF ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC16_PresetMultipleRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( BlockRoundTrip )
    {
        RegDataType writeVals[] = { 0x0100, 0x0200, 0x0300 };
        proto_.PresetMultipleRegisters( ctx(), 60, 3, writeVals );
        std::vector<RegDataType> readVals( 3 );
        proto_.ReadHoldingRegisters( ctx(), 60, 3, readVals.data() );
        BOOST_TEST( readVals[0] == 0x0100u );
        BOOST_TEST( readVals[1] == 0x0200u );
        BOOST_TEST( readVals[2] == 0x0300u );
    }

    BOOST_AUTO_TEST_CASE( SingleRegisterViaFC16 )
    {
        RegDataType val = 0xBEEF;
        proto_.PresetMultipleRegisters( ctx(), 70, 1, &val );
        BOOST_TEST( readH( proto_, 70 ) == 0xBEEFu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2] = { 1, 2 };
        BOOST_CHECK_THROW(
            proto_.PresetMultipleRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC22_MaskWrite4XRegister, ProtoFixture )

    BOOST_AUTO_TEST_CASE( MaskApplied )
    {
        // (0xAA55 & 0xFF00) | (0x00AA & 0x00FF) = 0xAA00 | 0x00AA = 0xAAAA
        proto_.PresetSingleRegister( ctx(), 80, 0xAA55 );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0xFF00, 0x00AA );
        BOOST_TEST( readH( proto_, 80 ) == 0xAAAAu );
    }

    BOOST_AUTO_TEST_CASE( ChainedMasks )
    {
        // starting from 0xAAAA: (0xAAAA & 0x0F0F) | (0xF0F0 & 0xF0F0) = 0x0A0A | 0xF0F0 = 0xFAFA
        proto_.PresetSingleRegister( ctx(), 80, 0xAAAA );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0x0F0F, 0xF0F0 );
        BOOST_TEST( readH( proto_, 80 ) == 0xFAFAu );
    }

    BOOST_AUTO_TEST_CASE( IdentityMaskLeavesValueUnchanged )
    {
        proto_.PresetSingleRegister( ctx(), 80, 0x5A5A );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0xFFFF, 0x0000 );
        BOOST_TEST( readH( proto_, 80 ) == 0x5A5Au );
    }

    BOOST_AUTO_TEST_CASE( ZeroAndMaskForcesOrMask )
    {
        proto_.PresetSingleRegister( ctx(), 80, 0xFFFF );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0x0000, 0x1234 );
        BOOST_TEST( readH( proto_, 80 ) == 0x1234u );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC05_ForceSingleCoil, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ForceOnAndReadBack )
    {
        // coilRegs[0] starts as 0 (even index)
        proto_.ForceSingleCoil( ctx(), 0, true );
        BOOST_TEST( readC( proto_, 0 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( ForceOffAndReadBack )
    {
        // coilRegs[1] starts as 1 (odd index)
        proto_.ForceSingleCoil( ctx(), 1, false );
        BOOST_TEST( readC( proto_, 1 ) == 0u );
    }

    BOOST_AUTO_TEST_CASE( ToggleCoil )
    {
        proto_.ForceSingleCoil( ctx(), 10, true );
        BOOST_TEST( readC( proto_, 10 ) == 1u );
        proto_.ForceSingleCoil( ctx(), 10, false );
        BOOST_TEST( readC( proto_, 10 ) == 0u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        BOOST_CHECK_THROW(
            proto_.ForceSingleCoil( ctx(), 256, true ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC15_ForceMultipleCoils, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ForceBlockAndReadBack )
    {
        // Force coils 0..7 to all ON (0xFF)
        CoilDataType data[1] = { 0xFF };
        proto_.ForceMultipleCoils( ctx(), 0, 8, data );
        CoilDataType readBuf[1] = { 0 };
        proto_.ReadCoilStatus( ctx(), 0, 8, readBuf );
        BOOST_TEST( readBuf[0] == 0xFFu );
    }

    BOOST_AUTO_TEST_CASE( ForcePatternAndReadBack )
    {
        // Force coils 16..31 to alternating pattern 0xA5, 0x5A
        CoilDataType data[2] = { 0xA5, 0x5A };
        proto_.ForceMultipleCoils( ctx(), 16, 16, data );
        CoilDataType readBuf[2] = { 0, 0 };
        proto_.ReadCoilStatus( ctx(), 16, 16, readBuf );
        BOOST_TEST( readBuf[0] == 0xA5u );
        BOOST_TEST( readBuf[1] == 0x5Au );
    }

    BOOST_AUTO_TEST_CASE( ForceAllOff )
    {
        CoilDataType data[1] = { 0x00 };
        proto_.ForceMultipleCoils( ctx(), 0, 8, data );
        CoilDataType readBuf[1] = { 0xFF };
        proto_.ReadCoilStatus( ctx(), 0, 8, readBuf );
        BOOST_TEST( readBuf[0] == 0x00u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType data[1] = { 0xFF };
        BOOST_CHECK_THROW(
            proto_.ForceMultipleCoils( ctx(), 255, 2, data ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC23_ReadWrite4XRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( WriteAndReadSameRegion )
    {
        // Write 0xAAAA to addr 100, then read it back atomically
        RegDataType writeVals[] = { 0xAAAA };
        RegDataType readVal = 0;
        proto_.ReadWrite4XRegisters( ctx(),
            100, 1, &readVal,    // read addr 100
            100, 1, writeVals ); // write addr 100
        // Write happens first, so read should see the written value
        BOOST_TEST( readVal == 0xAAAAu );
    }

    BOOST_AUTO_TEST_CASE( WriteThenReadDifferentRegions )
    {
        RegDataType writeVals[] = { 0x1111, 0x2222 };
        RegDataType readVals[3] = { 0, 0, 0 };
        proto_.ReadWrite4XRegisters( ctx(),
            0, 3, readVals,          // read holding regs 0..2
            50, 2, writeVals );      // write to 50..51
        // Regs 0..2 were not modified, should still be 0,1,2
        BOOST_TEST( readVals[0] == 0u );
        BOOST_TEST( readVals[1] == 1u );
        BOOST_TEST( readVals[2] == 2u );
        // Verify write took effect
        BOOST_TEST( readH( proto_, 50 ) == 0x1111u );
        BOOST_TEST( readH( proto_, 51 ) == 0x2222u );
    }

    BOOST_AUTO_TEST_CASE( WriteBeforeReadSemantics )
    {
        // Write 0xBEEF to addr 200, then read addr 200 in same call
        // Per spec, write is performed before read
        RegDataType writeVals[] = { 0xBEEF };
        RegDataType readVal = 0;
        proto_.ReadWrite4XRegisters( ctx(),
            200, 1, &readVal,
            200, 1, writeVals );
        BOOST_TEST( readVal == 0xBEEFu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeReadThrows )
    {
        RegDataType writeVals[] = { 0x0001 };
        RegDataType readVals[2] = { 0, 0 };
        BOOST_CHECK_THROW(
            proto_.ReadWrite4XRegisters( ctx(),
                255, 2, readVals,
                0, 1, writeVals ),
            EBaseException );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeWriteThrows )
    {
        RegDataType writeVals[] = { 0x0001, 0x0002 };
        RegDataType readVal = 0;
        BOOST_CHECK_THROW(
            proto_.ReadWrite4XRegisters( ctx(),
                0, 1, &readVal,
                255, 2, writeVals ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE( FC01_FC02_DummyEndpoint )

    BOOST_AUTO_TEST_CASE( DummyReadCoilsLeavesBufferUnchanged )
    {
        DummyProtocol proto;
        SessionManager session( proto );

        CoilDataType v[2] = { 0x5Au, 0xA5u };
        proto.ReadCoilStatus( Context( 1 ), 0, 16, v );
        BOOST_TEST( v[0] == 0x5Au );
        BOOST_TEST( v[1] == 0xA5u );
    }

    BOOST_AUTO_TEST_CASE( DummyReadInputsLeavesBufferUnchanged )
    {
        DummyProtocol proto;
        SessionManager session( proto );

        CoilDataType v[2] = { 0x33u, 0xCCu };
        proto.ReadInputStatus( Context( 1 ), 0, 16, v );
        BOOST_TEST( v[0] == 0x33u );
        BOOST_TEST( v[1] == 0xCCu );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE( FC01_FC02_RTUEndpoint )

    BOOST_AUTO_TEST_CASE( ReadCoilsWithoutOpenPortThrows )
    {
        RTUProtocol proto;
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto.ReadCoilStatus( Context( 1 ), 0, 1, v ),
            Exception );
    }

    BOOST_AUTO_TEST_CASE( ReadInputsWithoutOpenPortThrows )
    {
        RTUProtocol proto;
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto.ReadInputStatus( Context( 1 ), 0, 1, v ),
            Exception );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( TransactionId, ProtoFixture )

    BOOST_AUTO_TEST_CASE( LowTidEchoed )
    {
        RegDataType v = 0;
        // base class throws if the server does not echo the TID correctly
        BOOST_CHECK_NO_THROW(
            proto_.ReadHoldingRegisters( TCPIPContext( 1, 0x0001 ), 0, 1, &v ) );
    }

    BOOST_AUTO_TEST_CASE( MaxTidEchoed )
    {
        RegDataType v = 0;
        BOOST_CHECK_NO_THROW(
            proto_.ReadHoldingRegisters( TCPIPContext( 1, 0xFFFF ), 0, 1, &v ) );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------
// Entry point bridge for Unicode startup.
// When _TCHAR maps to wchar_t, startup calls wmain; Boost.Test provides main.
//---------------------------------------------------------------------------

int _tmain( int argc, _TCHAR* argv[] )
{
    (void)argc;
    (void)argv;
    extern int main( int, char** );
    return main( __argc, __argv );
}
