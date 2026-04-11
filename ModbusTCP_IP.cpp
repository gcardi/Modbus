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
    RaiseExceptionIfIsConnected( _D( "Unable to change host" ) );
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
    RaiseExceptionIfIsConnected( _D( "Unable to change port" ) );
    DoSetPort( Val );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::RaiseExceptionIfBMAPIsNotValid( Context const & Context,
                                                    TBytes const Buffer )
{
    if ( GetLength( Buffer ) < GetBMAPHeaderLength() ) {
        throw EContextException( Context, _D( "Invalid BMAP length" ) );
    }
}
//---------------------------------------------------------------------------

void TCPIPProtocol::RaiseExceptionIfBMAPDataLengthIsNotValid( Context const & Context,
                                                              BMAPDataLengthType DataLength )
{
    // MBAP length field covers Unit ID (1) + PDU. Minimum valid PDU is 2 bytes
    // (FC + 1 data byte), so minimum total is 3. Maximum Modbus PDU is 253 bytes,
    // so maximum MBAP length is 254.
    if ( DataLength < 3 || DataLength > 254 ) {
        throw EContextException( Context, _D( "MBAP length out of valid range" ) );
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
        throw EContextException( Context, _D( "Invalid BMAP Transaction Identifier" ) );
    }

    BMAPProtocolType const LBMAPProtocol = GetBMAPProtocol( LBuffer );
    if ( LBMAPProtocol != GetBMAPProtocol( RBuffer ) || LBMAPProtocol ) {
        throw EContextException( Context, _D( "Invalid BMAP Protocol" ) );
    }
    if ( GetBMAPUnitIdentifier( LBuffer ) != GetBMAPUnitIdentifier( RBuffer ) ) {
        throw EContextException( Context, _D( "Invalid BMAP Unit Identifier" ) );
    }
    RaiseExceptionIfBMAPDataLengthIsNotValid( Context, GetBMAPDataLength( RBuffer ) );
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
                    _D( "Invalid Function Code: expected 0x%.2X, read 0x%.2X" )
                  , ARRAYOFCONST( (
                        ( static_cast<int>( ExpectedFunctionCode ) & 0xFF ),
                        ( static_cast<int>( FnCode ) & 0xFF )
                    ) )
                )
            );
        }
    }
    else {
        throw EContextException( Context, _D( "reply is too short" ) );
    }
}
//---------------------------------------------------------------------------

