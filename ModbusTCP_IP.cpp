//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include <algorithm>

#include "Modbus.h"
#include "ModbusTCP_IP.h"

#define  MODBUS_TCP_IP_BMAP_TRANSACTION_ID_OFFSET  0
#define  MODBUS_TCP_IP_BMAP_PROTOCOL_OFFSET        2
#define  MODBUS_TCP_IP_BMAP_DATA_LENGTH_OFFSET     4
#define  MODBUS_TCP_IP_BMAP_UNIT_ID_OFFSET         6

#define  MODBUS_TCP_IP_REPLY_FUNCTION_CODE_OFFSET  0
#define  MODBUS_TCP_IP_REPLY_EXCEPTION_CODE_OFFSET 1
#define  MODBUS_TCP_IP_REPLY_DATA_OFFSET           1

using std::min;

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

String TCPIPProtocol::GetHost() const
{
    return DoGetHost();
}
//---------------------------------------------------------------------------

void TCPIPProtocol::SetHost( String Val )
{
    RaiseExceptionIfIsConnected( _T( "Unable to change host" ) );
    DoSetHost( Val );
}
//---------------------------------------------------------------------------

uint16_t TCPIPProtocol::GetPort() const
{
    return DoGetPort();
}
//---------------------------------------------------------------------------

