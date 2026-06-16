//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include "ModbusPDU.h"

#define  MODBUS_PDU_FUNCTION_CODE_OFFSET   0
#define  MODBUS_PDU_EXCEPTION_CODE_OFFSET  1
#define  MODBUS_PDU_BYTE_COUNT_OFFSET      1

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace PDU {
//---------------------------------------------------------------------------

FunctionCode GetFunctionCode( TBytes const Buffer ) noexcept
{
    return FunctionCode( Buffer[MODBUS_PDU_FUNCTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

ExceptionCode GetExceptionCode( TBytes const Buffer ) noexcept
{
    return ExceptionCode( Buffer[MODBUS_PDU_EXCEPTION_CODE_OFFSET] );
}
//---------------------------------------------------------------------------

uint8_t GetByteCount( TBytes const Buffer ) noexcept
{
    return Buffer[MODBUS_PDU_BYTE_COUNT_OFFSET];
}
//---------------------------------------------------------------------------

void RaiseExceptionIfReplyIsNotValid( Context const & Context,
                                      TBytes const Buffer,
                                      FunctionCode ExpectedFunctionCode )
{
    if ( Buffer.Length > 1 ) {
        FunctionCode const FnCode = GetFunctionCode( Buffer );
        if ( static_cast<int>( FnCode ) & 0x80 ) {
            RaiseStandardException( Context, GetExceptionCode( Buffer ) );
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

int WriteAddressPointCountPair( TBytes & OutBuffer, int StartIdx,
                                RegAddrType StartAddr, RegCountType PointCount ) noexcept
{
    OutBuffer[StartIdx++] = ( StartAddr >> 8 ) & 0xFF;   // Starting Address Hi
    OutBuffer[StartIdx++] = StartAddr & 0xFF;            // Starting Address Lo
    OutBuffer[StartIdx++] = ( PointCount >> 8 ) & 0xFF;  // No. of Points Hi
    OutBuffer[StartIdx++] = PointCount & 0xFF;           // No. of Points Lo
    return StartIdx;
}
//---------------------------------------------------------------------------

int WriteWord( TBytes & OutBuffer, int StartIdx, uint16_t Data ) noexcept
{
    OutBuffer[StartIdx++] = ( Data >> 8 ) & 0xFF;   // Data Hi
    OutBuffer[StartIdx++] = Data & 0xFF;            // Data Lo
    return StartIdx;
}
//---------------------------------------------------------------------------

int GetPayloadLength( Context const & Context, TBytes const Buffer, int BufferOffset )
{
    int const ByteCount = GetByteCount( Buffer );
    if ( ( Buffer.Length - 2 != ByteCount ) ||
         ( ~( Buffer.Length - BufferOffset ) & 1 ) )
    {
        throw EContextException(
            Context,
            Format(
                _D( "Invalid received frame lenght %d" )
              , ARRAYOFCONST( (
                    ByteCount
                ) )
            )
        );
    }
    return ByteCount;
}
//---------------------------------------------------------------------------

void CopyDataWords( Context const & Context, TBytes const Buffer,
                    int BufferOffset, uint16_t* Data )
{
    int DataLength = GetPayloadLength( Context, Buffer, BufferOffset );
    for ( int Idx = BufferOffset + 1 ; DataLength ; DataLength -= 2 ) {
        *Data++ = ( (uint16_t)Buffer[Idx] << 8 ) | ( (uint16_t)Buffer[Idx + 1] & 0xFF );
        Idx += 2;
    }
}

//---------------------------------------------------------------------------
}; // End of namespace PDU
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
