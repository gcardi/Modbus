/**
 * @file ModbusServerRTU.h
 * @brief Modbus::Server::RTUProtocol — Win32 serial RTU slave server.
 *
 * @details Provides a concrete Modbus RTU slave built on the existing TCommPort
 *  Win32 serial wrapper (CommPort.h) and the Modbus::Server::Protocol dispatch base.
 *
 *  Key characteristics:
 *  - Start(ComPort, BaudRate, SlaveAddress) opens the COM port and starts a background
 *    std::thread that reads RTU request frames, validates the CRC, dispatches to the
 *    RequestHandler via Protocol::DispatchRequest(), and writes the RTU response.
 *  - Frame boundary detection uses a two-phase read: block for the first byte with a
 *    100ms per-call timeout (to allow clean shutdown); then switch to a 20ms
 *    inter-character gap timeout to accumulate the rest of the frame.
 *  - CRC-16 (Modbus polynomial 0xA001) is computed locally.
 *  - Broadcast frames (slave address 0) are dispatched but not answered, per spec.
 *  - Stop() sets an atomic stop flag and joins the background thread.
 *
 * @note Windows-only. Requires linking CommPort.cpp.
 */

//---------------------------------------------------------------------------

#ifndef ModbusServerRTUH
#define ModbusServerRTUH

#include <windows.h>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include "CommPort.h"
#include "ModbusServer.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------

/**
 * @brief Win32 serial Modbus RTU slave server.
 *
 * @details Concrete NVI implementation of Protocol using TCommPort for serial I/O.
 *  The server opens a COM port, accepts RTU-framed requests from any master, and
 *  dispatches each request through the RequestHandler supplied at construction.
 *
 *  **NVI Pattern:** Stop() and IsRunning() satisfy the Protocol pure-virtual contract.
 *  Start() has RTU-specific parameters (COM port name, baud rate, slave address) so
 *  it is NOT inherited from Protocol.
 *
 *  **Thread safety:** Start() and Stop() must not be called concurrently.
 *  RequestHandler methods are called from the background server thread.
 *
 * @note This class is Windows-only.
 */
class RTUProtocol : public Protocol {
public:
    /**
     * @brief Constructs the RTU slave server.
     * @param Handler Application-level FC dispatch target; must outlive this object.
     */
    explicit RTUProtocol( RequestHandler& Handler );

    /** @brief Destructor; ensures the server thread is stopped. */
    ~RTUProtocol();

    /**
     * @brief Opens the COM port and starts the request-serve thread.
     *
     * @param ComPort       COM port device name (e.g. L"COM9" or L"\\\\.\\COM9").
     * @param BaudRate      Serial baud rate (default 9600).
     * @param SlaveAddress  Modbus slave address this server responds to (1-247).
     *
     * @details Blocks until the port is successfully opened and the serve thread is
     *  running (using std::promise synchronisation), mirroring the TCP server's
     *  readiness-on-return guarantee.  Idempotent if already running.
     *
     * @throws ECommError if the COM port cannot be opened.
     */
    void Start( const String& ComPort,
                DWORD BaudRate     = 9600,
                uint8_t SlaveAddress = 1 );

    /** @brief Signals the serve thread to stop and joins it. */
    void Stop() override;

    /** @brief Returns true while the serve thread is running. */
    [[ nodiscard ]] bool IsRunning() const override { return running_.load(); }

private:
    void ServeLoop( String ComPort, DWORD BaudRate, uint8_t SlaveAddress,
                    std::promise<bool> ReadyPromise );

    std::vector<uint8_t> ReadRTUFrame( HANDLE hCom );

    static uint16_t ComputeCRC( const uint8_t* data, size_t len ) noexcept;

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