void TCPIPProtocol::SetPort( uint16_t Val )
{
    RaiseExceptionIfIsConnected( _T( "Unable to change port" ) );
    DoSetPort( Val );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::RaiseExceptionIfBMAPIsNotValid( Context const & Context,
                                                    TBytes const Buffer )
{
    if ( GetLength( Buffer ) < GetBMAPHeaderLength() ) {
        throw EContextException( Context, _T( "Invalid BMAP length" ) );
    }
}
//---------------------------------------------------------------------------

void TCPIPProtocol::RaiseExceptionIfBMAPIsNotEQ( Context const & Context,
                                                 TBytes const LBuffer,
                                                 TBytes const RBuffer )
{
    RaiseExceptionIfBMAPIsNotValid( Context, LBuffer );
    RaiseExceptionIfBMAPIsNotValid( Context, RBuffer );
    if ( GetBMAPTransactionIdentifier( LBuffer ) != GetBMAPTransactionIdentifier( RBuffer ) ) {
        throw EContextException( Context, _T( "Invalid BMAP Transaction Identifier" ) );
    }

    BMAPProtocolType const LBMAPProtocol = GetBMAPProtocol( LBuffer );
    if ( LBMAPProtocol != GetBMAPProtocol( RBuffer ) || LBMAPProtocol ) {
        throw EContextException( Context, _T( "Invalid BMAP Protocol" ) );
    }
    if ( GetBMAPUnitIdentifier( LBuffer ) != GetBMAPUnitIdentifier( RBuffer ) ) {
        throw EContextException( Context, _T( "Invalid BMAP Unit Identifier" ) );
    }
}
//---------------------------------------------------------------------------

void TCPIPProtocol::RaiseExceptionIfReplyIsNotValid( Context const & Context,
                                                     TBytes const Buffer,
                                                     FunctionCode ExpectedFunctionCode )
{
    if ( GetLength( Buffer ) > 1 ) {
        FunctionCode const FnCode = GetFunctionCode( Buffer );
        if ( static_cast<int>( FnCode ) & 0x80 ) {
            RaiseStandardException( Context, GetExceptCode( Buffer ) );
        }
        else if ( FnCode != ExpectedFunctionCode ) {
            throw EContextException(
                Context,
                Format(
                    _T( "Invalid Function Code: expected 0x%.2X, read 0x%.2X" )
                  , ARRAYOFCONST( (
                        ( static_cast<int>( ExpectedFunctionCode ) & 0xFF ),
                        ( static_cast<int>( FnCode ) & 0xFF )
                    ) )
                )
            );
        }
    }
    else {
        throw EContextException( Context, _T( "reply is too short" ) );
    }
}
//---------------------------------------------------------------------------

FunctionCode TCPIPProtocol::GetFunctionCode( TBytes const Buffer )
{
    return FunctionCode( Buffer[MODBUS_TCP_IP_REPLY_FUNCTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

ExceptionCode TCPIPProtocol::GetExceptCode( TBytes const Buffer )
{
    return ExceptionCode( Buffer[MODBUS_TCP_IP_REPLY_EXCEPTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPDataLengthType TCPIPProtocol::GetDataLength( TBytes const Buffer )
{
    return Buffer[MODBUS_TCP_IP_REPLY_DATA_OFFSET];
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPTransactionIdType TCPIPProtocol::GetBMAPTransactionIdentifier(
                                                 TBytes const Buffer )
{
    int const Idx = MODBUS_TCP_IP_BMAP_TRANSACTION_ID_OFFSET;
    return ( static_cast<BMAPTransactionIdType>( Buffer[Idx] ) << 8 ) |
           ( static_cast<BMAPTransactionIdType>( Buffer[Idx + 1] ) & 0xFF );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPProtocolType TCPIPProtocol::GetBMAPProtocol(
                                                 TBytes const Buffer )
{
    const int Idx = MODBUS_TCP_IP_BMAP_PROTOCOL_OFFSET;
    return ( static_cast<BMAPProtocolType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<BMAPProtocolType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPDataLengthType TCPIPProtocol::GetBMAPDataLength(
                                                 TBytes const Buffer )
{
    const int Idx = MODBUS_TCP_IP_BMAP_DATA_LENGTH_OFFSET;
    return ( static_cast<BMAPDataLengthType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<BMAPDataLengthType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPUnitIdType TCPIPProtocol::GetBMAPUnitIdentifier(
                                                 TBytes const Buffer )
{
    return Buffer[MODBUS_TCP_IP_BMAP_UNIT_ID_OFFSET];
}
//---------------------------------------------------------------------------

int TCPIPProtocol::WriteBMAPHeader( TBytes & OutBuffer, int StartIdx,
                                    Context const & Context )
{
    /*-----------------------------------------------------------------------*/
    /*                                  BMAP                                 */
    /*-------------------------+-----+---------------------------------------*/
    /*        Field            | Len |          Description                  */
    /*-------------------------+-----+---------------------------------------*/
    /* Transaction Identifier  |   2 | Identification of a MODBUS            */
    /*                         |     | Request / Response transaction        */
    /*-------------------------+-----+---------------------------------------*/
    /* Protocol Identifier     |   2 | 0 = MODBUS protocol                   */
    /*-------------------------+-----+---------------------------------------*/
    /* Length                  |   2 |  Number of following bytes            */
    /*-------------------------+-----+---------------------------------------*/
    /* Unit Identifier         |   1 | Identification of a remote slave con- */
    /*                         |     | nected on a serial line or on other   */
    /*                         |     | buses                                 */
    /*-------------------------+-----+---------------------------------------*/

    BMAPTransactionIdType const TransactionId = Context.GetTransactionIdentifier();
    OutBuffer[StartIdx++] = ( TransactionId >> 8 ) & 0xFF;   // Transaction Identifier Lo
    OutBuffer[StartIdx++] = TransactionId & 0xFF;            // Transaction Identifier Hi

    OutBuffer[StartIdx++] = 0x00;   // Protocol Identifier Lo
    OutBuffer[StartIdx++] = 0x00;   // Protocol Identifier Hi

    BMAPDataLengthType const PayloadLength = GetLength( OutBuffer ) - 6;
    OutBuffer[StartIdx++] = ( PayloadLength >> 8 ) & 0xFF;   // Length Lo
    OutBuffer[StartIdx++] = PayloadLength & 0xFF;            // Length Hi

    OutBuffer[StartIdx++] = Context.GetSlaveAddr();   // Unit Identifier

    return StartIdx;
}
//---------------------------------------------------------------------------

int TCPIPProtocol::WriteAddressPointCountPair( TBytes & OutBuffer,
                                               int StartIdx,
                                               RegAddrType StartAddr,
                                               RegCountType PointCount )
{
    OutBuffer[StartIdx++] = ( StartAddr >> 8 ) & 0xFF;   // Starting Address Hi
    OutBuffer[StartIdx++] = StartAddr & 0xFF;            // Starting Address Lo
    OutBuffer[StartIdx++] = ( PointCount >> 8 ) & 0xFF;  // No. of Points Hi
    OutBuffer[StartIdx++] = PointCount & 0xFF;           // No. of Points Lo
    return StartIdx;
}
//---------------------------------------------------------------------------

int TCPIPProtocol::WriteData( TBytes & OutBuffer,
                              int StartIdx, RegAddrType Data )
{
    OutBuffer[StartIdx++] = ( Data >> 8 ) & 0xFF;   // Data Hi
    OutBuffer[StartIdx++] = Data & 0xFF;            // Data Lo
    return StartIdx;
}
//---------------------------------------------------------------------------

void TCPIPProtocol::CopyDataWord( Context const & Context, TBytes const Buffer,
                                  int BufferOffset, uint16_t* Data )
{
    int DataLength = GetPayloadLength( Context, Buffer, BufferOffset );
    for ( int Idx = BufferOffset + 1 ; DataLength ; DataLength -= 2 ) {
        *Data++ = ( (uint16_t)Buffer[Idx] << 8 ) | ( (uint16_t)Buffer[Idx + 1] & 0xFF );
        Idx += 2;
    }
}
//---------------------------------------------------------------------------

int TCPIPProtocol::GetPayloadLength( Context const & Context,
                                     TBytes const Buffer,
                                     int BufferOffset )
{
    int const DataLength = GetDataLength( Buffer );
    if ( ( GetLength( Buffer ) - 2 != DataLength ) ||
         ( ~( GetLength( Buffer ) - BufferOffset ) & 1 ) )
    {
        throw EContextException(
            Context,
            Format(
                _T( "Invalid received frame lenght %d" )
              , ARRAYOFCONST( (
                    DataLength
                ) )
            )
        );
    }
    return DataLength;
}
//---------------------------------------------------------------------------

String TCPIPProtocol::DoGetProtocolParamsStr() const
{
    return Format( _T( "%s:%u" ), ARRAYOFCONST( ( GetHost(), GetPort() ) ) );
}

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadCoilStatus
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadInputStatus
//---------------------------------------------------------------------------

void TCPIPProtocol::ReadRegisters( FunctionCode FnCode, Context const & Context,
                                   RegAddrType StartAddr, RegCountType PointCount,
                                   RegDataType* Data )
{
    // Send
    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + GetAddressPointCountPairLength() );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] = static_cast<RegDataType>( FnCode );
    Idx = WriteAddressPointCountPair( OutBuffer, Idx, StartAddr, PointCount );
    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    // Verifica BMAP di risposta
    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    // Verifica parametri di risposta
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FnCode );

    CopyDataWord( Context, ReplyBuffer, MODBUS_TCP_IP_REPLY_DATA_OFFSET, Data );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::DoReadHoldingRegisters( Context const & Context,
                                            RegAddrType StartAddr,
                                            RegCountType PointCount,
                                            RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _T( "ReadHoldingRegisters failed" ) );

    ReadRegisters( FunctionCode::ReadHoldingRegisters, Context, StartAddr,
                   PointCount, Data );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::DoReadInputRegisters( Context const & Context,
                                          RegAddrType StartAddr,
                                          RegCountType PointCount,
                                          RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _T( "ReadInputRegisters failed" ) );

    ReadRegisters( FunctionCode::ReadInputRegisters, Context, StartAddr,
                   PointCount, Data );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoForceSingleCoil
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoPresetSingleRegister
void TCPIPProtocol::DoPresetSingleRegister( Context const & Context,
                                            RegAddrType Addr, RegDataType Data )
{
    RaiseExceptionIfIsNotConnected( _T( "PresetSingleRegister failed" ) );

    // Send
    TBytes OutBuffer;

    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 2 + 2 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::PresetSingleRegister );

    Idx = WriteData( OutBuffer, Idx, Addr );
    Idx = WriteData( OutBuffer, Idx, Data );

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    // Verifica BMAP di risposta
    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    // Verifica parametri di risposta
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::PresetSingleRegister );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadExceptionStatus

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoDiagnostics

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoProgram484

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoPoll484

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoFetchCommEventCtr

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoFetchCommEventLog

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoProgramController

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoPollController

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoForceMultipleCoils

//---------------------------------------------------------------------------

void TCPIPProtocol::DoPresetMultipleRegisters( Context const & Context,
                                               RegAddrType StartAddr,
                                               RegCountType PointCount,
                                               RegDataType const * Data )
{
    RaiseExceptionIfIsNotConnected( _T( "PresetMultipleRegister failed" ) );

    // Send
    TBytes OutBuffer;

    SetLength(
        OutBuffer,
        GetBMAPHeaderLength() + GetAddressPointCountPairLength() +
        PointCount * sizeof( RegDataType ) + 2
    );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::PresetMultipleRegisters );
    Idx = WriteAddressPointCountPair( OutBuffer, Idx, StartAddr, PointCount );
    OutBuffer[Idx++] = PointCount * sizeof( RegDataType );

    for ( RegCountType DataIdx = 0 ; DataIdx < PointCount ; ++DataIdx ) {
        const RegDataType Reg = Data[DataIdx];
        OutBuffer[Idx++] = ( Reg >> 8 ) & 0xFF;   // Data Hi
        OutBuffer[Idx++] = Reg & 0xFF;            // Data Lo
    }

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    // Verifica BMAP di risposta
    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    // Verifica parametri di risposta
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::PresetMultipleRegisters );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReportSlave

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoProgram884_M84

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoResetCommLink

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadGeneralReference

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoWriteGeneralReference

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoMaskWrite4XRegister
void TCPIPProtocol::DoMaskWrite4XRegister( Context const & Context,
                                           RegAddrType Addr,
                                           RegDataType AndMask,
                                           RegDataType OrMask )
{
    RaiseExceptionIfIsNotConnected( _T( "MaskWrite4XRegister failed" ) );

    // Send
    TBytes OutBuffer;

    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 2 + 2 + 2 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::MaskWrite4XRegister );
    Idx = WriteData( OutBuffer, Idx, Addr );
    Idx = WriteData( OutBuffer, Idx, AndMask );
    Idx = WriteData( OutBuffer, Idx, OrMask );

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    // Verifica BMAP di risposta
    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    // Verifica parametri di risposta
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::MaskWrite4XRegister );

}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadWrite4XRegisters
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadFIFOQueue

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
