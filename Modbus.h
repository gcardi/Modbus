/**
 * @file Modbus.h
 * @brief Core Modbus definitions: enumerations, exception hierarchy, Context, and Protocol base class.
 *
 * @details This header defines the fundamental building blocks of the Modbus Master Library:
 *  - FunctionCode and ExceptionCode enumerations (standard Modbus protocol codes).
 *  - Context: identifies a transaction target (slave address + optional transaction ID).
 *  - Exception classes: EBaseException, EContextException, EProtocolException,
 *    EProtocolStdException and its typed aliases (EIllegalFunction, EIllegalDataAddress, etc.).
 *  - Master::Protocol: abstract base class for all Modbus master transport implementations.
 *  - Master::SessionManager: RAII helper that calls Protocol::Open() on construction
 *    and Protocol::Close() on destruction.
 */

//---------------------------------------------------------------------------

#ifndef ModbusH
#define ModbusH

#include <System.SysUtils.hpp>

#include <cstdint>

#include <string>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------

/** @brief Dumps a byte array to the debugger output as a hex string (debug helper). */
extern void DebugBytesToHex( String Prologue, TBytes Data );

/**
 * @brief Standard Modbus function codes as defined by the Modbus specification.
 *
 * @details Only a subset of these function codes is implemented by this library.
 *  Unsupported codes are listed for completeness and possible future extension.
 */
enum class FunctionCode {
    ReadCoilStatus = 1,
    ReadInputStatus = 2,
    ReadHoldingRegisters = 3,
    ReadInputRegisters = 4,
    ForceSingleCoil = 5,
    PresetSingleRegister = 6,
    ReadExceptionStatus = 7,
    Diagnostics = 8,
    Program484 = 9,
    Poll484 = 10,
    FetchCommEventCtr = 11,
    FetchCommEventLog = 12,
    ProgramController = 13,
    PollController = 14,
    ForceMultipleCoils = 15,
    PresetMultipleRegisters = 16,
    ReportSlave = 17,
    Program884_M84 = 18,
    ResetCommLink = 19,
    ReadGeneralReference = 20,
    WriteGeneralReference = 21,
    MaskWrite4XRegister = 22,
    ReadWrite4XRegisters = 23,
    ReadFIFOQueue = 24
};

/**
 * @brief Standard Modbus exception codes returned by a slave when a request cannot be fulfilled.
 *
 * @details Each enumerator maps directly to the numeric code transmitted in a Modbus
 *  exception response frame (function code | 0x80).
 */
enum class ExceptionCode {
   IllegalFunction = 1,      // The function code received in the query
                             // is not an allowable action for the slave.
                             // If a Poll Program Complete command
                             // was issued, this code indicates that no
                             // program function preceded it.

   IllegalDataAddress = 2,   // The data address received in the query
                             // is not an allowable address for the
                             // slave.

   IllegalDataValue = 3,     // A value contained in the query data
                             // field is not an allowable value for the
                             // slave.

   SlaveDeviceFailure = 4,   // An unrecoverable error occurred while
                             // the slave was attempting to perform the
                             // requested action.

   Acknowledge = 5,          // The slave has accepted the request
                             // and is processing it, but a long duration
                             // of time will be required to do so. This
                             // response is returned to prevent a
                             // timeout error from occurring in the
                             // master. The master can next issue a
                             // Poll Program Complete message to
                             // determine if processing is completed.

   SlaveDeviceBusy = 6,      // The slave is engaged in processing a
                             // long�duration program command. The
                             // master should retransmit the message
                             // later when the slave is free.

   NegativeAcknowledge = 7,  // The slave cannot perform the program
                             // function received in the query. This
                             // code is returned for an unsuccessful
                             // programming request using function
                             // code 13 or 14 decimal. The master
                             // should request diagnostic or error
                             // information from the slave.

   MemoryParityError = 8,    // The slave attempted to read extended
                             // memory, but detected a parity error in
                             // the memory. The master can retry the
                             // request, but service may be required on
                             // the slave device.
};

//---------------------------------------------------------------------------

