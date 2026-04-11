//---------------------------------------------------------------------------

#pragma hdrstop

#include <vector>
#include <iterator>
#include <algorithm>
#include <iostream>

#include "Modbus.h"
#include "ModbusRTU.h"

using std::vector;
using std::back_insert_iterator;
using std::back_inserter;
using std::wcout;
using std::endl;
using std::copy;

//---------------------------------------------------------------------------

#pragma package(smart_init)

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

RTUProtocol::RTUProtocol( int pRetryCount )
  : cancelTXEcho_( false )
  , retryCount_( pRetryCount )
  , onFlowEvent_( 0 )
{
    commPort_.SetParity( NOPARITY );
    commPort_.SetByteSize( 8 );
    commPort_.SetStopBits( ONESTOPBIT );
    commPort_.SetBaudRate( 115200 );
}
//---------------------------------------------------------------------------

RTUProtocol::~RTUProtocol()
{
}
//---------------------------------------------------------------------------

String RTUProtocol::GetCommPort() const
{
    return const_cast<TCommPort&>( commPort_ ).GetCommPort().c_str();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommPort( String Val )
{
    commPort_.SetCommPort( Val.c_str() );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommSpeed() const noexcept
{
    return const_cast<TCommPort&>( commPort_ ).GetBaudRate();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommSpeed( int Val )
{
    commPort_.SetBaudRate( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommParity() const noexcept
{
    return const_cast<TCommPort&>( commPort_ ).GetParity();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommParity( int Val )
{
    commPort_.SetParity( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommBits() const noexcept
{
    return const_cast<TCommPort&>( commPort_ ).GetByteSize();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommBits( int Val )
{
    commPort_.SetByteSize( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommStopBits() const noexcept
{
    return const_cast<TCommPort&>( commPort_ ).GetStopBits();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommStopBits( int Val )
{
    commPort_.SetStopBits( Val );
}
//---------------------------------------------------------------------------

unsigned int RTUProtocol::GetParityBitCount() const
{
    switch ( const_cast<TCommPort&>( commPort_ ).GetParity() ) {
        case NOPARITY:
            return 0;
        case ODDPARITY:
        case EVENPARITY:
            return 1;
        default:
            throw ERTUParametersError( _D( "Invalid parity setting" ) );
    }
}
//---------------------------------------------------------------------------

unsigned int RTUProtocol::GetStopBitCount() const
{
    switch ( const_cast<TCommPort&>( commPort_ ).GetParity() ) {
        case ONESTOPBIT :
            return 1;
        case ONE5STOPBITS:
        case TWOSTOPBITS:
            return 2;
        default:
            throw ERTUParametersError( _D( "Invalid stop bit setting" ) );
    }
}
//---------------------------------------------------------------------------

RTUProtocol::TFlowEvent RTUProtocol::SetFlowEventHandler( TFlowEvent EventHandler ) noexcept
{
    TFlowEvent Old = onFlowEvent_;
    onFlowEvent_ = EventHandler;
    return Old;
}
//---------------------------------------------------------------------------

String RTUProtocol::ParityToStr( int Val )
{
    switch ( Val ) {
        case NOPARITY:     return _D( "N" );
        case ODDPARITY:    return _D( "O" );
        case EVENPARITY:   return _D( "E" );
        case MARKPARITY:   return _D( "M" );
        case SPACEPARITY:  return _D( "S" );
        default:           return _D( "-" );
    }
}
//---------------------------------------------------------------------------

String RTUProtocol::StopBitsToStr( int Val )
{
    switch ( Val ) {
        case ONESTOPBIT:   return _D( "1" );
        case ONE5STOPBITS: return _D( "1.5" );
        case TWOSTOPBITS:  return _D( "2" );
        default:           return _D( "-" );
    }
}
//---------------------------------------------------------------------------

String RTUProtocol::DoGetProtocolParamsStr() const
{
    return
        Format(
            _D( "%s:%d,%s,%d,%s" )
          , ARRAYOFCONST( (
                ExtractFileName( GetCommPort() ),
                GetCommSpeed(),
                ParityToStr( GetCommParity() ),
                GetCommBits(),
                StopBitsToStr( GetCommStopBits() )
            ) )
        );
}
//---------------------------------------------------------------------------

void RTUProtocol::DoOpen()
{
    commPort_.OpenCommPort();
}
//---------------------------------------------------------------------------

void RTUProtocol::DoClose()
{
    commPort_.CloseCommPort();
}
//---------------------------------------------------------------------------

bool RTUProtocol::DoIsConnected() const noexcept
{
    return const_cast<TCommPort&>( commPort_ ).GetConnected();
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadCoilStatus
//    RTUProtocol::DoReadInputStatus

void RTUProtocol::ReadBits( FunctionCode FnCode, Context const & Context,
                            CoilAddrType StartAddr, CoilCountType PointCount,
                            CoilDataType* Data )
{
    if ( PointCount == 0 || PointCount > 2000 ) {
        throw EContextException(
            Context, _D( "Too many points have been requested" )
        );
    }

    FrameCont TxFrame;
    const FrameCont::size_type ExpectedByteCount( ( PointCount + 7 ) / 8 );
    const FrameCont::size_type ExpectedRxFramelength( ExpectedByteCount + 5 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ = static_cast<RegDataType>( FnCode );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, StartAddr, PointCount );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    FrameCont::const_iterator RxInIt = RxFrame.begin();

    const FrameCont::size_type RxByteCount =
        static_cast<FrameCont::size_type>( *RxInIt++ );

    if ( RxByteCount != ExpectedByteCount ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    for ( FrameCont::size_type Idx = 0; Idx < RxByteCount; ++Idx ) {
        *Data++ = *RxInIt++;
    }
}
//---------------------------------------------------------------------------

void RTUProtocol::DoReadCoilStatus( Context const & Context,
                                    CoilAddrType StartAddr,
                                    CoilCountType PointCount,
                                    CoilDataType* Data )
{
    ReadBits( FunctionCode::ReadCoilStatus, Context, StartAddr,
              PointCount, Data );
}
//---------------------------------------------------------------------------

void RTUProtocol::DoReadInputStatus( Context const & Context,
                                     CoilAddrType StartAddr,
                                     CoilCountType PointCount,
                                     CoilDataType* Data )
{
    ReadBits( FunctionCode::ReadInputStatus, Context, StartAddr,
              PointCount, Data );
}

//---------------------------------------------------------------------------

void RTUProtocol::ReadRegisters( FunctionCode FnCode, Context const & Context,
                                 RegAddrType StartAddr, RegCountType PointCount,
                                 RegDataType* Data )
{
    FrameCont TxFrame;
    FrameCont::size_type const ExpectedRxFramelength( PointCount * 2 + 5 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ = static_cast<RegDataType>( FnCode );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, StartAddr, PointCount );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    FrameCont::const_iterator RxInIt = RxFrame.begin();

    const FrameCont::size_type RxByteCount =
        static_cast<FrameCont::size_type>( *RxInIt++ );

    if ( RxByteCount !=  ExpectedRxFramelength - 5 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    while ( PointCount-- ) {
        RxInIt = Read( RxInIt, *Data++ );
    }
}
//---------------------------------------------------------------------------

void RTUProtocol::DoReadHoldingRegisters( Context const & Context,
                                          RegAddrType StartAddr,
                                          RegCountType PointCount,
                                          RegDataType* Data )
{
    ReadRegisters( FunctionCode::ReadHoldingRegisters, Context, StartAddr,
                   PointCount, Data );
/*
    if ( PointCount > 125 ) {
        throw EContextException(
            Context, _D( "Too many points have been requested" )
        );
    }

    FrameCont TxFrame;
    FrameCont::size_type const ExpectedRxFramelength( PointCount * 2 + 5 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ReadHoldingRegisters );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, StartAddr, PointCount );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    FrameCont::const_iterator RxInIt = RxFrame.begin();

    const FrameCont::size_type RxByteCount =
        static_cast<FrameCont::size_type>( *RxInIt++ );

    if ( RxByteCount !=  ExpectedRxFramelength - 5 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    while ( PointCount-- ) {
        RxInIt = Read( RxInIt, *Data++ );
    }
*/
}
//---------------------------------------------------------------------------

void RTUProtocol::DoReadInputRegisters( Context const & Context,
                                        RegAddrType StartAddr,
                                        RegCountType PointCount,
                                        RegDataType* Data )
{
    ReadRegisters( FunctionCode::ReadInputRegisters, Context, StartAddr,
                   PointCount, Data );
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoForceSingleCoil
void RTUProtocol::DoForceSingleCoil( Context const & Context,
                                     CoilAddrType Addr, bool Value )
{
    FrameCont TxFrame;
    const FrameCont::size_type ExpectedRxFramelength( 8 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ForceSingleCoil );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, Addr );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, static_cast<uint16_t>( Value ? 0xFF00 : 0x0000 ) );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    RegAddrType ReadAddr;
    FrameCont::const_iterator RxInIt = Read( RxFrame.begin(), ReadAddr );
    if ( ReadAddr != Addr ) {
        throw EContextException( Context, _D( "Address mismatch" ) );
    }

    uint16_t ReadValue;
    RxInIt = Read( RxInIt, ReadValue );
    uint16_t const ExpectedValue = Value ? 0xFF00 : 0x0000;
    if ( ReadValue != ExpectedValue ) {
        throw EContextException( Context, _D( "Data mismatch" ) );
    }
}
//---------------------------------------------------------------------------

void RTUProtocol::DoPresetSingleRegister( Context const & Context,
                                          RegAddrType Addr, RegDataType Data )
{
    FrameCont TxFrame;
    const FrameCont::size_type ExpectedRxFramelength( 8 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::PresetSingleRegister );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, Addr );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, Data );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    back_insert_iterator<FrameCont> RxFrameBkInsIt( RxFrame );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    RegAddrType ReadAddr;
    FrameCont::const_iterator RxInIt = Read( RxFrame.begin(), ReadAddr );
    if ( ReadAddr != Addr ) {
        throw EContextException( Context, _D( "Address mismatch" ) );
    }

    RegDataType ReadData;
    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != Data ) {
        throw EContextException( Context, _D( "Data mismatch" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadExceptionStatus
ExceptionStatusDataType RTUProtocol::DoReadExceptionStatus(
                                         Context const & Context )
{
    FrameCont TxFrame;
    // Response: SlaveAddr(1) + FC(1) + ExceptionStatus(1) + CRC(2) = 5
    const FrameCont::size_type ExpectedRxFramelength( 5 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 4 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ReadExceptionStatus );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    // RxFrame payload (after SendAndReceiveFrames strips SlaveAddr+FC+CRC):
    // ExceptionStatus(1)
    return static_cast<ExceptionStatusDataType>( RxFrame[0] );
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoDiagnostics
RegDataType RTUProtocol::DoDiagnostics( Context const & Context,
                                        DiagSubFnType SubFunction,
                                        RegDataType Data )
{
    FrameCont TxFrame;
    // Response: SlaveAddr(1) + FC(1) + SubFunction(2) + Data(2) + CRC(2) = 8
    const FrameCont::size_type ExpectedRxFramelength( 8 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::Diagnostics );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, SubFunction );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, Data );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    // RxFrame payload: SubFunction(2) + Data(2)
    FrameCont::const_iterator RxInIt = RxFrame.begin();

    uint16_t ReadSubFn;
    RxInIt = Read( RxInIt, ReadSubFn );
    if ( ReadSubFn != SubFunction ) {
        throw EContextException( Context, _D( "Sub-function mismatch" ) );
    }

    RegDataType ReadData;
    RxInIt = Read( RxInIt, ReadData );
    return ReadData;
}

//---------------------------------------------------------------------------

//    RTUProtocol::DoProgram484

//---------------------------------------------------------------------------

//    RTUProtocol::DoPoll484

//---------------------------------------------------------------------------

//    RTUProtocol::DoFetchCommEventCtr

//---------------------------------------------------------------------------

//    RTUProtocol::DoFetchCommEventLog

//---------------------------------------------------------------------------

//    RTUProtocol::DoProgramController

//---------------------------------------------------------------------------

//    RTUProtocol::DoPollController

//---------------------------------------------------------------------------

//    RTUProtocol::DoForceMultipleCoils
void RTUProtocol::DoForceMultipleCoils( Context const & Context,
                                        CoilAddrType StartAddr,
                                        CoilCountType PointCount,
                                        const CoilDataType* Data )
{
    if ( PointCount == 0 || PointCount > 1968 ) {
        throw EContextException(
            Context, _D( "Too many points have been requested" )
        );
    }

    const uint8_t ByteCount = static_cast<uint8_t>( ( PointCount + 7 ) / 8 );

    FrameCont TxFrame;
    const FrameCont::size_type ExpectedRxFramelength( 8 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 + ByteCount );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ForceMultipleCoils );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, StartAddr, PointCount );
    TxFrameBkInsIt =
        Write( TxFrameBkInsIt, ByteCount );

    for ( uint8_t Idx = 0; Idx < ByteCount; ++Idx ) {
        TxFrameBkInsIt = Write( TxFrameBkInsIt, Data[Idx] );
    }

    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    RegAddrType ReadStartAddr;
    FrameCont::const_iterator RxInIt = Read( RxFrame.begin(), ReadStartAddr );
    if ( ReadStartAddr != StartAddr ) {
        throw EContextException( Context, _D( "Start address mismatch" ) );
    }

    CoilCountType ReadPointCount;
    RxInIt = Read( RxInIt, ReadPointCount );
    if ( ReadPointCount != PointCount ) {
        throw EContextException( Context, _D( "Point count mismatch" ) );
    }
}
//---------------------------------------------------------------------------

void RTUProtocol::DoPresetMultipleRegisters( Context const & Context,
                                             RegAddrType StartAddr,
                                             RegCountType PointCount,
                                             const RegDataType* Data )
{
    if ( PointCount > 123 ) {
        throw EContextException(
            Context, _D( "Too many points have been requested" )
        );
    }

    const size_t DataByteCount = PointCount * sizeof( RegDataType );

    FrameCont TxFrame;
    const FrameCont::size_type ExpectedRxFramelength( 8 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 8 + DataByteCount );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::PresetMultipleRegisters );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, StartAddr, PointCount );
    TxFrameBkInsIt =
        Write( TxFrameBkInsIt, static_cast<uint8_t>( DataByteCount ) );

    for ( RegCountType Idx = 0 ; Idx < PointCount ; ++Idx ) {
        TxFrameBkInsIt = Write( TxFrameBkInsIt, *Data++ );
    }

    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    back_insert_iterator<FrameCont> RxFrameBkInsIt( RxFrame );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    RegAddrType ReadStartAddr;
    FrameCont::const_iterator RxInIt = Read( RxFrame.begin(), ReadStartAddr );
    if ( ReadStartAddr != StartAddr ) {
        throw EContextException( Context, _D( "Start address mismatch" ) );
    }

    RegCountType ReadPointCount;
    RxInIt = Read( RxInIt, ReadPointCount );
    if ( ReadPointCount != PointCount ) {
        throw EContextException( Context, _D( "Point count mismatch" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReportSlave

//---------------------------------------------------------------------------

//    RTUProtocol::DoProgram884_M84

//---------------------------------------------------------------------------

//    RTUProtocol::DoResetCommLink

//---------------------------------------------------------------------------

//    RTUProtocol::DoReadGeneralReference
void RTUProtocol::DoReadGeneralReference( Context const & Context,
                                          const FileSubRequest* SubRequests,
                                          size_t SubReqCount,
                                          RegDataType* Data )
{
    // FC20 request PDU:
    //   FC(1) + ByteCount(1) + N * [RefType(1) + FileNo(2) + RecNo(2) + RecLen(2)]
    // Each sub-request group is 7 bytes; RefType is always 0x06.
    const size_t subReqBytes = SubReqCount * 7;

    FrameCont TxFrame;
    TxFrame.reserve( 1 + 1 + 1 + subReqBytes + 2 ); // SlaveAddr+FC+ByteCount+subs+CRC
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<uint8_t>( FunctionCode::ReadGeneralReference );
    *TxFrameBkInsIt++ = static_cast<uint8_t>( subReqBytes );

    size_t totalRegs = 0;
    for ( size_t i = 0; i < SubReqCount; ++i ) {
        *TxFrameBkInsIt++ = 0x06; // reference type
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].FileNumber );
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].RecordNumber );
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].RecordLength );
        totalRegs += SubRequests[i].RecordLength;
    }
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    // FC20 response PDU (variable-length):
    //   SlaveAddr(1) + FC(1) + RespDataLen(1)
    //   + N * [SubRespLen(1) + RefType(1) + Data(RecordLength*2)]
    //   + CRC(2)
    // We use manual I/O like DoReadFIFOQueue because the response is variable-length.

    if ( onFlowEvent_ )
        onFlowEvent_( *this, FlowDirection::TX, TxFrame );

    commPort_.PurgeCommPort();
    commPort_.WriteBuffer(
        const_cast<FrameCont::value_type*>( &TxFrame[0] ), TxFrame.size()
    );

    if ( CancelTXEcho ) {
        for ( FrameCont::size_type Cnt = TxFrame.size() ; Cnt ; --Cnt ) {
            uint8_t Char;
            if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                throw EContextException( Context, _D( "TX echo timeout" ) );
            }
        }
    }

    // Read fixed header: SlaveAddr(1) + FC(1) + RespDataLen(1) = 3
    FrameCont RxFrame;
    RxFrame.reserve( 3 + SubReqCount * ( 2 + totalRegs * 2 ) + 2 );

    for ( int I = 0; I < 3; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );

        if ( I == 1 && ( Char & 0x80 ) ) {
            // Exception response
            for ( int J = 0; J < 3; ++J ) {
                if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                    throw EContextException( Context, _D( "Timeout error" ) );
                }
                RxFrame.push_back( Char );
            }
            if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
                throw EContextException( Context, _D( "Bad CRC (RX)" ) );
            }
            if ( RxFrame[0] != Context.GetSlaveAddr() ) {
                throw EContextException( Context, _D( "Slave address mismatch" ) );
            }
            RaiseStandardException( Context, ExceptionCode( RxFrame[2] ) );
        }
    }

    if ( RxFrame[0] != Context.GetSlaveAddr() ) {
        throw EContextException( Context, _D( "Slave address mismatch" ) );
    }
    if ( RxFrame[1] != static_cast<uint8_t>( FunctionCode::ReadGeneralReference ) ) {
        throw EContextException( Context, _D( "Function code mismatch" ) );
    }

    const uint8_t respDataLen = RxFrame[2];

    // Read remaining bytes: respDataLen + CRC(2)
    const int Remaining = respDataLen + 2;
    for ( int I = 0; I < Remaining; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );
    }

    if ( onFlowEvent_ ) {
        onFlowEvent_( *this, FlowDirection::RX, RxFrame );
    }

    if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
        throw EContextException( Context, _D( "Bad CRC (RX)" ) );
    }

    // Parse sub-response groups starting at offset 3
    size_t off = 3;
    RegDataType* dataOut = Data;
    for ( size_t i = 0; i < SubReqCount; ++i ) {
        if ( off >= RxFrame.size() - 2 ) {
            throw EContextException( Context, _D( "Truncated response" ) );
        }
        const uint8_t subRespLen = RxFrame[off++];
        const uint8_t refType    = RxFrame[off++];
        if ( refType != 0x06 ) {
            throw EContextException( Context, _D( "Invalid reference type" ) );
        }
        // subRespLen includes RefType(1) + Data(N*2), so data bytes = subRespLen - 1
        const size_t dataBytes = subRespLen - 1;
        if ( dataBytes != SubRequests[i].RecordLength * 2 ) {
            throw EContextException( Context, _D( "Sub-response length mismatch" ) );
        }
        for ( RecordLengthType r = 0; r < SubRequests[i].RecordLength; ++r ) {
            *dataOut++ = static_cast<RegDataType>(
                ( static_cast<uint16_t>( RxFrame[off] ) << 8 ) |
                ( static_cast<uint16_t>( RxFrame[off + 1] ) & 0xFF )
            );
            off += 2;
        }
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoWriteGeneralReference
void RTUProtocol::DoWriteGeneralReference( Context const & Context,
                                           const FileSubRequest* SubRequests,
                                           size_t SubReqCount,
                                           const RegDataType* Data )
{
    // FC21 request PDU:
    //   FC(1) + ByteCount(1) + N * [RefType(1) + FileNo(2) + RecNo(2) + RecLen(2) + Data(RecLen*2)]
    size_t totalRegs = 0;
    for ( size_t i = 0; i < SubReqCount; ++i )
        totalRegs += SubRequests[i].RecordLength;

    const size_t reqBytes = SubReqCount * 7 + totalRegs * 2;

    FrameCont TxFrame;
    TxFrame.reserve( 1 + 1 + 1 + reqBytes + 2 ); // SlaveAddr+FC+ByteCount+data+CRC
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<uint8_t>( FunctionCode::WriteGeneralReference );
    *TxFrameBkInsIt++ = static_cast<uint8_t>( reqBytes );

    const RegDataType* dataIn = Data;
    for ( size_t i = 0; i < SubReqCount; ++i ) {
        *TxFrameBkInsIt++ = 0x06; // reference type
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].FileNumber );
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].RecordNumber );
        TxFrameBkInsIt = Write( TxFrameBkInsIt, SubRequests[i].RecordLength );
        for ( RecordLengthType r = 0; r < SubRequests[i].RecordLength; ++r ) {
            TxFrameBkInsIt = Write( TxFrameBkInsIt, *dataIn++ );
        }
    }
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    // FC21 response is an echo of the request (variable-length).
    // Use manual I/O.

    if ( onFlowEvent_ )
        onFlowEvent_( *this, FlowDirection::TX, TxFrame );

    commPort_.PurgeCommPort();
    commPort_.WriteBuffer(
        const_cast<FrameCont::value_type*>( &TxFrame[0] ), TxFrame.size()
    );

    if ( CancelTXEcho ) {
        for ( FrameCont::size_type Cnt = TxFrame.size() ; Cnt ; --Cnt ) {
            uint8_t Char;
            if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                throw EContextException( Context, _D( "TX echo timeout" ) );
            }
        }
    }

    // Read fixed header: SlaveAddr(1) + FC(1) + RespDataLen(1) = 3
    FrameCont RxFrame;
    RxFrame.reserve( TxFrame.size() );

    for ( int I = 0; I < 3; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );

        if ( I == 1 && ( Char & 0x80 ) ) {
            for ( int J = 0; J < 3; ++J ) {
                if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                    throw EContextException( Context, _D( "Timeout error" ) );
                }
                RxFrame.push_back( Char );
            }
            if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
                throw EContextException( Context, _D( "Bad CRC (RX)" ) );
            }
            if ( RxFrame[0] != Context.GetSlaveAddr() ) {
                throw EContextException( Context, _D( "Slave address mismatch" ) );
            }
            RaiseStandardException( Context, ExceptionCode( RxFrame[2] ) );
        }
    }

    if ( RxFrame[0] != Context.GetSlaveAddr() ) {
        throw EContextException( Context, _D( "Slave address mismatch" ) );
    }
    if ( RxFrame[1] != static_cast<uint8_t>( FunctionCode::WriteGeneralReference ) ) {
        throw EContextException( Context, _D( "Function code mismatch" ) );
    }

    const uint8_t respDataLen = RxFrame[2];

    // Read remaining bytes: respDataLen + CRC(2)
    const int Remaining = respDataLen + 2;
    for ( int I = 0; I < Remaining; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );
    }

    if ( onFlowEvent_ ) {
        onFlowEvent_( *this, FlowDirection::RX, RxFrame );
    }

    if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
        throw EContextException( Context, _D( "Bad CRC (RX)" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoMaskWrite4XRegister
void RTUProtocol::DoMaskWrite4XRegister( Context const & Context,
                                         RegAddrType Addr,
                                         RegDataType AndMask,
                                         RegDataType OrMask )
{
    FrameCont TxFrame;
    const FrameCont::size_type ExpectedRxFramelength = 10;
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 10 );

    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::MaskWrite4XRegister );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, Addr );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, AndMask );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, OrMask );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    back_insert_iterator<FrameCont> RxFrameBkInsIt( RxFrame );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    RegAddrType ReadAddr;
    FrameCont::const_iterator RxInIt = Read( RxFrame.begin(), ReadAddr );
    if ( ReadAddr != Addr ) {
        throw EContextException( Context, _D( "Address mismatch" ) );
    }

    RegDataType ReadData;
    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != AndMask ) {
        throw EContextException( Context, _D( "And Mask mismatch" ) );
    }

    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != OrMask ) {
        throw EContextException( Context, _D( "Or Mask mismatch" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadWrite4XRegisters
void RTUProtocol::DoReadWrite4XRegisters( Context const & Context,
                                          RegAddrType ReadStartAddr,
                                          RegCountType ReadPointCount,
                                          RegDataType* ReadData,
                                          RegAddrType WriteStartAddr,
                                          RegCountType WritePointCount,
                                          const RegDataType* WriteData )
{
    const size_t WriteByteCount = WritePointCount * sizeof( RegDataType );

    FrameCont TxFrame;
    // Response: SlaveAddr(1) + FC(1) + ByteCount(1) + ReadData(ReadPointCount*2) + CRC(2)
    FrameCont::size_type const ExpectedRxFramelength( ReadPointCount * 2 + 5 );
    FrameCont RxFrame;

    RxFrame.reserve( ExpectedRxFramelength );
    TxFrame.reserve( 13 + WriteByteCount );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );
    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ReadWrite4XRegisters );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, ReadStartAddr, ReadPointCount );
    TxFrameBkInsIt =
        WriteAddressPointCountPair( TxFrameBkInsIt, WriteStartAddr, WritePointCount );
    TxFrameBkInsIt =
        Write( TxFrameBkInsIt, static_cast<uint8_t>( WriteByteCount ) );

    for ( RegCountType Idx = 0; Idx < WritePointCount; ++Idx ) {
        TxFrameBkInsIt = Write( TxFrameBkInsIt, WriteData[Idx] );
    }

    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    SendAndReceiveFrames(
        Context, TxFrame, back_inserter( RxFrame ),
        ExpectedRxFramelength, retryCount_
    );

    FrameCont::const_iterator RxInIt = RxFrame.begin();

    const FrameCont::size_type RxByteCount =
        static_cast<FrameCont::size_type>( *RxInIt++ );

    if ( RxByteCount != ReadPointCount * 2 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    RegCountType Remaining = ReadPointCount;
    while ( Remaining-- ) {
        RxInIt = Read( RxInIt, *ReadData++ );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadFIFOQueue
FIFOCountType RTUProtocol::DoReadFIFOQueue( Context const & Context,
                                            FIFOAddrType FIFOAddr,
                                            RegDataType* Data )
{
    FrameCont TxFrame;
    // We don't know the FIFO count yet, so first try with max response size:
    // SlaveAddr(1) + FC(1) + ByteCount(2) + FIFOCount(2) + MaxValues(31*2) + CRC(2) = 70
    // But SendAndReceiveFrames expects the exact length.
    // We'll send the request and read a minimal response first to get the count.
    // Actually, the RTU framing reads byte-by-byte, so we need a two-phase approach:
    //
    // Phase 1: Send request, read header to determine FIFO count.
    // For RTU, SendAndReceiveFrames reads exactly ExpectedRxFramelength bytes.
    // We must first read the minimum response (no FIFO values) to learn the count,
    // then read the remaining values. However, the existing RTU framework reads
    // the full frame in one call.
    //
    // Since the RTU protocol reads byte-by-byte with CRC validation, we can use
    // a max-size approach: request the maximum possible frame length (70 bytes)
    // but the actual slave response will be shorter. The timeout on missing bytes
    // triggers a retry, which won't work for variable-length responses.
    //
    // Instead, we do manual I/O similar to SendAndReceiveFramesInt but handle
    // the variable-length response ourselves.

    TxFrame.reserve( 6 );
    back_insert_iterator<FrameCont> TxFrameBkInsIt( TxFrame );

    *TxFrameBkInsIt++ = Context.GetSlaveAddr();
    *TxFrameBkInsIt++ =
        static_cast<RegDataType>( FunctionCode::ReadFIFOQueue );
    TxFrameBkInsIt = Write( TxFrameBkInsIt, FIFOAddr );
    WriteCRC( TxFrameBkInsIt, TxFrame.begin(), TxFrame.end() );

    // Read the fixed header portion first:
    // SlaveAddr(1) + FC(1) + ByteCount(2) + FIFOCount(2) + CRC(2) = 8
    // Then we know how many more bytes to expect.
    // However, we need to handle this in a single read because CRC covers everything.
    //
    // Alternative: read the fixed header (6 bytes: SlaveAddr+FC+ByteCount+FIFOCount),
    // compute remaining bytes from FIFOCount, read the rest, then validate CRC.
    //
    // For simplicity and consistency with the existing pattern, we use a reasonable
    // upper bound. The Modbus spec limits FIFO to 31 values, so max frame = 70 bytes.
    // We'll read exactly 8 bytes first (minimum valid response), extract FIFOCount,
    // then compute the full expected length and re-read if needed.
    //
    // Actually, the cleanest approach that fits the existing architecture:
    // Just use SendAndReceiveFrames with the minimum header length (8 = empty FIFO),
    // but that won't work if FIFOCount > 0.
    //
    // Let's do a direct serial I/O approach:

    if ( onFlowEvent_ )
        onFlowEvent_( *this, FlowDirection::TX, TxFrame );

    commPort_.PurgeCommPort();
    commPort_.WriteBuffer(
        const_cast<FrameCont::value_type*>( &TxFrame[0] ), TxFrame.size()
    );

    if ( CancelTXEcho ) {
        for ( FrameCont::size_type Cnt = TxFrame.size() ; Cnt ; --Cnt ) {
            uint8_t Char;
            if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                throw EContextException( Context, _D( "TX echo timeout" ) );
            }
        }
    }

    // Read fixed header: SlaveAddr(1) + FC(1) + ByteCount(2) + FIFOCount(2) = 6
    FrameCont RxFrame;
    RxFrame.reserve( 70 );

    for ( int I = 0; I < 6; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );

        // Check for exception response at byte index 1
        if ( I == 1 && ( Char & 0x80 ) ) {
            // Exception response: read one more byte (exception code) + CRC(2)
            for ( int J = 0; J < 3; ++J ) {
                if ( !commPort_.ReadBytes( &Char, 1 ) ) {
                    throw EContextException( Context, _D( "Timeout error" ) );
                }
                RxFrame.push_back( Char );
            }
            // Validate CRC
            if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
                throw EContextException( Context, _D( "Bad CRC (RX)" ) );
            }
            if ( RxFrame[0] != Context.GetSlaveAddr() ) {
                throw EContextException( Context, _D( "Slave address mismatch" ) );
            }
            RaiseStandardException( Context, ExceptionCode( RxFrame[2] ) );
        }
    }

    // Validate slave address and function code
    if ( RxFrame[0] != Context.GetSlaveAddr() ) {
        throw EContextException( Context, _D( "Slave address mismatch" ) );
    }
    if ( RxFrame[1] != static_cast<uint8_t>( FunctionCode::ReadFIFOQueue ) ) {
        throw EContextException( Context, _D( "Function code mismatch" ) );
    }

    uint16_t const ByteCount =
        ( static_cast<uint16_t>( RxFrame[2] ) << 8 ) |
        ( static_cast<uint16_t>( RxFrame[3] ) & 0xFF );

    uint16_t const FIFOCount =
        ( static_cast<uint16_t>( RxFrame[4] ) << 8 ) |
        ( static_cast<uint16_t>( RxFrame[5] ) & 0xFF );

    if ( FIFOCount > 31 ) {
        throw EContextException( Context, _D( "FIFO count exceeds maximum (31)" ) );
    }

    if ( ByteCount != 2 + FIFOCount * 2 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    // Read remaining bytes: FIFOValues(FIFOCount * 2) + CRC(2)
    const int Remaining = FIFOCount * 2 + 2;
    for ( int I = 0; I < Remaining; ++I ) {
        uint8_t Char;
        if ( !commPort_.ReadBytes( &Char, 1 ) ) {
            throw EContextException( Context, _D( "Timeout error" ) );
        }
        RxFrame.push_back( Char );
    }

    if ( onFlowEvent_ ) {
        onFlowEvent_( *this, FlowDirection::RX, RxFrame );
    }

    // Validate CRC over entire frame
    if ( ComputeCRC( RxFrame.begin(), RxFrame.end() ) ) {
        throw EContextException( Context, _D( "Bad CRC (RX)" ) );
    }

    // Extract FIFO register values (starting at byte offset 6)
    for ( FIFOCountType I = 0; I < FIFOCount; ++I ) {
        int const Off = 6 + I * 2;
        Data[I] = static_cast<RegDataType>(
            ( static_cast<uint16_t>( RxFrame[Off] ) << 8 ) |
            ( static_cast<uint16_t>( RxFrame[Off + 1] ) & 0xFF )
        );
    }

    return static_cast<FIFOCountType>( FIFOCount );
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

