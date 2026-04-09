/**
 * @file ModbusTCP_WinSock.h
 * @brief Modbus::Master::TCPProtocolWinSock — WinSock2 TCP transport for Modbus TCP.
 *
 * @details Provides a complete Modbus TCP master implementation built on top of
 *  the Windows WinSock2 API (ws2_32.lib).  Key characteristics:
 *  - WSAStartup / WSACleanup called in constructor / destructor.
 *  - Hostname resolution via GetAddrInfoW() (supports Unicode hostnames).
 *  - Non-blocking connect with a 5-second timeout using select().
 *  - SO_RCVTIMEO set to 2 seconds for receive operations.
 */

//---------------------------------------------------------------------------

#ifndef ModbusTCP_WinSockH
#define ModbusTCP_WinSockH

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>

#include "ModbusTCP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief WinSock2 TCP implementation of the Modbus master protocol.
 *
 * @details Concrete NVI implementation using Windows WinSock2 API for TCP I/O.
 *  Opens a TCP connection to the Modbus server, sends MBAP-framed requests, and receives responses.
 *
 *  **NVI Pattern:** Implements all protected Do…() virtual hooks defined in TCPIPProtocol
 *  (DoOpen, DoClose, DoIsConnected, DoWrite, DoRead, DoGetHost, DoSetHost, DoGetPort, DoSetPort).
 *  The public API methods are inherited non-virtually from Protocol.
 *
 *  Connection & Initialization:
 *  - WSAStartup is called in the constructor; WSACleanup in the destructor.
 *  - GetAddrInfoW() is used for name resolution (Unicode-safe).
 *  - connect() is performed in non-blocking mode with a 5-second timeout via select().
 *  - SO_RCVTIMEO is set to 2 seconds; a timeout causes an EBaseException to be thrown.
 *
 *  @note This class is Windows-only and requires linking against ws2_32.lib.
 */
class TCPProtocolWinSock : public TCPProtocol {
public:
    /**
     * @brief Constructs the protocol object and initialises WinSock2.
     * @param Host Server hostname or IP address (default: "localhost").
     * @param Port Server TCP port (default: 502).
     * @throws EBaseException if WSAStartup fails.
     */
    TCPProtocolWinSock( String Host = String( DEFAULT_MODBUS_TCPIP_HOST ),
                        uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );

    /** @brief Destructor; closes the socket and calls WSACleanup. */
    ~TCPProtocolWinSock();
protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus TCP (WinSock)" ); }
    virtual String DoGetHost() const override;
    virtual void DoSetHost( String Val ) override;
    virtual uint16_t DoGetPort() const noexcept override;
    virtual void DoSetPort( uint16_t Val ) override;
    virtual void DoOpen() override;
    virtual void DoClose() override;
    virtual bool DoIsConnected() const noexcept override;
    virtual void DoInputBufferClear() override;
    virtual void DoWrite( TBytes const OutBuffer ) override;
    virtual void DoRead( TBytes & InBuffer, size_t Length ) override;
private:
    String   host_;
    uint16_t port_;
    SOCKET   socket_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