/**
 * @brief Identifies the target of a Modbus transaction.
 *
 * @details A Context carries at minimum the slave address (unit identifier).
 *  Subclasses such as TCPIPContext also carry a MBAP transaction identifier used
 *  when matching TCP/UDP responses to their originating requests.
 *
 *  The base implementation returns a zero transaction identifier; override
 *  DoGetTransactionIdentifier() in a subclass when the transport needs it.
 */
class Context {
public:
    using SlaveAddrType = uint8_t;       ///< Type used for the Modbus slave (unit) address.
    using TransactionIdType = uint16_t;  ///< Type used for the MBAP transaction identifier.

    /**
     * @brief Constructs a Context with the given slave address.
     * @param SlaveAddr Modbus slave (unit) address (1–247 for RTU; 0–255 for TCP/UDP).
     */
    explicit Context( SlaveAddrType SlaveAddr ) : slaveAddr_( SlaveAddr ) {}
    virtual ~Context() = default;

    /** @brief Returns the slave (unit) address. */
    [[ nodiscard ]] SlaveAddrType GetSlaveAddr() const noexcept { return DoGetSlaveAddr(); }

    /** @brief Returns the transaction identifier (0 for RTU; meaningful for TCP/UDP). */
    [[ nodiscard ]] TransactionIdType GetTransactionIdentifier() const noexcept {
        return DoGetTransactionIdentifier();
    }
protected:
    /** @brief Virtual implementation hook for GetSlaveAddr(). */
    virtual SlaveAddrType DoGetSlaveAddr() const noexcept { return slaveAddr_; }
    /** @brief Virtual implementation hook for GetTransactionIdentifier(). Returns 0 by default. */
    virtual TransactionIdType DoGetTransactionIdentifier() const noexcept {
        return TransactionIdType();
    }
private:
    SlaveAddrType slaveAddr_;
};
//---------------------------------------------------------------------------

/**
 * @brief Base exception class for all Modbus library exceptions.
 *
 * @details Derives from Embarcadero/VCL @c Exception.  All exceptions thrown by
 *  the library are instances of EBaseException or one of its subclasses.
 */
class EBaseException : public Exception {
public:
    /** @brief Constructs with a plain message string. */
    explicit EBaseException( String Message )
      : Exception(
            _D( "Modbus exception %s" )
          , ARRAYOFCONST( ( Message ) )
        ) {}
	EBaseException( String Msg, TVarRec *Args, int Args_High )
      : Exception(
            _D( "Modbus exception %s" )
          , ARRAYOFCONST( ( Format( Msg, Args, Args_High ) ) )
        ) {}
};
//---------------------------------------------------------------------------

/**
 * @brief Exception that also carries the Modbus Context in which it was raised.
 *
 * @details Use GetContext() to retrieve the slave address and transaction ID that
 *  were active when the error occurred.
 */
class EContextException : public EBaseException {
public:
    /** @brief Constructs with a Context and a plain message. */
    explicit EContextException( Context const & Context, String Message )
       : EBaseException( Message )
       , context_( Context ) {}
    EContextException( Context const & Context, String Msg, TVarRec *Args,
                       int Args_High )
       : EBaseException( Msg, Args, Args_High )
       , context_( Context ) {}
    [[ nodiscard ]] Context const & GetContext() const noexcept { return context_; }
public:
    Context context_;
};
//---------------------------------------------------------------------------

/**
 * @brief Exception representing a well-formed Modbus exception response from a slave.
 *
 * @details The slave returned an exception response frame (function code | 0x80).
 *  GetCode() provides the ExceptionCode that the slave reported.
 */
class EProtocolException : public EContextException
{
public:
    /** @brief Constructs with Context, the Modbus ExceptionCode, and a message string. */
    EProtocolException( Context const & Context, ExceptionCode Code, String Message )
       : EContextException( Context, Message )
       , code_( Code ) {}
    EProtocolException( Context const & Context, ExceptionCode Code, String Msg,
                        TVarRec *Args, int Args_High )
       : EContextException( Context, Msg, Args, Args_High )
       , code_( Code ) {}
    [[ nodiscard ]] ExceptionCode GetCode() const noexcept { return code_; }
private:
    ExceptionCode code_;
};
//---------------------------------------------------------------------------

