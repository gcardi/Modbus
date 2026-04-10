/**
 * @file ModbusRTU.h
 * @brief ERTUParametersError and Modbus::Master::RTUProtocol — serial RS-485 RTU transport.
 *
 * @details RTUProtocol implements the Modbus RTU framing protocol over a Win32 serial (COM)
 *  port.  It uses CRC16 framing, supports configurable baud rate, parity, data bits and
 *  stop bits, and performs automatic retry on CRC errors or timeouts.
 *
 *  An optional flow-event callback (TFlowEvent) allows the caller to observe raw TX and RX
 *  frames for diagnostics or logging.
 */

//---------------------------------------------------------------------------

#ifndef ModbusRTUH
#define ModbusRTUH
//---------------------------------------------------------------------------

#include <vector>
#include <algorithm>

#include <System.DateUtils.hpp>

#include <cstdint>

#include <boost/crc.hpp>

#include "CommPort.h"
#include "Modbus.h"

/** @brief Windows FILETIME units per microsecond (100 ns = 10 ticks). */
#define FT_MICROSECOND   ( 10UI64 )
/** @brief Windows FILETIME units per millisecond. */
#define FT_MILLISECOND   ( 1000UI64 * FT_MICROSECOND )
/** @brief Windows FILETIME units per second. */
#define FT_SECOND        ( 1000UI64 * FT_MILLISECOND )
/** @brief Windows FILETIME units per minute. */
#define FT_MINUTE        ( 60UI64 * FT_SECOND )
/** @brief Windows FILETIME units per hour. */
#define FT_HOUR          ( 60UI64 * FT_MINUTE )
/** @brief Windows FILETIME units per day. */
#define FT_DAY           ( 24UI64 * FT_HOUR )

#if !defined( MODBUS_RTU_DEFAULT_RETRY_COUNT )
  /** @brief Default number of retransmission attempts for RTU transactions. Override before including this header. */
  #define  MODBUS_RTU_DEFAULT_RETRY_COUNT  3
#endif

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------

/**
 * @brief Exception thrown when invalid serial port parameters are supplied to RTUProtocol.
 *
 * @details Raised for example when an unsupported parity, stop-bit count, or baud rate
 *  is configured before opening the port.
 */
class ERTUParametersError : public EBaseException
{
public:
    /** @brief Constructs with a descriptive message. */
    ERTUParametersError( String Msg ) : EBaseException( Msg ) {}
};

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief Modbus RTU master protocol over a Win32 serial (COM) port.
 *
 * @details RTUProtocol is a concrete transport that frames Modbus requests using the RTU encoding:
 *  each PDU is preceded by the slave address byte and terminated with a two-byte CRC16
 *  (little-endian, using the standard Modbus CRC16 polynomial).  It implements all the
 *  virtual NVI hooks (DoOpen, DoClose, DoReadHoldingRegisters, etc.) defined in Protocol.
 *
 *  Features:
 *  - Configurable serial port (name, baud rate, parity, data bits, stop bits).
 *  - Automatic retry on timeout or CRC error; retry count defaults to MODBUS_RTU_DEFAULT_RETRY_COUNT.
 *  - Optional TX-echo cancellation for half-duplex RS-485 adapters that loop back transmitted bytes.
 *  - Optional TFlowEvent callback to observe raw TX/RX frames for diagnostics.
 *  - CancelTXEcho, RetryCount, and TimeoutValue exposed as C++Builder __property members.
 *
 *  **Architecture:** Inherits from Protocol and implements all protected Do…() virtual methods
 *  following the NVI pattern. Public methods are inherited from Protocol.
 *
 *  @note Open the port by calling Protocol::Open() before issuing any requests (inherited method).
 *        Close it with Protocol::Close() when done, or use a SessionManager guard.
 */
class RTUProtocol : public Protocol {
public:
    /** @brief Container type for raw RTU frame bytes. */
    using FrameCont = std::vector<uint8_t>;

    /** @brief Indicates whether a flow event relates to a transmitted or received frame. */
    enum class FlowDirection { RX, TX };

    /**
     * @brief Signature of the optional frame-flow diagnostic callback.
     * @details Called once for each transmitted frame (FlowDirection::TX) immediately
     *  before it is written to the COM port, and once for each received frame
     *  (FlowDirection::RX) immediately after it passes CRC validation.
     */
    using TFlowEvent =
       void __fastcall ( __closure * )(
           RTUProtocol& Sender, FlowDirection Dir, const FrameCont& Frame
       );

