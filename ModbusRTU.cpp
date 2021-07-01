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

int RTUProtocol::GetCommSpeed() const
{
    return const_cast<TCommPort&>( commPort_ ).GetBaudRate();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommSpeed( int Val )
{
    commPort_.SetBaudRate( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommParity() const
{
    return const_cast<TCommPort&>( commPort_ ).GetParity();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommParity( int Val )
{
    commPort_.SetParity( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommBits() const
{
    return const_cast<TCommPort&>( commPort_ ).GetByteSize();
}
//---------------------------------------------------------------------------

void RTUProtocol::SetCommBits( int Val )
{
    commPort_.SetByteSize( Val );
}
//---------------------------------------------------------------------------

int RTUProtocol::GetCommStopBits() const
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
            throw ERTUParametersError( _T( "Invalid parity setting" ) );
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
            throw ERTUParametersError( _T( "Invalid stop bit setting" ) );
    }
}
//---------------------------------------------------------------------------

RTUProtocol::TFlowEvent RTUProtocol::SetFlowEventHandler( TFlowEvent EventHandler )
{
    TFlowEvent Old = onFlowEvent_;
    onFlowEvent_ = EventHandler;
    return Old;
}
//---------------------------------------------------------------------------

String RTUProtocol::ParityToStr( int Val )
{
    switch ( Val ) {
        case NOPARITY:     return _T( "N" );
        case ODDPARITY:    return _T( "O" );
        case EVENPARITY:   return _T( "E" );
        case MARKPARITY:   return _T( "M" );
        case SPACEPARITY:  return _T( "S" );
        default:           return _T( "-" );
    }
}
//---------------------------------------------------------------------------

String RTUProtocol::StopBitsToStr( int Val )
{
    switch ( Val ) {
        case ONESTOPBIT:   return _T( "1" );
        case ONE5STOPBITS: return _T( "1.5" );
        case TWOSTOPBITS:  return _T( "2" );
        default:           return _T( "-" );
    }
}
//---------------------------------------------------------------------------

String RTUProtocol::DoGetProtocolParamsStr() const
{
    return
        Format(
            _T( "%s:%d,%s,%d,%s" )
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

bool RTUProtocol::DoIsConnected() const
{
    return const_cast<TCommPort&>( commPort_ ).GetConnected();
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadCoilStatus
//    RTUProtocol::DoReadInputStatus

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
        throw EContextException( Context, _T( "Byte count mismatch" ) );
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
            Context, _T( "Too many points have been requested" )
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
        throw EContextException( Context, _T( "Byte count mismatch" ) );
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
        throw EContextException( Context, _T( "Address mismatch" ) );
    }

    RegDataType ReadData;
    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != Data ) {
        throw EContextException( Context, _T( "Data mismatch" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadExceptionStatus

//---------------------------------------------------------------------------

//    RTUProtocol::DoDiagnostics

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

//---------------------------------------------------------------------------

void RTUProtocol::DoPresetMultipleRegisters( Context const & Context,
                                             RegAddrType StartAddr,
                                             RegCountType PointCount,
                                             const RegDataType* Data )
{
    if ( PointCount > 123 ) {
        throw EContextException(
            Context, _T( "Too many points have been requested" )
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
        throw EContextException( Context, _T( "Start address mismatch" ) );
    }

    RegCountType ReadPointCount;
    RxInIt = Read( RxInIt, ReadPointCount );
    if ( ReadPointCount != PointCount ) {
        throw EContextException( Context, _T( "Point count mismatch" ) );
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

//---------------------------------------------------------------------------

//    RTUProtocol::DoWriteGeneralReference

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
        throw EContextException( Context, _T( "Address mismatch" ) );
    }

    RegDataType ReadData;
    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != AndMask ) {
        throw EContextException( Context, _T( "And Mask mismatch" ) );
    }

    RxInIt = Read( RxInIt, ReadData );
    if ( ReadData != OrMask ) {
        throw EContextException( Context, _T( "Or Mask mismatch" ) );
    }
}
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadWrite4XRegisters
//---------------------------------------------------------------------------

//    RTUProtocol::DoReadFIFOQueue
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