[[ nodiscard ]] extern String GetExceptionCodeText( ExceptionCode Code );
[[ nodiscard ]] extern String GetExceptionCodeDescription( ExceptionCode Code );

#if !defined(__BORLANDC__) || defined(__clang__) // BCC32C, BCC64
[[ noreturn ]] extern void RaiseFunctionCodeNotImplementedException( FunctionCode Code );
[[ noreturn ]] extern void RaiseStandardException( Context const & Context,
                                                   ExceptionCode Code,
                                                   String Prefix = String() );
  #else // BCC32
extern void RaiseFunctionCodeNotImplementedException( FunctionCode Code ) [[noreturn]];
extern void RaiseStandardException( Context const & Context,
                                    ExceptionCode Code,
                                    String Prefix = String() ) [[noreturn]];
  #endif
//#endif

//---------------------------------------------------------------------------

/**
 * @brief Typed Modbus protocol exception parameterised on a specific ExceptionCode.
 *
 * @details This template is instantiated once for each standard Modbus exception code and
 *  then aliased to a named exception type (EIllegalFunction, EIllegalDataAddress, etc.).
 *  This allows catch blocks to distinguish exception codes without inspecting GetCode().
 *
 * @tparam Code The specific Modbus ExceptionCode this exception represents.
 */
template<ExceptionCode Code>
class EProtocolStdException : public EProtocolException
{
public:
    /**
     * @brief Constructs the exception with an optional descriptive prefix.
     * @param Context  The active Modbus context when the error occurred.
     * @param Prefix   Optional prefix prepended to the standard code description text.
     */
    EProtocolStdException( Context const & Context, String Prefix = String() )
        : EProtocolException(
            Context,
            Code,
            Prefix.IsEmpty() ?
              GetExceptionCodeText( Code )
            :
              Format( _D( "%s: %s" ), ARRAYOFCONST( ( Prefix, GetExceptionCodeText( Code ) ) ) )
          )
    {}
};

/** @brief Thrown when the slave reports ExceptionCode::IllegalFunction (FC not allowed). */
using
  EIllegalFunction =
    EProtocolStdException<ExceptionCode::IllegalFunction>;

/** @brief Thrown when the slave reports ExceptionCode::IllegalDataAddress (address out of range). */
using
  EIllegalDataAddress =
    EProtocolStdException<ExceptionCode::IllegalDataAddress>;

/** @brief Thrown when the slave reports ExceptionCode::IllegalDataValue (value not allowed). */
using
  EIllegalDataValue =
    EProtocolStdException<ExceptionCode::IllegalDataValue>;

/** @brief Thrown when the slave reports ExceptionCode::SlaveDeviceFailure (unrecoverable error). */
using
  ESlaveDeviceFailure =
    EProtocolStdException<ExceptionCode::SlaveDeviceFailure>;

/** @brief Thrown when the slave returns ExceptionCode::Acknowledge (long-duration command accepted). */
using
  EAcknowledge =
    EProtocolStdException<ExceptionCode::Acknowledge>;

/** @brief Thrown when the slave reports ExceptionCode::SlaveDeviceBusy (busy processing). */
using
  ESlaveDeviceBusy =
    EProtocolStdException<ExceptionCode::SlaveDeviceBusy>;

/** @brief Thrown when the slave reports ExceptionCode::NegativeAcknowledge (cannot perform program function). */
using
  ENegativeAcknowledge =
    EProtocolStdException<ExceptionCode::NegativeAcknowledge>;

/** @brief Thrown when the slave reports ExceptionCode::MemoryParityError (extended memory parity error). */
using
  EMemoryParityError =
    EProtocolStdException<ExceptionCode::MemoryParityError>;

using CoilAddrType  = uint16_t;  ///< Type for a coil (discrete output) address.
using CoilCountType = uint16_t;  ///< Type for the number of coils in a request.
using CoilDataType  = uint8_t;   ///< Type for packed coil data bytes.

using RegAddrType  = uint16_t;   ///< Type for a holding/input register address.
using RegCountType = uint16_t;   ///< Type for the number of registers in a request.
using RegDataType  = uint16_t;   ///< Type for a single register value (16-bit word).

using ExceptionStatusDataType = uint8_t;  ///< Type for the FC07 exception status byte (8 coil values).
using DiagSubFnType = uint16_t;           ///< Type for a Diagnostics (FC08) sub-function code.
using FIFOAddrType  = uint16_t;           ///< Type for the FIFO pointer register address (FC24).
using FIFOCountType = uint16_t;           ///< Type for the number of FIFO register values (FC24).

/**
 * @brief Standard Diagnostics sub-function codes for FC08.
 *
 * @details Only a subset of commonly used sub-functions is listed here.
 *  The full list is defined in the Modbus specification.
 */
enum class DiagnosticsSubFunction : DiagSubFnType {
    ReturnQueryData            = 0x0000,  ///< Echo test — slave echoes request data.
    RestartCommunicationsOption = 0x0001, ///< Restart communications, optionally clear log.
    ReturnDiagnosticRegister   = 0x0002,  ///< Return the slave's 16-bit diagnostic register.
    ChangeAsciiInputDelimiter  = 0x0003,  ///< Change the ASCII mode input delimiter.
    ForceListenOnlyMode        = 0x0004,  ///< Force the slave into listen-only mode.
    ClearCountersAndDiagnosticRegister = 0x000A, ///< Clear all counters and the diagnostic register.
    ReturnBusMessageCount      = 0x000B,  ///< Return total bus message count.
    ReturnBusCommunicationErrorCount = 0x000C, ///< Return CRC error count.
    ReturnBusExceptionErrorCount = 0x000D, ///< Return slave exception response count.
    ReturnSlaveMessageCount    = 0x000E,  ///< Return messages addressed to this slave.
    ReturnSlaveNoResponseCount = 0x000F,  ///< Return no-response count for this slave.
    ReturnSlaveNAKCount        = 0x0010,  ///< Return NAK exception count.
    ReturnSlaveBusyCount       = 0x0011,  ///< Return slave busy exception count.
    ReturnBusCharacterOverrunCount = 0x0012, ///< Return character overrun count.
    ClearOverrunCounterAndFlag = 0x0014,  ///< Clear the overrun counter and error flag.
};

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief Abstract base class for all Modbus master transport implementations.
 *
    * @details Protocol implements the **Non-Virtual Interface (NVI)** pattern to decouple the public
    *  API from transport-specific implementations. Concrete subclasses supply the transport logic
    *  (TCP, UDP, RTU, or Dummy) by implementing the @c Do…() virtual methods.
 *
    *  **NVI Pattern Overview:**
    *  - All public methods (Open, Close, ReadCoilStatus, ReadInputStatus,
    *    ReadHoldingRegisters, ReadInputRegisters, ForceSingleCoil,
    *    PresetSingleRegister, ForceMultipleCoils, PresetMultipleRegisters,
    *    MaskWrite4XRegister, ReadWrite4XRegisters, etc.)
    *    are concrete and non-virtual; they perform input validation and orchestration.
    *  - Each public method forwards to a corresponding protected @c Do…() virtual method that
    *    performs the actual transport-specific work (e.g., DoOpen(), DoReadHoldingRegisters()).
    *  - Subclasses override only the @c Do…() virtual methods; they never override the public API.
    *  - This ensures consistent contract enforcement and logging/validation across all transports.
 *
    *  **Virtual Method Naming Convention:**
    *  - Virtual hooks use the @c "Do" prefix: DoOpen, DoClose, DoReadHoldingRegisters, etc.
    *  - These methods are declared in the protected section and marked @c virtual.
    *  - Subclasses use @c override keyword when implementing these hooks.
    *
    *  **Idempotency:**
    *  - Open() and Close() are idempotent: calling Open() when already connected, or
    *    Close() when already disconnected, is a safe no-op.
 */