FunctionCode TCPIPProtocol::GetFunctionCode( TBytes const Buffer ) noexcept
{
    return FunctionCode( Buffer[MODBUS_TCP_IP_REPLY_FUNCTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

ExceptionCode TCPIPProtocol::GetExceptCode( TBytes const Buffer ) noexcept
{
    return ExceptionCode( Buffer[MODBUS_TCP_IP_REPLY_EXCEPTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPDataLengthType TCPIPProtocol::GetDataLength( TBytes const Buffer ) noexcept
{
    return Buffer[MODBUS_TCP_IP_REPLY_DATA_OFFSET];
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPTransactionIdType TCPIPProtocol::GetBMAPTransactionIdentifier(
                                                 TBytes const Buffer ) noexcept
{
    int const Idx = MODBUS_TCP_IP_BMAP_TRANSACTION_ID_OFFSET;
    return ( static_cast<BMAPTransactionIdType>( Buffer[Idx] ) << 8 ) |
           ( static_cast<BMAPTransactionIdType>( Buffer[Idx + 1] ) & 0xFF );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPProtocolType TCPIPProtocol::GetBMAPProtocol(
                                                 TBytes const Buffer ) noexcept
{
    const int Idx = MODBUS_TCP_IP_BMAP_PROTOCOL_OFFSET;
    return ( static_cast<BMAPProtocolType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<BMAPProtocolType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPDataLengthType TCPIPProtocol::GetBMAPDataLength(
                                                 TBytes const Buffer ) noexcept
{
    const int Idx = MODBUS_TCP_IP_BMAP_DATA_LENGTH_OFFSET;
    return ( static_cast<BMAPDataLengthType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<BMAPDataLengthType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

TCPIPProtocol::BMAPUnitIdType TCPIPProtocol::GetBMAPUnitIdentifier(
                                                 TBytes const Buffer ) noexcept
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
                                               RegCountType PointCount ) noexcept
{
    OutBuffer[StartIdx++] = ( StartAddr >> 8 ) & 0xFF;   // Starting Address Hi
    OutBuffer[StartIdx++] = StartAddr & 0xFF;            // Starting Address Lo
    OutBuffer[StartIdx++] = ( PointCount >> 8 ) & 0xFF;  // No. of Points Hi
    OutBuffer[StartIdx++] = PointCount & 0xFF;           // No. of Points Lo
    return StartIdx;
}
//---------------------------------------------------------------------------

int TCPIPProtocol::WriteData( TBytes & OutBuffer,
                              int StartIdx, RegAddrType Data ) noexcept
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
                _D( "Invalid received frame lenght %d" )
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
    return Format( _D( "%s:%u" ), ARRAYOFCONST( ( GetHost(), GetPort() ) ) );
}

//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadCoilStatus
void TCPIPProtocol::ReadBits( FunctionCode FnCode, Context const & Context,
                              CoilAddrType StartAddr, CoilCountType PointCount,
                              CoilDataType* Data )
{
    if ( PointCount == 0 || PointCount > 2000 ) {
        throw EContextException( Context, _D( "Invalid point count" ) );
    }

    const uint8_t ExpectedByteCount =
        static_cast<uint8_t>( ( PointCount + 7 ) / 8 );

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

    // Validate BMAP and payload function code
    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FnCode );

    if ( GetLength( ReplyBuffer ) < 2 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    const uint8_t ByteCount = ReplyBuffer[1];
    if ( ByteCount != ExpectedByteCount || GetLength( ReplyBuffer ) != ByteCount + 2 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    for ( uint8_t I = 0; I < ByteCount; ++I ) {
        Data[I] = ReplyBuffer[2 + I];
    }
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadInputStatus
void TCPIPProtocol::DoReadCoilStatus( Context const & Context,
                                      CoilAddrType StartAddr,
                                      CoilCountType PointCount,
                                      CoilDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadCoilStatus failed" ) );

    ReadBits( FunctionCode::ReadCoilStatus, Context, StartAddr,
              PointCount, Data );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::DoReadInputStatus( Context const & Context,
                                       CoilAddrType StartAddr,
                                       CoilCountType PointCount,
                                       CoilDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadInputStatus failed" ) );

    ReadBits( FunctionCode::ReadInputStatus, Context, StartAddr,
              PointCount, Data );
}
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
    RaiseExceptionIfIsNotConnected( _D( "ReadHoldingRegisters failed" ) );

    ReadRegisters( FunctionCode::ReadHoldingRegisters, Context, StartAddr,
                   PointCount, Data );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::DoReadInputRegisters( Context const & Context,
                                          RegAddrType StartAddr,
                                          RegCountType PointCount,
                                          RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadInputRegisters failed" ) );

    ReadRegisters( FunctionCode::ReadInputRegisters, Context, StartAddr,
                   PointCount, Data );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoForceSingleCoil
void TCPIPProtocol::DoForceSingleCoil( Context const & Context,
                                       CoilAddrType Addr, bool Value )
{
    RaiseExceptionIfIsNotConnected( _D( "ForceSingleCoil failed" ) );

    // Send
    TBytes OutBuffer;

    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 2 + 2 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::ForceSingleCoil );
    Idx = WriteData( OutBuffer, Idx, Addr );
    Idx = WriteData( OutBuffer, Idx, static_cast<RegDataType>( Value ? 0xFF00 : 0x0000 ) );

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
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::ForceSingleCoil );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoPresetSingleRegister
void TCPIPProtocol::DoPresetSingleRegister( Context const & Context,
                                            RegAddrType Addr, RegDataType Data )
{
    RaiseExceptionIfIsNotConnected( _D( "PresetSingleRegister failed" ) );

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
ExceptionStatusDataType TCPIPProtocol::DoReadExceptionStatus(
                                           Context const & Context )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadExceptionStatus failed" ) );

    // Send: BMAP(7) + FC(1) — no additional payload
    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::ReadExceptionStatus );

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive BMAP header
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );

    // Receive payload: FC(1) + ExceptionStatus(1)
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    RaiseExceptionIfReplyIsNotValid(
        Context, ReplyBuffer, FunctionCode::ReadExceptionStatus
    );

    if ( GetLength( ReplyBuffer ) < 2 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    return static_cast<ExceptionStatusDataType>( ReplyBuffer[1] );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoDiagnostics
RegDataType TCPIPProtocol::DoDiagnostics( Context const & Context,
                                          DiagSubFnType SubFunction,
                                          RegDataType Data )
{
    RaiseExceptionIfIsNotConnected( _D( "Diagnostics failed" ) );

    // Send: BMAP(7) + FC(1) + SubFunction(2) + Data(2)
    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 2 + 2 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::Diagnostics );
    Idx = WriteData( OutBuffer, Idx, SubFunction );
    Idx = WriteData( OutBuffer, Idx, Data );

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive BMAP header
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );

    // Receive payload: FC(1) + SubFunction(2) + Data(2)
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    RaiseExceptionIfReplyIsNotValid(
        Context, ReplyBuffer, FunctionCode::Diagnostics
    );

    if ( GetLength( ReplyBuffer ) < 5 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    // Validate sub-function echo
    uint16_t const ReplySF =
        ( static_cast<uint16_t>( ReplyBuffer[1] ) << 8 ) |
        ( static_cast<uint16_t>( ReplyBuffer[2] ) & 0xFF );
    if ( ReplySF != SubFunction ) {
        throw EContextException( Context, _D( "Sub-function mismatch" ) );
    }

    // Extract returned data word
    return static_cast<RegDataType>(
        ( static_cast<uint16_t>( ReplyBuffer[3] ) << 8 ) |
        ( static_cast<uint16_t>( ReplyBuffer[4] ) & 0xFF )
    );
}

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
void TCPIPProtocol::DoForceMultipleCoils( Context const & Context,
                                          CoilAddrType StartAddr,
                                          CoilCountType PointCount,
                                          const CoilDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ForceMultipleCoils failed" ) );

    if ( PointCount == 0 || PointCount > 1968 ) {
        throw EContextException( Context, _D( "Invalid point count" ) );
    }

    const uint8_t ByteCount = static_cast<uint8_t>( ( PointCount + 7 ) / 8 );

    // Send
    TBytes OutBuffer;

    SetLength(
        OutBuffer,
        GetBMAPHeaderLength() + 1 + GetAddressPointCountPairLength() + 1 + ByteCount
    );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::ForceMultipleCoils );
    Idx = WriteAddressPointCountPair( OutBuffer, Idx, StartAddr, PointCount );
    OutBuffer[Idx++] = ByteCount;

    for ( uint8_t I = 0; I < ByteCount; ++I ) {
        OutBuffer[Idx++] = Data[I];
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
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::ForceMultipleCoils );
}
//---------------------------------------------------------------------------

void TCPIPProtocol::DoPresetMultipleRegisters( Context const & Context,
                                               RegAddrType StartAddr,
                                               RegCountType PointCount,
                                               RegDataType const * Data )
{
    RaiseExceptionIfIsNotConnected( _D( "PresetMultipleRegister failed" ) );

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
void TCPIPProtocol::DoReadGeneralReference( Context const & Context,
                                            const FileSubRequest* SubRequests,
                                            size_t SubReqCount,
                                            RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadGeneralReference failed" ) );

    // FC20 request PDU: FC(1) + ByteCount(1) + N * [RefType(1)+FileNo(2)+RecNo(2)+RecLen(2)]
    const size_t subReqBytes = SubReqCount * 7;

    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 1 + subReqBytes );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<uint8_t>( FunctionCode::ReadGeneralReference );
    OutBuffer[Idx++] = static_cast<uint8_t>( subReqBytes );

    for ( size_t i = 0; i < SubReqCount; ++i ) {
        OutBuffer[Idx++] = 0x06; // reference type
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].FileNumber >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].FileNumber & 0xFF );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordNumber >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordNumber & 0xFF );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordLength >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordLength & 0xFF );
    }

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive BMAP header
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );

    // Receive payload
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    RaiseExceptionIfReplyIsNotValid(
        Context, ReplyBuffer, FunctionCode::ReadGeneralReference
    );

    // Response: FC(1) + RespDataLen(1) + N * [SubRespLen(1) + RefType(1) + Data(RecLen*2)]
    if ( GetLength( ReplyBuffer ) < 2 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    int off = 2; // skip FC + RespDataLen
    RegDataType* dataOut = Data;
    for ( size_t i = 0; i < SubReqCount; ++i ) {
        if ( off + 2 > GetLength( ReplyBuffer ) ) {
            throw EContextException( Context, _D( "Truncated response" ) );
        }
        const uint8_t subRespLen = ReplyBuffer[off++];
        const uint8_t refType    = ReplyBuffer[off++];
        if ( refType != 0x06 ) {
            throw EContextException( Context, _D( "Invalid reference type" ) );
        }
        const size_t dataBytes = subRespLen - 1;
        if ( dataBytes != SubRequests[i].RecordLength * 2 ) {
            throw EContextException( Context, _D( "Sub-response length mismatch" ) );
        }
        for ( RecordLengthType r = 0; r < SubRequests[i].RecordLength; ++r ) {
            *dataOut++ = static_cast<RegDataType>(
                ( static_cast<uint16_t>( ReplyBuffer[off] ) << 8 ) |
                ( static_cast<uint16_t>( ReplyBuffer[off + 1] ) & 0xFF )
            );
            off += 2;
        }
    }
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoWriteGeneralReference
void TCPIPProtocol::DoWriteGeneralReference( Context const & Context,
                                             const FileSubRequest* SubRequests,
                                             size_t SubReqCount,
                                             const RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "WriteGeneralReference failed" ) );

    // FC21 request PDU: FC(1) + ByteCount(1)
    //   + N * [RefType(1)+FileNo(2)+RecNo(2)+RecLen(2)+Data(RecLen*2)]
    size_t totalRegs = 0;
    for ( size_t i = 0; i < SubReqCount; ++i )
        totalRegs += SubRequests[i].RecordLength;

    const size_t reqBytes = SubReqCount * 7 + totalRegs * 2;

    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 1 + reqBytes );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<uint8_t>( FunctionCode::WriteGeneralReference );
    OutBuffer[Idx++] = static_cast<uint8_t>( reqBytes );

    const RegDataType* dataIn = Data;
    for ( size_t i = 0; i < SubReqCount; ++i ) {
        OutBuffer[Idx++] = 0x06; // reference type
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].FileNumber >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].FileNumber & 0xFF );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordNumber >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordNumber & 0xFF );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordLength >> 8 );
        OutBuffer[Idx++] = static_cast<uint8_t>( SubRequests[i].RecordLength & 0xFF );
        for ( RecordLengthType r = 0; r < SubRequests[i].RecordLength; ++r ) {
            const RegDataType Reg = *dataIn++;
            OutBuffer[Idx++] = static_cast<uint8_t>( ( Reg >> 8 ) & 0xFF );
            OutBuffer[Idx++] = static_cast<uint8_t>( Reg & 0xFF );
        }
    }

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive BMAP header
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );

    // Receive payload (response is an echo)
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    RaiseExceptionIfReplyIsNotValid(
        Context, ReplyBuffer, FunctionCode::WriteGeneralReference
    );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoMaskWrite4XRegister