    /**
     * @brief Constructs the RTU protocol object.
     * @param RetryCount Maximum number of retransmission attempts per transaction
     *                   (default: MODBUS_RTU_DEFAULT_RETRY_COUNT = 3).
     */
    explicit RTUProtocol( int RetryCount = MODBUS_RTU_DEFAULT_RETRY_COUNT );

    /** @brief Destructor; closes the serial port if it is still open. */
    ~RTUProtocol();

    /** @brief Returns the COM port name (e.g., L"COM1"). */
    [[ nodiscard ]] String GetCommPort() const;
    /** @brief Sets the COM port name (e.g., L"COM3"). Must be called before Open(). */
    void SetCommPort( String Val );

    /** @brief Returns the configured baud rate (e.g., 9600, 19200). */
    [[ nodiscard ]] int GetCommSpeed() const noexcept;
    /** @brief Sets the baud rate. Must be called before Open(). */
    void SetCommSpeed( int Val );

    /** @brief Returns the configured parity (NOPARITY, ODDPARITY, EVENPARITY, etc.). */
    [[ nodiscard ]] int GetCommParity() const noexcept;
    /** @brief Sets the parity. Must be called before Open(). */
    void SetCommParity( int Val );

    /** @brief Returns the configured data bits (typically 8). */
    [[ nodiscard ]] int GetCommBits() const noexcept;
    /** @brief Sets the data bits. Must be called before Open(). */
    void SetCommBits( int Val );

    /** @brief Returns the configured stop bits (ONESTOPBIT, TWOSTOPBITS, etc.). */
    [[ nodiscard ]] int GetCommStopBits() const noexcept;
    /** @brief Sets the stop bits. Must be called before Open(). */
    void SetCommStopBits( int Val );

    /**
     * @brief Installs a frame-flow diagnostic callback and returns the previous one.
     * @param EventHandler New callback (pass @c nullptr to remove the current one).
     * @return The previously installed callback, or @c nullptr if none was installed.
     */
    TFlowEvent SetFlowEventHandler( TFlowEvent EventHandler ) noexcept;

protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus RTU" ); }
    virtual String DoGetProtocolParamsStr() const override;
    virtual void DoOpen() override;
    virtual void DoClose() override;
    virtual bool DoIsConnected() const noexcept override;

    virtual void DoReadCoilStatus( Context const & Context,
                                   CoilAddrType StartAddr,
                                   CoilCountType PointCount,
                                   CoilDataType* Data ) override;
    virtual void DoReadInputStatus( Context const & Context,
                                    CoilAddrType StartAddr,
                                    CoilCountType PointCount,
                                    CoilDataType* Data ) override;

    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) override;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) override;
    virtual void DoForceSingleCoil( Context const & Context,
                                    CoilAddrType Addr,
                                    bool Value ) override;
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr, RegDataType Data ) override;
    virtual ExceptionStatusDataType DoReadExceptionStatus(
                                        Context const & Context ) override;
    virtual RegDataType DoDiagnostics( Context const & Context,
                                       DiagSubFnType SubFunction,
                                       RegDataType Data ) override;
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
    virtual void DoForceMultipleCoils( Context const & Context,
                                       CoilAddrType StartAddr,
                                       CoilCountType PointCount,
                                       const CoilDataType* Data ) override;
    void DoPresetMultipleRegisters( Context const & Context,
                                    RegAddrType StartAddr,
                                    RegCountType PointCount,
                                    const RegDataType* Data ) override;

//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) override;
    virtual void DoReadWrite4XRegisters( Context const & Context,
                                         RegAddrType ReadStartAddr,
                                         RegCountType ReadPointCount,
                                         RegDataType* ReadData,
                                         RegAddrType WriteStartAddr,
                                         RegCountType WritePointCount,
                                         const RegDataType* WriteData ) override;
    virtual FIFOCountType DoReadFIFOQueue( Context const & Context,
                                          FIFOAddrType FIFOAddr,
                                          RegDataType* Data ) override;
