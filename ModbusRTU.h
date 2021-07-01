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

#define FT_MICROSECOND   ( 10UI64 )
#define FT_MILLISECOND   ( 1000UI64 * FT_MICROSECOND )
#define FT_SECOND        ( 1000UI64 * FT_MILLISECOND )
#define FT_MINUTE        ( 60UI64 * FT_SECOND )
#define FT_HOUR          ( 60UI64 * FT_MINUTE )
#define FT_DAY           ( 24UI64 * FT_HOUR )

#if !defined( MODBUS_RTU_DEFAULT_RETRY_COUNT )
  #define  MODBUS_RTU_DEFAULT_RETRY_COUNT  3
#endif

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------

class ERTUParametersError : public EBaseException
{
public:
    ERTUParametersError( String Msg ) : EBaseException( Msg ) {}
};

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class RTUProtocol : public Protocol {
public:
    using FrameCont = std::vector<uint8_t>;

    enum class FlowDirection { RX, TX };

    using TFlowEvent =
       void __fastcall ( __closure * )(
           RTUProtocol& Sender, FlowDirection Dir, const FrameCont& Frame
       );

    explicit RTUProtocol( int RetryCount = MODBUS_RTU_DEFAULT_RETRY_COUNT );
    ~RTUProtocol();

    String GetCommPort() const;
    void SetCommPort( String Val );
    int GetCommSpeed() const;
    void SetCommSpeed( int Val );
    int GetCommParity() const;
    void SetCommParity( int Val );
    int GetCommBits() const;
    void SetCommBits( int Val );
    int GetCommStopBits() const;
    void SetCommStopBits( int Val );

    TFlowEvent SetFlowEventHandler( TFlowEvent EventHandler );

protected:
    virtual String DoGetProtocolName() const override { return _T( "Modbus RTU" ); }
    virtual String DoGetProtocolParamsStr() const override;
    virtual void DoOpen() override;
    virtual void DoClose() override;
    virtual bool DoIsConnected() const override;

//    DoReadCoilStatus
//    DoReadInputStatus

    virtual void DoReadHoldingRegisters( Context const & Context,
                                         RegAddrType StartAddr,
                                         RegCountType PointCount,
                                         RegDataType* Data ) override;
    virtual void DoReadInputRegisters( Context const & Context,
                                       RegAddrType StartAddr,
                                       RegCountType PointCount,
                                       RegDataType* Data ) override;
//    DoForceSingleCoil
    virtual void DoPresetSingleRegister( Context const & Context,
                                         RegAddrType Addr, RegDataType Data ) override;
//    DoReadExceptionStatus
//    DoDiagnostics
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
//    DoForceMultipleCoils
//    DoPresetMultipleRegisters
    void DoPresetMultipleRegisters( Context const & Context,
                                    RegAddrType StartAddr,
                                    RegCountType PointCount,
                                    const RegDataType* Data ) override;

//    DoReportSlave
//    DoProgram884_M84
//    DoResetCommLink
//    DoReadGeneralReference
//    DoWriteGeneralReference
//    DoMaskWrite4XRegister
    virtual void DoMaskWrite4XRegister( Context const & Context,
                                        RegAddrType Addr,
                                        RegDataType AndMask,
                                        RegDataType OrMask ) override;
//    DoReadWrite4XRegisters
//    DoReadFIFOQueue
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

public:
    template<typename OutputIterator, typename InputIterator>
    static OutputIterator WriteCRC( OutputIterator Out,
                                    InputIterator Begin, InputIterator End );
    __property bool CancelTXEcho = { read = cancelTXEcho_, write = cancelTXEcho_ };
    __property int RetryCount = { read = retryCount_, write = retryCount_ };
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
        Msg += IntToHex( int( *Begin++ ) & 0xFF, 2 ) + String( _T( ' ' ) );
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
            _T( "TX(%s): " ), ARRAYOFCONST( ( commPort_.GetCommPort().c_str() ) )
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
//::OutputDebugString( Format( _T( "%.2X" ), ARRAYOFCONST( ( int( Char ) ) ) ).c_str() );
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
                _T( "Timeout error" )
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
            _T( "RX(%s): " ), ARRAYOFCONST( ( commPort_.GetCommPort().c_str() ) )
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
            throw EContextException( Context, _T( "Bad CRC (RX)" ) );
        }
    }

    if ( RxFrame[0] != TxFrame[0] ) {
        if ( NoThrow ) {
            return false;
        }
        else {
            throw EContextException( Context, _T( "Slave address mismatch" ) );
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
                throw EContextException( Context, _T( "Function code mismatch" ) );
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