class Protocol {
public:
    Protocol();
    virtual ~Protocol();

    /**
     * @brief Opens the communication channel (e.g., connects TCP socket, opens COM port).
     * @details Idempotent — has no effect if the protocol is already connected.
     * @throws EBaseException on failure to establish the connection.
        *
        * @note This is a non-virtual public method implementing the NVI pattern. It calls
        *  the protected virtual DoOpen() method, allowing subclasses to inject transport-specific
        *  logic without overriding the public API.
     */
    void Open();

    /**
     * @brief Closes the communication channel.
     * @details Idempotent — has no effect if the protocol is already disconnected.
     */
    void Close();

    /** @brief Returns @c true if the communication channel is currently open/connected. */
    [[ nodiscard ]] bool IsConnected() const { return DoIsConnected(); }

    /** @brief Returns a human-readable protocol name (e.g., "Modbus TCP (WinSock)"). */
    [[ nodiscard ]] String GetProtocolName() const { return DoGetProtocolName(); }

    /** @brief Returns a human-readable string describing the protocol parameters (host:port, COM port settings, etc.). */
    [[ nodiscard ]] String GetProtocolParamsStr() const { return DoGetProtocolParamsStr(); }

    /**
     * @brief Reads one or more coil statuses (FC01).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start coil address.
     * @param PointCount Number of coils to read (max 2000 by Modbus spec).
     * @param[out] Data  Pointer to packed coil bytes; required size is
     *                   @c (PointCount + 7) / 8 bytes.
     *
     * @details Coil values are bit-packed in LSB-first order per Modbus spec.
     *  Bit 0 of @c Data[0] corresponds to @p StartAddr.
     *
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ReadCoilStatus( Context const & Context,
                         CoilAddrType StartAddr, CoilCountType PointCount,
                         CoilDataType* Data )
    {
        DoReadCoilStatus( Context, StartAddr, PointCount, Data );
    }

    /**
     * @brief Reads one or more discrete input statuses (FC02).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start input address.
     * @param PointCount Number of discrete inputs to read (max 2000 by Modbus spec).
     * @param[out] Data  Pointer to packed input bytes; required size is
     *                   @c (PointCount + 7) / 8 bytes.
     *
     * @details Input values are bit-packed in LSB-first order per Modbus spec.
     *  Bit 0 of @c Data[0] corresponds to @p StartAddr.
     *
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ReadInputStatus( Context const & Context,
                          CoilAddrType StartAddr, CoilCountType PointCount,
                          CoilDataType* Data )
    {
        DoReadInputStatus( Context, StartAddr, PointCount, Data );
    }

    /**
     * @brief Reads one or more holding registers (FC03).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start register address.
     * @param PointCount Number of registers to read (max 125 for TCP/UDP, hardware-limited for RTU).
     * @param[out] Data  Pointer to output buffer; must hold at least @p PointCount elements.
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
        *
        * @note Uses the NVI pattern: this public method delegates to DoReadHoldingRegisters().
     */
    void ReadHoldingRegisters( Context const & Context,
                               RegAddrType StartAddr, RegCountType PointCount,
                               RegDataType* Data )
    {
        DoReadHoldingRegisters( Context, StartAddr, PointCount, Data );
    }

