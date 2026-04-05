/**
 * @file ModbusUDP_WinSock.h
 * @brief Modbus::Master::UDPProtocolWinSock — WinSock2 UDP transport for Modbus UDP.
 *
 * @details Provides a Modbus UDP master implementation built on the Windows WinSock2 API.
 *  Key characteristics:
 *  - WSAStartup / WSACleanup called in constructor / destructor.
 *  - Hostname resolution via GetAddrInfoW() (supports Unicode hostnames).
 *  - DoWrite() sends a complete MBAP datagram via sendto(), then immediately receives
 *    the server response via recvfrom() and stores it in an internal receive buffer.
 *  - DoRead() slices the requested number of bytes from the cached receive buffer.
 *  - SO_RCVTIMEO set to 2 seconds for the recvfrom() call.
 */

//---------------------------------------------------------------------------

#ifndef ModbusUDP_WinSockH
#define ModbusUDP_WinSockH

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>

#include "ModbusUDP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief WinSock2 UDP implementation of the Modbus master protocol.
 *
 * @details Uses Windows WinSock2 UDP sockets for datagram-based Modbus communication.
 *  Unlike the TCP variant, no persistent connection is maintained; each write/read cycle
 *  consists of a single sendto() immediately followed by a recvfrom() on the same socket.
 *  The received datagram is cached internally; subsequent DoRead() calls slice bytes from
 *  this cache without issuing further recvfrom() calls.
 *
 *  SO_RCVTIMEO is set to 2 seconds.  A timeout causes an EBaseException to be thrown.
 *
 *  @note This class is Windows-only and requires linking against ws2_32.lib.
 */
class UDPProtocolWinSock : public UDPProtocol {
public:
    /**
     * @brief Constructs the protocol object and initialises WinSock2.
     * @param Host Server hostname or IP address (default: "localhost").
     * @param Port Server UDP port (default: 502).
     * @throws EBaseException if WSAStartup fails.
     */
    UDPProtocolWinSock( String Host = String( DEFAULT_MODBUS_TCPIP_HOST ),
                        uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );

    /** @brief Destructor; closes the socket and calls WSACleanup. */
    ~UDPProtocolWinSock();
protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus UDP (WinSock)" ); }
    virtual String DoGetHost() const override;
    virtual void DoSetHost( String Val ) override;
    virtual uint16_t DoGetPort() const override;
    virtual void DoSetPort( uint16_t Val ) override;
    virtual void DoOpen() override;
    virtual void DoClose() override;
    virtual bool DoIsConnected() const override;
    virtual void DoInputBufferClear() override;
    virtual void DoWrite( TBytes const OutBuffer ) override;
    virtual void DoRead( TBytes & InBuffer, size_t Length ) override;
private:
    String          host_;
    uint16_t        port_;
    SOCKET          socket_;
    sockaddr_storage serverAddr_;
    int             serverAddrLen_;
    TBytes          recvBuffer_;
    int             recvBufferPos_;
    int             recvBufferSize_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#endif
