/**
 * @file ModbusServerTCP_WinSock.h
 * @brief Modbus::Server::TCPProtocolWinSock — WinSock2 TCP transport for the Modbus slave server.
 *
 * @details Provides a concrete Modbus TCP slave implementation built on top of the Windows
 *  WinSock2 API (ws2_32.lib).  Key characteristics:
 *  - WSAStartup / WSACleanup called in constructor / destructor.
 *  - Start(Port) binds a listen socket, sets SO_REUSEADDR, and starts a background
 *    std::thread that accept()-loops until Stop() is called.
 *  - One client connection is served at a time; the server loop calls ProcessFrame()
 *    for each received MBAP request and sends the resulting response frame.
 *  - Stop() sets an atomic stop flag and joins the background thread.
 *
 *  @note Windows-only; requires linking against ws2_32.lib.
 */

//---------------------------------------------------------------------------

#ifndef ModbusServerTCP_WinSockH
#define ModbusServerTCP_WinSockH

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <future>
#include <thread>

#include "ModbusServer.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------

/**
 * @brief WinSock2 TCP Modbus slave server.
 *
 * @details Concrete NVI implementation of TCPIPProtocol using Windows WinSock2.
 *  The server listens on a configurable TCP port, accepts one client connection
 *  at a time, and dispatches each MBAP-framed request through ProcessFrame().
 *
 *  **NVI Pattern:** Implements DoStart(), DoStop(), and DoIsRunning() as required
 *  by TCPIPProtocol.  The public Start()/Stop()/IsRunning() API is inherited.
 *
 *  **Thread safety:** Start() and Stop() must not be called concurrently.
 *  RequestHandler methods are called from the background server thread.
 *
 *  @note This class is Windows-only and requires linking against ws2_32.lib.
 */
class TCPProtocolWinSock : public TCPIPProtocol {
public:
    /**
     * @brief Constructs the server object and initialises WinSock2.
     * @param Handler Application-level FC dispatch target; must outlive this object.
     * @throws EBaseException if WSAStartup fails.
     */
    explicit TCPProtocolWinSock( RequestHandler& Handler );

    /** @brief Destructor; ensures the server is stopped and calls WSACleanup. */
    ~TCPProtocolWinSock();

protected:
    /**
     * @brief Binds a listen socket to @p Port and starts the accept-loop thread.
     * @throws EBaseException if socket creation, bind, or listen fails.
     */
    virtual void DoStart( uint16_t Port ) override;

    /** @brief Signals the accept-loop thread to stop and joins it. */
    virtual void DoStop() override;

    /** @brief Returns true while the accept-loop thread is running. */
    virtual bool DoIsRunning() const override;

private:
    void ServeLoop( uint16_t Port, std::promise<bool> ReadyPromise );
    bool ServeClient( SOCKET ClientSock );

    std::thread       thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> stop_{ false };
};

//---------------------------------------------------------------------------
}; // End of namespace Server
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
