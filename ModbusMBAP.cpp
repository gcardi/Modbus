//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include "ModbusMBAP.h"

#define  MODBUS_MBAP_TRANSACTION_ID_OFFSET  0
#define  MODBUS_MBAP_PROTOCOL_OFFSET        2
#define  MODBUS_MBAP_DATA_LENGTH_OFFSET     4
#define  MODBUS_MBAP_UNIT_ID_OFFSET         6

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace MBAP {
//---------------------------------------------------------------------------

int WriteHeader( TBytes & OutBuffer, int StartIdx, Context const & Context )
{
    /*-----------------------------------------------------------------------*/
    /*                                  MBAP                                 */
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

    TransactionIdType const TransactionId = Context.GetTransactionIdentifier();
    OutBuffer[StartIdx++] = ( TransactionId >> 8 ) & 0xFF;   // Transaction Identifier Hi
    OutBuffer[StartIdx++] = TransactionId & 0xFF;            // Transaction Identifier Lo

    OutBuffer[StartIdx++] = 0x00;   // Protocol Identifier Hi
    OutBuffer[StartIdx++] = 0x00;   // Protocol Identifier Lo

    DataLengthType const PayloadLength = static_cast<DataLengthType>( OutBuffer.Length - 6 );
    OutBuffer[StartIdx++] = ( PayloadLength >> 8 ) & 0xFF;   // Length Hi
    OutBuffer[StartIdx++] = PayloadLength & 0xFF;            // Length Lo

    OutBuffer[StartIdx++] = Context.GetSlaveAddr();   // Unit Identifier

    return StartIdx;
}
//---------------------------------------------------------------------------

TransactionIdType GetTransactionIdentifier( TBytes const Buffer ) noexcept
{
    int const Idx = MODBUS_MBAP_TRANSACTION_ID_OFFSET;
    return ( static_cast<TransactionIdType>( Buffer[Idx] ) << 8 ) |
           ( static_cast<TransactionIdType>( Buffer[Idx + 1] ) & 0xFF );
}
//---------------------------------------------------------------------------

ProtocolIdType GetProtocolIdentifier( TBytes const Buffer ) noexcept
{
    int const Idx = MODBUS_MBAP_PROTOCOL_OFFSET;
    return ( static_cast<ProtocolIdType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<ProtocolIdType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

DataLengthType GetDataLength( TBytes const Buffer ) noexcept
{
    int const Idx = MODBUS_MBAP_DATA_LENGTH_OFFSET;
    return ( static_cast<DataLengthType>( Buffer[Idx] << 8 ) ) |
           ( static_cast<DataLengthType>( Buffer[Idx + 1] & 0xFF ) );
}
//---------------------------------------------------------------------------

UnitIdType GetUnitIdentifier( TBytes const Buffer ) noexcept
{
    return Buffer[MODBUS_MBAP_UNIT_ID_OFFSET];
}
//---------------------------------------------------------------------------

void RaiseExceptionIfHeaderIsNotValid( Context const & Context, TBytes const Buffer )
{
    if ( Buffer.Length < HeaderLength ) {
        throw EContextException( Context, _D( "Invalid MBAP header length" ) );
    }
}
//---------------------------------------------------------------------------

void RaiseExceptionIfDataLengthIsNotValid( Context const & Context, DataLengthType DataLength )
{
    // MBAP length field covers Unit ID (1) + PDU. Minimum valid PDU is 2 bytes
    // (FC + 1 data byte), so minimum total is 3. Maximum Modbus PDU is 253 bytes,
    // so maximum MBAP length is 254.
    if ( DataLength < 3 || DataLength > 254 ) {
        throw EContextException( Context, _D( "MBAP length out of valid range" ) );
    }
}
//---------------------------------------------------------------------------

void RaiseExceptionIfHeadersAreNotEQ( Context const & Context, TBytes const LBuffer, TBytes const RBuffer )
{
    RaiseExceptionIfHeaderIsNotValid( Context, LBuffer );
    RaiseExceptionIfHeaderIsNotValid( Context, RBuffer );
    if ( GetTransactionIdentifier( LBuffer ) != GetTransactionIdentifier( RBuffer ) ) {
        throw EContextException( Context, _D( "Invalid MBAP Transaction Identifier" ) );
    }

    ProtocolIdType const LProtocol = GetProtocolIdentifier( LBuffer );
    if ( LProtocol != GetProtocolIdentifier( RBuffer ) || LProtocol ) {
        throw EContextException( Context, _D( "Invalid MBAP Protocol" ) );
    }
    if ( GetUnitIdentifier( LBuffer ) != GetUnitIdentifier( RBuffer ) ) {
        throw EContextException( Context, _D( "Invalid MBAP Unit Identifier" ) );
    }
    RaiseExceptionIfDataLengthIsNotValid( Context, GetDataLength( RBuffer ) );
}

//---------------------------------------------------------------------------
}; // End of namespace MBAP
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