    /**
     * @brief Reads one or more input registers (FC04).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start register address.
     * @param PointCount Number of input registers to read.
     * @param[out] Data  Pointer to output buffer; must hold at least @p PointCount elements.
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ReadInputRegisters( Context const & Context,
                             RegAddrType StartAddr, RegCountType PointCount,
                             RegDataType* Data )
    {
        DoReadInputRegisters( Context, StartAddr, PointCount, Data );
    }

    /**
     * @brief Forces a single coil to ON or OFF (FC05).
     * @param Context Transaction context (slave address, transaction ID).
     * @param Addr    Zero-based coil address.
     * @param Value   @c true to force ON (0xFF00), @c false to force OFF (0x0000).
     * @throws EIllegalDataAddress if the address is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ForceSingleCoil( Context const & Context,
                          CoilAddrType Addr, bool Value )
    {
        DoForceSingleCoil( Context, Addr, Value );
    }

    /**
     * @brief Writes a single holding register (FC06).
     * @param Context Transaction context (slave address, transaction ID).
     * @param Addr    Zero-based register address.
     * @param Data    16-bit value to write.
     * @throws EIllegalDataAddress if the address is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void PresetSingleRegister( Context const & Context,
                               RegAddrType Addr, RegDataType Data )
    {
        DoPresetSingleRegister( Context, Addr, Data );
    }
    /**
     * @brief Reads the eight exception status coils from the slave (FC07).
     * @param Context Transaction context (slave address, transaction ID).
     * @return A single byte whose bits represent the eight exception status outputs.
     *
     * @details This is a simple, fast diagnostic read.  The meaning of each bit
     *  is device-specific.
     *
     * @throws EIllegalFunction if the slave does not support FC07.
     * @throws EBaseException on communication error or timeout.
     */
    [[ nodiscard ]]
    ExceptionStatusDataType ReadExceptionStatus( Context const & Context )
    {
        return DoReadExceptionStatus( Context );
    }

    /**
     * @brief Sends a Diagnostics request to the slave (FC08).
     * @param Context     Transaction context (slave address, transaction ID).
     * @param SubFunction Diagnostics sub-function code (see DiagnosticsSubFunction enum).
     * @param Data        16-bit data field sent with the request.
     * @return The 16-bit data field returned by the slave.
     *
     * @details For sub-function 0x0000 (Return Query Data / echo test), the slave
     *  echoes back the same data value.  Other sub-functions may return counters,
     *  status, or zero depending on the sub-function semantics.
     *
     * @throws EIllegalFunction if the slave does not support FC08 or the sub-function.
     * @throws EIllegalDataValue if the data value is not accepted by the sub-function.
     * @throws EBaseException on communication error or timeout.
     */
    [[ nodiscard ]]
    RegDataType Diagnostics( Context const & Context,
                             DiagSubFnType SubFunction, RegDataType Data )
    {
        return DoDiagnostics( Context, SubFunction, Data );
    }

//    Program484
//    Poll484
//    FetchCommEventCtr
//    FetchCommEventLog
//    ProgramController
//    PollController