void TCPIPProtocol::DoMaskWrite4XRegister( Context const & Context,
                                           RegAddrType Addr,
                                           RegDataType AndMask,
                                           RegDataType OrMask )
{
    RaiseExceptionIfIsNotConnected( _D( "MaskWrite4XRegister failed" ) );

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
void TCPIPProtocol::DoReadWrite4XRegisters( Context const & Context,
                                            RegAddrType ReadStartAddr,
                                            RegCountType ReadPointCount,
                                            RegDataType* ReadData,
                                            RegAddrType WriteStartAddr,
                                            RegCountType WritePointCount,
                                            const RegDataType* WriteData )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadWrite4XRegisters failed" ) );

    // Send
    // PDU: FC(1) + ReadAddr(2) + ReadCount(2) + WriteAddr(2) + WriteCount(2)
    //      + WriteByteCount(1) + WriteValues(WritePointCount * 2)
    TBytes OutBuffer;

    SetLength(
        OutBuffer,
        GetBMAPHeaderLength() + 1 + 2 + 2 + 2 + 2 + 1 +
        WritePointCount * sizeof( RegDataType )
    );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::ReadWrite4XRegisters );
    Idx = WriteAddressPointCountPair( OutBuffer, Idx, ReadStartAddr, ReadPointCount );
    Idx = WriteAddressPointCountPair( OutBuffer, Idx, WriteStartAddr, WritePointCount );
    OutBuffer[Idx++] =
        static_cast<uint8_t>( WritePointCount * sizeof( RegDataType ) );

    for ( RegCountType DataIdx = 0; DataIdx < WritePointCount; ++DataIdx ) {
        const RegDataType Reg = WriteData[DataIdx];
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
    RaiseExceptionIfReplyIsNotValid( Context, ReplyBuffer, FunctionCode::ReadWrite4XRegisters );

    CopyDataWord( Context, ReplyBuffer, MODBUS_TCP_IP_REPLY_DATA_OFFSET, ReadData );
}
//---------------------------------------------------------------------------