private:

    static Context const DefaultRTUContext;

    TCommPort commPort_;
    bool cancelTXEcho_;
    int retryCount_;
    unsigned timeoutValue_;
    TFlowEvent onFlowEvent_;

  #if defined ( _DEBUG ) && defined( VIEW_RTU_PROTOCOL_DIAG )
    template<typename P, typename It>
    void ShowBuffer( const P& Prefix, It Begin, It End );
  #endif

    template<typename OutputIterator>
    void SendAndReceiveFrames( Context const & Context,
                               const FrameCont& TxFrame, OutputIterator Out,
                               FrameCont::size_type RxFramelength,
                               int RetryCount );

    template<typename OutputIterator>
    bool SendAndReceiveFramesInt( Context const & Context,
                                  const FrameCont& TxFrame, OutputIterator Out,
                                  FrameCont::size_type RxFramelength,
                                  bool NoThrow );

    template<typename OutputIterator>
    static OutputIterator Write( OutputIterator Out, uint8_t Data );

    template<typename OutputIterator>
    static OutputIterator Write( OutputIterator Out, uint16_t Data );

    template<typename InputIterator>
    static InputIterator Read( InputIterator In, uint16_t& Data );

    template<typename InputIterator>
    static uint16_t ComputeCRC( InputIterator Begin, InputIterator End );

    template<typename OutputIterator>
    static OutputIterator WriteAddressPointCountPair( OutputIterator Out,
                                                      RegAddrType StartAddr,
                                                      RegCountType PointCount );

    template<typename T>
    __int64 GetMinimumFrameTime( T FrameLen ) const;

    unsigned int GetParityBitCount() const;
    unsigned int GetStopBitCount() const;

//    static unsigned __int64 GetSystemTimeAsUint64();
//    unsigned __int64 GetTimeoutIntervalAsUint64() const;

    static String ParityToStr( int Val );
    static String StopBitsToStr( int Val );


    void ReadRegisters( FunctionCode FnCode, Context const & Context,
                        RegAddrType StartAddr, RegCountType PointCount,
                        RegDataType* Data );

    void ReadBits( FunctionCode FnCode, Context const & Context,
                   CoilAddrType StartAddr, CoilCountType PointCount,
                   CoilDataType* Data );

public:
    /**
     * @brief Computes and appends the Modbus CRC16 to an output iterator range.
     * @tparam OutputIterator Output iterator type (must accept uint8_t values).
     * @tparam InputIterator  Input iterator type over the frame bytes to checksum.
     * @param Out   Output iterator to receive the two CRC bytes (LSB first).
     * @param Begin Iterator to the start of the frame data.
     * @param End   Iterator past the end of the frame data.
     * @return Iterator past the written CRC bytes.
     */
    template<typename OutputIterator, typename InputIterator>
    static OutputIterator WriteCRC( OutputIterator Out,
                                    InputIterator Begin, InputIterator End );

    /** @brief When @c true, the protocol reads back and discards echoed TX bytes (half-duplex RS-485). */
    __property bool CancelTXEcho = { read = cancelTXEcho_, write = cancelTXEcho_ };

    /** @brief Maximum number of retransmission attempts on timeout or CRC error. */
    __property int RetryCount = { read = retryCount_, write = retryCount_ };

    /** @brief Per-character read timeout in milliseconds used by the serial port. */
    __property unsigned TimeoutValue = { read = timeoutValue_, write = timeoutValue_ };
};
//---------------------------------------------------------------------------

template<typename OutputIterator>
OutputIterator RTUProtocol::Write( OutputIterator Out, uint8_t Data )
{
    *Out++ = Data;
    return Out;
}
//---------------------------------------------------------------------------

template<typename OutputIterator>
OutputIterator RTUProtocol::Write( OutputIterator Out, uint16_t Data )
{
    *Out++ = ( Data >> 8 ) & 0xFF;   // Data Hi
    *Out++ = Data & 0xFF;            // Data Lo
    return Out;
}
//---------------------------------------------------------------------------

template<typename InputIterator>
InputIterator RTUProtocol::Read( InputIterator In, uint16_t& Data )
{
    uint16_t const HiData = static_cast<uint16_t>( *In++ & 0xFF ) << 8;
    Data = HiData + *In++;
    return In;
}
//---------------------------------------------------------------------------

template<typename OutputIterator>
OutputIterator RTUProtocol::WriteAddressPointCountPair( OutputIterator Out,
                                                        RegAddrType StartAddr,
                                                        RegCountType PointCount )
{
    Out = Write( Out, StartAddr );
    return Write( Out, PointCount );
}
//---------------------------------------------------------------------------

template<typename OutputIterator, typename InputIterator>
OutputIterator RTUProtocol::WriteCRC( OutputIterator Out,
                                            InputIterator Begin,
                                            InputIterator End )
{
    uint16_t const CRC = ComputeCRC( Begin, End );
    *Out++ = CRC & 0xFF;
    *Out++ = ( CRC >> 8 ) & 0xFF;
    return Out;
}
//---------------------------------------------------------------------------

template<typename InputIterator>
uint16_t RTUProtocol::ComputeCRC( InputIterator Begin, InputIterator End )
{
    return std::for_each( Begin, End, boost::crc_16_type( 0xFFFF ) ).checksum();
}
//---------------------------------------------------------------------------