    /**
     * @brief Forces multiple consecutive coils (FC15).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start coil address.
     * @param PointCount Number of coils to force (max 1968 by Modbus spec).
     * @param Data       Pointer to packed coil bytes; bit 0 of @c Data[0] maps to @p StartAddr.
     *                   Required size is @c (PointCount + 7) / 8 bytes.
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ForceMultipleCoils( Context const & Context,
                             CoilAddrType StartAddr, CoilCountType PointCount,
                             const CoilDataType* Data )
    {
        DoForceMultipleCoils( Context, StartAddr, PointCount, Data );
    }

    /**
     * @brief Writes multiple consecutive holding registers (FC16).
     * @param Context    Transaction context (slave address, transaction ID).
     * @param StartAddr  Zero-based start register address.
     * @param PointCount Number of registers to write.
     * @param Data       Pointer to source buffer holding at least @p PointCount elements.
     * @throws EIllegalDataAddress if the address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void PresetMultipleRegisters( Context const & Context,
                                  RegAddrType StartAddr, RegCountType PointCount,
                                  const RegDataType* Data )
    {
        DoPresetMultipleRegisters( Context, StartAddr, PointCount, Data );
    }
//    ReportSlave
//    Program884_M84
//    ResetCommLink
//    ReadGeneralReference
//    WriteGeneralReference

    /**
     * @brief Performs a masked write on a single holding register (FC22).
     * @details The slave computes: Result = (CurrentValue AND AndMask) OR (OrMask AND NOT AndMask).
     * @param Context Transaction context (slave address, transaction ID).
     * @param Addr    Zero-based register address.
     * @param AndMask AND mask to apply to the current register value.
     * @param OrMask  OR mask to apply after the AND operation.
     * @throws EIllegalDataAddress if the address is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void MaskWrite4XRegister( Context const & Context,
                              RegAddrType Addr, RegDataType AndMask,
                              RegDataType OrMask )
    {
        DoMaskWrite4XRegister( Context, Addr, AndMask, OrMask );
    }

    /**
     * @brief Atomically reads and writes multiple holding registers (FC23).
     * @param Context         Transaction context (slave address, transaction ID).
     * @param ReadStartAddr   Zero-based start address for reading.
     * @param ReadPointCount  Number of registers to read (max 125).
     * @param[out] ReadData   Pointer to output buffer; must hold at least @p ReadPointCount elements.
     * @param WriteStartAddr  Zero-based start address for writing.
     * @param WritePointCount Number of registers to write (max 121).
     * @param WriteData       Pointer to source buffer holding at least @p WritePointCount elements.
     * @throws EIllegalDataAddress if any address or count is out of range on the slave.
     * @throws EBaseException on communication error or timeout.
     */
    void ReadWrite4XRegisters( Context const & Context,
                               RegAddrType ReadStartAddr,
                               RegCountType ReadPointCount,
                               RegDataType* ReadData,
                               RegAddrType WriteStartAddr,
                               RegCountType WritePointCount,
                               const RegDataType* WriteData )
    {
        DoReadWrite4XRegisters( Context, ReadStartAddr, ReadPointCount, ReadData,
                                WriteStartAddr, WritePointCount, WriteData );
    }
    /**
     * @brief Reads the contents of a FIFO queue of registers from the slave (FC24).
     * @param Context       Transaction context (slave address, transaction ID).
     * @param FIFOAddr      Address of the FIFO pointer register on the slave.
     * @param[out] Data     Pointer to output buffer; must hold at least 31 elements
     *                      (the Modbus-defined maximum FIFO depth).
     * @return The number of register values actually read (0–31).
     *
     * @details The slave returns the current FIFO count followed by that many 16-bit
     *  register values.  If the FIFO is empty the return value is 0 and Data is untouched.
     *
     * @throws EIllegalDataAddress if the FIFO pointer address is invalid on the slave.
     * @throws EIllegalDataValue if the FIFO contains more than 31 values.
     * @throws EIllegalFunction if the slave does not support FC24.
     * @throws EBaseException on communication error or timeout.
     */
    [[ nodiscard ]]
    FIFOCountType ReadFIFOQueue( Context const & Context,
                                 FIFOAddrType FIFOAddr,
                                 RegDataType* Data )
    {
        return DoReadFIFOQueue( Context, FIFOAddr, Data );
    }

protected:
    virtual String DoGetProtocolName() const = 0;
    virtual String DoGetProtocolParamsStr() const = 0;
        /**
         * @brief Pure virtual hook for opening the communication channel (NVI implementation hook).
         *
         * @details Subclasses override this to provide transport-specific open logic.
         *  The public non-virtual Open() method calls this; subclasses must implement DoOpen(),
         *  not override Open().  Only implement connection logic here; validation and retry logic
         *  are handled by the public interface.
         *
         * @throws EBaseException on failure to open the channel.
         */
    virtual void DoOpen() = 0;
        /**
         * @brief Pure virtual hook for closing the communication channel (NVI implementation hook).
         *
         * @details Subclasses override this to provide transport-specific close logic.
         *  The public non-virtual Close() method calls this; subclasses must implement DoClose(),
         *  not override Close().  This should cleanly release all transport resources.
         */
    virtual void DoClose() = 0;
        /**
         * @brief Pure virtual hook for checking connection state (NVI implementation hook).
         *
         * @details Subclasses override this to return the transport-specific connection status.
         *  The public non-virtual IsConnected() method calls this; subclasses must implement
         *  DoIsConnected(), not override IsConnected().
         *
         * @return true if the channel is open/connected; false otherwise.
         */
    virtual bool DoIsConnected() const = 0;
    /**
     * @brief Pure virtual hook for reading holding registers (FC03).
     *
     * @details This is the NVI pattern implementation hook. Subclasses override this method
     *  to provide transport-specific logic for FC03 read operations.  The public API method
     *  ReadHoldingRegisters() calls this; subclasses must not override ReadHoldingRegisters().
     *
     * Subclasses must implement this method to:
     *  1. Construct a Modbus request PDU (function code 03, start address, point count).
     *  2. Send it via the transport (TCP, UDP, RTU, or Dummy).
     *  3. Receive and parse the response, validating CRC/MBAP headers.
     *  4. Extract the register values and populate the Data buffer.
     */

