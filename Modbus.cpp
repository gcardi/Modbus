
#pragma hdrstop

#include <memory>

#include "Modbus.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------

void DebugBytesToHex( String Prologue, TBytes Data )
{
#if defined( _DEBUG )
    auto SB = std::make_unique<TStringBuilder>( Prologue );
    for ( auto Val : Data ) {
        SB->AppendFormat( _T( "%.2X " ), ARRAYOFCONST( ( Val ) ) );
    }
    ::OutputDebugString( SB->ToString().c_str() );
#endif
}

static const String ExceptionCodeText[8] = {
    _T( "Illegal Function" ),
    _T( "Illegal Data Address" ),
    _T( "Illegal Data Value" ),
    _T( "Slave Device Failure" ),
    _T( "Acknowledge" ),
    _T( "Slave Device Busy" ),
    _T( "Negative Acknowledge" ),
    _T( "Memory Parity Error" ),
};

static const String ExceptionCodeDescription[8] = {
    _T( "The function code received in the query "
        "is not an allowable action for the slave. "
        "If a Poll Program Complete command "
        "was issued, this code indicates that no "
        "program function preceded it." ),

    _T( "The data address received in the query "
        "is not an allowable address for the "
        "slave." ),

    _T( "A value contained in the query data "
        "field is not an allowable value for the "
        "slave." ),

    _T( "An unrecoverable error occurred while "
        "the slave was attempting to perform the "
        "requested action." ),

    _T( "The slave has accepted the request "
        "and is processing it, but a long duration "
        "of time will be required to do so. This "
        "response is returned to prevent a "
        "timeout error from occurring in the "
        "master. The master can next issue a "
        "Poll Program Complete message to "
        "determine if processing is completed." ),

    _T( "The slave is engaged in processing a "
        "long–duration program command. The "
        "master should retransmit the message "
        "later when the slave is free." ),

    _T( "The slave cannot perform the program "
        "function received in the query. This "
        "code is returned for an unsuccessful "
        "programming request using function "
        "code 13 or 14 decimal. The master "
        "should request diagnostic or error "
        "information from the slave." ),

    _T( "The slave attempted to read extended "
        "memory, but detected a parity error in "
        "the memory. The master can retry the "
        "request, but service may be required on "
        "the slave device." ),
};
//---------------------------------------------------------------------------

String GetExceptionCodeText( ExceptionCode Code )
{
    return ExceptionCodeText[static_cast<size_t>( Code ) - 1];
}
//---------------------------------------------------------------------------

String GetExceptionCodeDescription( ExceptionCode Code )
{
    return ExceptionCodeDescription[static_cast<size_t>( Code ) - 1];
}
//---------------------------------------------------------------------------

void RaiseFunctionCodeNotImplementedException( FunctionCode Code )
{
    throw EBaseException(
        Format(
            _T( "Function code %.2Xh not implemented" )
          , ARRAYOFCONST( ( static_cast<int>( Code ) ) )
        )
    );

/*
    throw EBaseException( String( _T( "Function code " ) ) +
                          IntToHex( static_cast<int>( Code ), 2 ) +
                          String( _T( "H not implemented!" ) ) );
*/
}
//---------------------------------------------------------------------------

void RaiseStandardException( Context const & Context, ExceptionCode Code,
                             String Prefix )
{
    switch ( Code ) {
        case ExceptionCode::IllegalFunction:     throw EIllegalFunction( Context, Prefix );
        case ExceptionCode::IllegalDataAddress:  throw EIllegalDataAddress( Context, Prefix );
        case ExceptionCode::IllegalDataValue:    throw EIllegalDataValue( Context, Prefix );
        case ExceptionCode::SlaveDeviceFailure:  throw ESlaveDeviceFailure( Context, Prefix );
        case ExceptionCode::Acknowledge:         throw EAcknowledge( Context, Prefix );
        case ExceptionCode::SlaveDeviceBusy:     throw ESlaveDeviceBusy( Context, Prefix );
        case ExceptionCode::NegativeAcknowledge: throw ENegativeAcknowledge( Context, Prefix );
        case ExceptionCode::MemoryParityError:   throw EMemoryParityError( Context, Prefix );
        default:
            throw EProtocolException(
                Context, Code, _T( "Unknown Modbus exception code" )
            );
    }
}

//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

Protocol::Protocol()
{
}
//---------------------------------------------------------------------------

Protocol::~Protocol()
{
}
//---------------------------------------------------------------------------

void Protocol::Open()
{
    if ( !IsConnected() ) {
    //RaiseExceptionIfIsConnected( _T( "connection already open" ) );
        DoOpen();
    }
}
//---------------------------------------------------------------------------

void Protocol::Close()
{
    if ( IsConnected() ) {
    //RaiseExceptionIfIsNotConnected( _T( "connection already closed" ) );
        DoClose();
    }
}
//---------------------------------------------------------------------------

//    DoReadCoilStatus
//    DoReadInputStatus

void Protocol::DoReadHoldingRegisters( Context const & /* Context */,
                                       RegAddrType /*StartAddr*/,
                                       RegCountType /*PointCount*/,
                                       RegDataType* /*Data*/ )
{
    RaiseFunctionCodeNotImplementedException( FunctionCode::ReadHoldingRegisters );
}
//---------------------------------------------------------------------------

//    DoReadInputRegisters
//    DoForceSingleCoil

void Protocol::DoPresetSingleRegister( Context const & /* Context */,
                                       RegAddrType /* Addr */,
                                       RegDataType /* Data */ )
{
    RaiseFunctionCodeNotImplementedException( FunctionCode::PresetSingleRegister );
}
//---------------------------------------------------------------------------

//    DoReadExceptionStatus
//    DoDiagnostics
//    DoProgram484
//    DoPoll484
//    DoFetchCommEventCtr
//    DoFetchCommEventLog
//    DoProgramController
//    DoPollController
//    DoForceMultipleCoils

void Protocol::DoPresetMultipleRegisters( Context const & /* Context */,
                                          RegAddrType /*StartAddr*/,
                                          RegCountType /*PointCount*/,
                                          RegDataType const * /*Data*/ )
{
    RaiseFunctionCodeNotImplementedException( FunctionCode::PresetMultipleRegisters );
}
//---------------------------------------------------------------------------

//    ReportSlave
//    Program884_M84
//    ResetCommLink
//    ReadGeneralReference
//    WriteGeneralReference
//    MaskWrite4XRegister
void Protocol::DoMaskWrite4XRegister( Context const & Context,
                                      RegAddrType Addr,
                                      RegDataType AndMask,
                                      RegDataType OrMask )
{
    RaiseFunctionCodeNotImplementedException( FunctionCode::PresetMultipleRegisters );
}
//    ReadWrite4XRegisters
//    ReadFIFOQueue
//---------------------------------------------------------------------------


void Protocol::RaiseExceptionIfIsConnected( String Msg ) const
{
    if ( IsConnected() ) {
        throw EBaseException(
            Format(
                _T( "The connection is already estabilished: %s" )
              , ARRAYOFCONST( ( Msg ) )
            )
        );
    }
}
//---------------------------------------------------------------------------

void Protocol::RaiseExceptionIfIsNotConnected( String Msg ) const
{
    if ( !IsConnected() ) {
        throw EBaseException(
            Format(
                _T( "The connection was already previously close: %s" )
              , ARRAYOFCONST( ( Msg ) )
            )
        );
    }
}

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