//    TCPIPProtocol::DoReadFIFOQueue
FIFOCountType TCPIPProtocol::DoReadFIFOQueue( Context const & Context,
                                              FIFOAddrType FIFOAddr,
                                              RegDataType* Data )
{
    RaiseExceptionIfIsNotConnected( _D( "ReadFIFOQueue failed" ) );

    // Send: BMAP(7) + FC(1) + FIFOPointerAddr(2)
    TBytes OutBuffer;
    SetLength( OutBuffer, GetBMAPHeaderLength() + 1 + 2 );
    int Idx = WriteBMAPHeader( OutBuffer, 0, Context );
    OutBuffer[Idx++] =
        static_cast<RegDataType>( FunctionCode::ReadFIFOQueue );
    Idx = WriteData( OutBuffer, Idx, FIFOAddr );

    DoInputBufferClear();
    DoWrite( OutBuffer );

    // Receive BMAP header
    TBytes ReplyBMAPBuffer;
    SetLength( ReplyBMAPBuffer, GetBMAPHeaderLength() );
    DoRead( ReplyBMAPBuffer, GetLength( ReplyBMAPBuffer ) );

    RaiseExceptionIfBMAPIsNotEQ( Context, OutBuffer, ReplyBMAPBuffer );

    // Receive payload
    TBytes ReplyBuffer;
    SetLength( ReplyBuffer, GetBMAPDataLength( ReplyBMAPBuffer ) - 1 );
    DoRead( ReplyBuffer, GetLength( ReplyBuffer ) );

    RaiseExceptionIfReplyIsNotValid(
        Context, ReplyBuffer, FunctionCode::ReadFIFOQueue
    );

    // Response: FC(1) + ByteCount(2) + FIFOCount(2) + FIFOValues(FIFOCount*2)
    if ( GetLength( ReplyBuffer ) < 5 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    uint16_t const ByteCount =
        ( static_cast<uint16_t>( ReplyBuffer[1] ) << 8 ) |
        ( static_cast<uint16_t>( ReplyBuffer[2] ) & 0xFF );

    uint16_t const FIFOCount =
        ( static_cast<uint16_t>( ReplyBuffer[3] ) << 8 ) |
        ( static_cast<uint16_t>( ReplyBuffer[4] ) & 0xFF );

    if ( FIFOCount > 31 ) {
        throw EContextException( Context, _D( "FIFO count exceeds maximum (31)" ) );
    }

    if ( ByteCount != 2 + FIFOCount * 2 ) {
        throw EContextException( Context, _D( "Byte count mismatch" ) );
    }

    if ( GetLength( ReplyBuffer ) != 5 + FIFOCount * 2 ) {
        throw EContextException( Context, _D( "Invalid reply length" ) );
    }

    // Extract FIFO register values
    for ( FIFOCountType I = 0; I < FIFOCount; ++I ) {
        int const Off = 5 + I * 2;
        Data[I] = static_cast<RegDataType>(
            ( static_cast<uint16_t>( ReplyBuffer[Off] ) << 8 ) |
            ( static_cast<uint16_t>( ReplyBuffer[Off + 1] ) & 0xFF )
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