    virtual void DoReadCoilStatus( Context const & Context,
                                   CoilAddrType StartAddr,
                                   CoilCountType PointCount,
                                   CoilDataType* Data ) = 0;
    virtual void DoReadInputStatus( Context const & Context,
                                    CoilAddrType StartAddr,
                                    CoilCountType PointCount,
                                    CoilDataType* Data ) = 0;

    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) = 0;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) = 0;

    virtual void DoForceSingleCoil( Context const & Context,
                                    CoilAddrType Addr,
                                    bool Value ) = 0;
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr,
                                         RegDataType Data ) = 0;
    virtual ExceptionStatusDataType DoReadExceptionStatus(
                                        Context const & Context ) = 0;
    virtual RegDataType DoDiagnostics( Context const & Context,
                                       DiagSubFnType SubFunction,
                                       RegDataType Data ) = 0;
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
    virtual void DoForceMultipleCoils( Context const & Context,
                                       CoilAddrType StartAddr,
                                       CoilCountType PointCount,
                                       const CoilDataType* Data ) = 0;
      virtual void DoPresetMultipleRegisters( Context const & Context,
                                              RegAddrType StartAddr,
                                              RegCountType PointCount,
                                              const RegDataType* Data ) = 0;
//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) = 0;
    virtual void DoReadWrite4XRegisters( Context const & Context,
                                         RegAddrType ReadStartAddr,
                                         RegCountType ReadPointCount,
                                         RegDataType* ReadData,
                                         RegAddrType WriteStartAddr,
                                         RegCountType WritePointCount,
                                         const RegDataType* WriteData ) = 0;
    virtual FIFOCountType DoReadFIFOQueue( Context const & Context,
                                          FIFOAddrType FIFOAddr,
                                          RegDataType* Data ) = 0;

    void RaiseExceptionIfIsConnected( String SubMsg ) const;
    void RaiseExceptionIfIsNotConnected( String SubMsg ) const;
private:
};
//---------------------------------------------------------------------------

/**
 * @brief RAII session guard for a Modbus::Master::Protocol.
 *
 * @details On construction, calls Protocol::Open().  On destruction, calls Protocol::Close().
 *  This ensures the communication channel is always properly closed even when exceptions
 *  are thrown inside the guarded scope.
 *
 *  SessionManager is non-copyable and non-movable.
 *
 * @note Because Protocol::Open() and Protocol::Close() are idempotent, nesting
 *       SessionManager instances for the same Protocol object is safe.
 */
class SessionManager  {
public:
    /**
     * @brief Constructs a SessionManager and opens the protocol.
     * @param Protocol The Modbus master protocol instance to manage.
     * @throws EBaseException if opening the protocol fails.
     */
    explicit SessionManager( Protocol& Protocol )
      : protocol_( Protocol ) { protocol_.Open(); }

    /** @brief Destroys the SessionManager and closes the protocol. */
    ~SessionManager()
    {
        try {
            protocol_.Close();
        }
        catch ( ... ) {
        }
    }

    SessionManager( SessionManager const & Rhs ) = delete;             ///< Non-copyable.
    SessionManager& operator=( SessionManager const & Rhs ) = delete;  ///< Non-copyable.
    SessionManager( SessionManager&& Rhs ) = delete;                   ///< Non-movable.
    SessionManager& operator=( SessionManager&& Rhs ) = delete;        ///< Non-movable.
private:
    Protocol& protocol_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