#if defined ( _DEBUG ) && defined( VIEW_RTU_PROTOCOL_DIAG )
template<typename P, typename It>
void RTUProtocol::ShowBuffer( const P& Prefix, It Begin, It End )
{
    String Msg = String( Prefix );
    while ( Begin != End ) {
        Msg += IntToHex( int( *Begin++ ) & 0xFF, 2 ) + String( _D( ' ' ) );
    }
    ::OutputDebugString( Msg.c_str() );
}
#endif

template<typename OutputIterator>
void RTUProtocol::SendAndReceiveFrames( Context const & Context,
                                        const FrameCont& TxFrame, OutputIterator Out,
                                        FrameCont::size_type RxFramelength,
                                        int RetryCount )
{
    for ( int Idx = 0 ; ; ++Idx ) {
        if ( SendAndReceiveFramesInt( Context, TxFrame, Out, RxFramelength, Idx < RetryCount ) ) {
            break;
        }
    }
}

template<typename OutputIterator>
bool RTUProtocol::SendAndReceiveFramesInt( Context const & Context,
                                           const FrameCont& TxFrame, OutputIterator Out,
                                           FrameCont::size_type RxFramelength,
                                           bool NoThrow )
{
    static const FrameCont::size_type EatEchoExtraCharCount = 0;

    FrameCont RxFrame;

    RxFrame.reserve( 200 );

#if defined ( _DEBUG ) && defined( VIEW_RTU_PROTOCOL_DIAG )
    ShowBuffer(
        Format(
            _D( "TX(%s): " ), ARRAYOFCONST( ( commPort_.GetCommPort().c_str() ) )
        ),
        TxFrame.begin(), TxFrame.end()
    );
#endif
    if ( onFlowEvent_ )
        onFlowEvent_( *this, FlowDirection::TX, TxFrame );

    commPort_.PurgeCommPort();

    commPort_.WriteBuffer(
        const_cast<FrameCont::value_type*>( &TxFrame[0] ), TxFrame.size()
    );

    if ( CancelTXEcho ) {
        for ( FrameCont::size_type Cnt = TxFrame.size() + EatEchoExtraCharCount ; Cnt ; --Cnt ) {
            uint8_t Char;

            if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                    if ( NoThrow ) {
                        return false;
                    }
                    else {
                        throw EContextException(
                            Context,
                            SysErrorMessage( GetLastError() )
                        );
                    }
            }
        }
    }

    boost::crc_16_type Crc( 0xFFFF );

    for ( FrameCont::size_type Idx = 0 ; Idx < RxFramelength ; ++Idx ) {
        uint8_t Char;
        if ( unsigned int const BytesRead = commPort_.ReadBytes( &Char, 1 ) ) {
            Crc.process_byte( Char );
            RxFrame.push_back( Char );

            if ( ( Char & 0x80 ) && Idx == 1 ) {
//#if defined( _DEBUG )
//::OutputDebugString( Format( _D( "%.2X" ), ARRAYOFCONST( ( int( Char ) ) ) ).c_str() );
//endif
                RxFramelength = 5;
            }

        }
        else if ( NoThrow ) {
            return false;
        }
        else if ( !BytesRead ) {
            throw EContextException(
                Context,
                _D( "Timeout error" )
            );
        }
        else {
            throw EContextException(
                Context,
                SysErrorMessage( GetLastError() )
            );
        }
    }

#if defined ( _DEBUG ) && defined( VIEW_RTU_PROTOCOL_DIAG )
    ShowBuffer(
        Format(
            _D( "RX(%s): " ), ARRAYOFCONST( ( commPort_.GetCommPort().c_str() ) )
        ),
        RxFrame.begin(), RxFrame.end()
    );
#endif

    if ( onFlowEvent_ ) {
        onFlowEvent_( *this, FlowDirection::RX, RxFrame );
    }

    if ( Crc.checksum() ) {
        if ( NoThrow ) {
            return false;
        }
        else {
            throw EContextException( Context, _D( "Bad CRC (RX)" ) );
        }
    }

    if ( RxFrame[0] != TxFrame[0] ) {
        if ( NoThrow ) {
            return false;
        }
        else {
            throw EContextException( Context, _D( "Slave address mismatch" ) );
        }
    }

    if ( RxFrame[1] != TxFrame[1] ) {
        if ( NoThrow ) {
            return false;
        }
        else {
            if ( RxFrame[1] & 0x80 ) {
                RaiseStandardException( Context, ExceptionCode( RxFrame[2] ) );
            }
            else {
                throw EContextException( Context, _D( "Function code mismatch" ) );
            }
        }
    }

    std::copy( RxFrame.begin() + 2, RxFrame.end(), Out );

    return true;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif

