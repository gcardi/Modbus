/**
 * @file ModbusServer.h
 * @brief Modbus::Server — abstract slave (server) protocol framework.
 *
 * @details Defines:
 *  - Modbus::Server::RequestHandler: abstract callback interface for FC dispatch.
 *    Override only the function codes your slave supports; unsupported codes
 *    automatically reply with ExceptionCode::IllegalFunction.
 *  - Modbus::Server::Protocol: abstract base owning the FC dispatch engine and
 *    RequestHandler reference. Subclasses add transport-specific framing.
 *  - Modbus::Server::TCPIPProtocol: abstract server base that reads MBAP-framed
 *    request frames, dispatches to RequestHandler, and writes MBAP-framed responses.
 *    Concrete subclasses supply the transport I/O by implementing DoStart(), DoStop(),
 *    and DoIsRunning().
 *
 * Usage pattern (NVI):
 *  1. Derive from RequestHandler and override the FC handler(s) you support.
 *  2. Instantiate a concrete Protocol subclass (e.g. TCPProtocolWinSock, RTUProtocol),
 *     passing your RequestHandler to its constructor.
 *  3. Call Start() to begin listening; call Stop() to tear down.
 */

//---------------------------------------------------------------------------

#ifndef ModbusServerH
#define ModbusServerH

#include <cstdint>
#include <optional>
#include <vector>

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------

/**
 * @brief Abstract callback interface implemented by the slave application.
 *
 * @details Each method corresponds to one Modbus function code.  The default
 *  implementation returns ExceptionCode::IllegalFunction for every FC, so you only
 *  need to override the FCs your slave device actually supports.
 *
 *  Return std::nullopt to indicate success; return an ExceptionCode to make the
 *  server send a Modbus exception response to the master.
 *
 * @note All methods are called on the server I/O thread. Protect any shared
 *  data with appropriate synchronisation in your implementation.
 */
class RequestHandler {
public:
    virtual ~RequestHandler() = default;

    /** @brief FC01 — Read Coil Status. Fill Data[(Count+7)/8] with packed coil bits (LSB-first). */
    virtual std::optional<ExceptionCode> OnReadCoilStatus(
        CoilAddrType StartAddr, CoilCountType Count, CoilDataType* Data );

    /** @brief FC02 — Read Input Status. Fill Data[(Count+7)/8] with packed input bits (LSB-first). */
    virtual std::optional<ExceptionCode> OnReadInputStatus(
        CoilAddrType StartAddr, CoilCountType Count, CoilDataType* Data );

    /** @brief FC03 — Read Holding Registers. Fill Data[Count] with register values. */
    virtual std::optional<ExceptionCode> OnReadHoldingRegisters(
        RegAddrType StartAddr, RegCountType Count, RegDataType* Data );

    /** @brief FC04 — Read Input Registers. Fill Data[Count] with register values. */
    virtual std::optional<ExceptionCode> OnReadInputRegisters(
        RegAddrType StartAddr, RegCountType Count, RegDataType* Data );

    /** @brief FC05 — Force Single Coil. Value is true (ON) or false (OFF). */
    virtual std::optional<ExceptionCode> OnForceSingleCoil(
        CoilAddrType Addr, bool Value );

    /** @brief FC06 — Preset Single Register. */
    virtual std::optional<ExceptionCode> OnPresetSingleRegister(
        RegAddrType Addr, RegDataType Value );

    /** @brief FC07 — Read Exception Status. Set Status to the 8-coil exception byte. */
    virtual std::optional<ExceptionCode> OnReadExceptionStatus(
        ExceptionStatusDataType& Status );

    /**
     * @brief FC08 — Diagnostics.
     * @param SubFunction Diagnostics sub-function code.
     * @param Data        16-bit data from the request.
     * @param[out] Reply  16-bit data to include in the response.
     */
    virtual std::optional<ExceptionCode> OnDiagnostics(
        DiagSubFnType SubFunction, RegDataType Data, RegDataType& Reply );

    /** @brief FC15 — Force Multiple Coils. Data is packed LSB-first, (Count+7)/8 bytes. */
    virtual std::optional<ExceptionCode> OnForceMultipleCoils(
        CoilAddrType StartAddr, CoilCountType Count, const CoilDataType* Data );

    /** @brief FC16 — Preset Multiple Registers. Data holds Count register values. */
    virtual std::optional<ExceptionCode> OnPresetMultipleRegisters(
        RegAddrType StartAddr, RegCountType Count, const RegDataType* Data );

    /**
     * @brief FC20 — Read General Reference.
     * @param SubRequests Array of sub-request descriptors.
     * @param SubReqCount Number of sub-requests.
     * @param[out] Data   Output buffer; caller allocates sum(SubRequests[i].RecordLength) words.
     */
    virtual std::optional<ExceptionCode> OnReadGeneralReference(
        const FileSubRequest* SubRequests, size_t SubReqCount, RegDataType* Data );

    /**
     * @brief FC21 — Write General Reference.
     * @param SubRequests Array of sub-request descriptors.
     * @param SubReqCount Number of sub-requests.
     * @param Data        Source buffer holding concatenated register values.
     */
    virtual std::optional<ExceptionCode> OnWriteGeneralReference(
        const FileSubRequest* SubRequests, size_t SubReqCount, const RegDataType* Data );

    /** @brief FC22 — Mask Write 4X Register. Result = (CurrentValue & AndMask) | (OrMask & ~AndMask). */
    virtual std::optional<ExceptionCode> OnMaskWrite4XRegister(
        RegAddrType Addr, RegDataType AndMask, RegDataType OrMask );

    /**
     * @brief FC23 — Read/Write 4X Registers.
     * @param ReadStartAddr  First holding-register address to read.
     * @param ReadCount      Number of holding registers to read.
     * @param[out] ReadData  Buffer for ReadCount register values.
     * @param WriteStartAddr First holding-register address to write.
     * @param WriteCount     Number of holding registers to write.
     * @param WriteData      Source buffer for WriteCount register values.
     */
    virtual std::optional<ExceptionCode> OnReadWrite4XRegisters(
        RegAddrType ReadStartAddr,  RegCountType ReadCount,  RegDataType* ReadData,
        RegAddrType WriteStartAddr, RegCountType WriteCount, const RegDataType* WriteData );

    /**
     * @brief FC24 — Read FIFO Queue.
     * @param Addr        FIFO pointer address.
     * @param[out] Data  Buffer to receive up to 31 FIFO register values.
     * @param[out] Count Actual number of values placed in Data.
     */
    virtual std::optional<ExceptionCode> OnReadFIFOQueue(
        FIFOAddrType Addr, RegDataType* Data, FIFOCountType& Count );
};

//---------------------------------------------------------------------------

/**
 * @brief Abstract base for all Modbus slave protocol implementations.
 *
 * @details Protocol owns the RequestHandler reference and the complete FC dispatch
 *  engine (DispatchRequest + HandleFC01–FC24).  Transport subclasses (TCPIPProtocol,
 *  RTUProtocol) call DispatchRequest() after they have decoded their framing, then
 *  wrap the returned PDU in their own framing before sending it.
 *
 *  Stop() and IsRunning() are pure virtual because their implementation depends on
 *  the transport's threading model.  Start() is NOT declared here because its
 *  parameters differ between transports (TCP takes a port number, RTU takes a COM
 *  port name + baud rate).
 */
class Protocol {
public:
    /**
     * @brief Constructs the protocol with the given request handler.
     * @param Handler Reference to the application-level FC dispatch target.
     *        The handler must outlive this protocol object.
     */
    explicit Protocol( RequestHandler& Handler );
    virtual ~Protocol() = default;

    /** @brief Stops the server and releases transport resources. */
    virtual void Stop() = 0;

    /** @brief Returns true if the server is currently running. */
    [[ nodiscard ]] virtual bool IsRunning() const = 0;

protected:
    /**
     * @brief Dispatches one PDU request to the RequestHandler and returns the response PDU.
     *
     * @details Routes to the appropriate HandleFCxx method based on @p FC, calls the
     *  corresponding RequestHandler virtual method, and encodes the success or exception
     *  response PDU.  The returned vector contains [FC byte][data...] with no framing.
     *  Transport subclasses call this after stripping their own framing, then add framing
     *  around the returned PDU before sending.
     *
     * @param FC      Modbus function code.
     * @param Data    Pointer to the PDU data bytes (after the FC byte).
     * @param DataLen Number of bytes in @p Data.
     * @return Response PDU: success data or [FC|0x80][ExceptionCode].
     */
    std::vector<uint8_t> DispatchRequest( uint8_t FC,
                                           const uint8_t* Data, int DataLen );

    static uint16_t Get16( const uint8_t* p ) noexcept;
    static void     Put16( uint8_t* p, uint16_t v ) noexcept;
    std::vector<uint8_t> MakeErrorPDU( uint8_t FC, ExceptionCode Code ) const;

private:
    RequestHandler& handler_;

    std::vector<uint8_t> HandleFC01( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC02( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC03( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC04( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC05( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC06( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC07( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC08( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC15( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC16( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC20( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC21( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC22( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC23( const uint8_t* d, int len );
    std::vector<uint8_t> HandleFC24( const uint8_t* d, int len );
};

//---------------------------------------------------------------------------

/**
 * @brief Abstract Modbus server protocol implementing MBAP framing over a transport.
 *
 * @details TCPIPProtocol extends Protocol with MBAP frame parsing and assembly:
 *  - ParseFrame() parses incoming MBAP-framed request frames.
 *  - It calls Protocol::DispatchRequest() to obtain the response PDU.
 *  - It wraps the PDU in an MBAP response header.
 *
 *  Concrete subclasses (e.g., TCPProtocolWinSock) supply transport I/O by implementing
 *  DoStart(), DoStop(), and DoIsRunning().  The main server loop in the subclass reads
 *  full MBAP request frames and calls ProcessFrame() to obtain the corresponding
 *  response frame to send.
 *
 *  **NVI Pattern:** Start()/Stop()/IsRunning() are non-virtual; they delegate to
 *  DoStart()/DoStop()/DoIsRunning() respectively.
 */
class TCPIPProtocol : public Protocol {
public:
    /**
     * @brief Constructs the protocol with the given request handler.
     * @param Handler Reference to the application-level FC dispatch target.
     *        The handler must outlive this protocol object.
     */
    explicit TCPIPProtocol( RequestHandler& Handler );
    virtual ~TCPIPProtocol();

    /** @brief Starts listening on @p Port. Idempotent if already running. */
    void Start( uint16_t Port = 502 );

    /** @brief Stops the server and releases resources. Idempotent if already stopped. */
    void Stop() override;

    /** @brief Returns true if the server is currently running. */
    [[ nodiscard ]] bool IsRunning() const override { return DoIsRunning(); }

protected:
    /**
     * @brief Processes one complete incoming MBAP request frame and returns the response frame.
     *
     * @details Parses the MBAP header, dispatches to the RequestHandler via
     *  Protocol::DispatchRequest(), encodes the response PDU, and wraps it in an MBAP
     *  response header.  Call this from the concrete subclass's per-connection serve loop
     *  for each received frame.
     *
     * @param InFrame Byte buffer containing the complete 7-byte MBAP header followed
     *                by the Unit Identifier and PDU (i.e., exactly as received off the wire).
     * @return Complete MBAP response frame ready to send, or empty on malformed input.
     */
    std::vector<uint8_t> ProcessFrame( const std::vector<uint8_t>& InFrame );

    /** @brief Starts the server, binding to @p Port. Called by Start(). */
    virtual void DoStart( uint16_t Port ) = 0;

    /** @brief Stops the server. Called by Stop(). */
    virtual void DoStop() = 0;

    /** @brief Returns true if the server is currently listening. */
    virtual bool DoIsRunning() const = 0;

private:
    std::vector<uint8_t> WrapInMBAP( uint16_t Tid, uint8_t UnitId,
                                      const std::vector<uint8_t>& PDU ) const;
};

//---------------------------------------------------------------------------
}; // End of namespace Server
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
