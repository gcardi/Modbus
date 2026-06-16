//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include <algorithm>
#include <cstring>

#include "ModbusServer.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------
// RequestHandler — default implementations return IllegalFunction
//---------------------------------------------------------------------------

std::optional<ExceptionCode> RequestHandler::OnReadCoilStatus(
    CoilAddrType, CoilCountType, CoilDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadInputStatus(
    CoilAddrType, CoilCountType, CoilDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadHoldingRegisters(
    RegAddrType, RegCountType, RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadInputRegisters(
    RegAddrType, RegCountType, RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnForceSingleCoil(
    CoilAddrType, bool )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnPresetSingleRegister(
    RegAddrType, RegDataType )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadExceptionStatus(
    ExceptionStatusDataType& )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnDiagnostics(
    DiagSubFnType, RegDataType, RegDataType& )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnForceMultipleCoils(
    CoilAddrType, CoilCountType, const CoilDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnPresetMultipleRegisters(
    RegAddrType, RegCountType, const RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadGeneralReference(
    const FileSubRequest*, size_t, RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnWriteGeneralReference(
    const FileSubRequest*, size_t, const RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnMaskWrite4XRegister(
    RegAddrType, RegDataType, RegDataType )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadWrite4XRegisters(
    RegAddrType, RegCountType, RegDataType*,
    RegAddrType, RegCountType, const RegDataType* )
{
    return ExceptionCode::IllegalFunction;
}

std::optional<ExceptionCode> RequestHandler::OnReadFIFOQueue(
    FIFOAddrType, RegDataType*, FIFOCountType& )
{
    return ExceptionCode::IllegalFunction;
}

//---------------------------------------------------------------------------
// Protocol
//---------------------------------------------------------------------------

Protocol::Protocol( RequestHandler& Handler )
    : handler_( Handler )
{}

//---------------------------------------------------------------------------
// Protocol — internal helpers shared by all transports
//---------------------------------------------------------------------------

uint16_t Protocol::Get16( const uint8_t* p ) noexcept
{
    return static_cast<uint16_t>( ( p[0] << 8 ) | p[1] );
}

void Protocol::Put16( uint8_t* p, uint16_t v ) noexcept
{
    p[0] = static_cast<uint8_t>( v >> 8 );
    p[1] = static_cast<uint8_t>( v & 0xFF );
}

std::vector<uint8_t> Protocol::MakeErrorPDU( uint8_t FC, ExceptionCode Code ) const
{
    return { static_cast<uint8_t>( FC | 0x80 ),
             static_cast<uint8_t>( Code ) };
}

//---------------------------------------------------------------------------
// Protocol::DispatchRequest — routes by FC to the appropriate HandleFCxx
//---------------------------------------------------------------------------

std::vector<uint8_t> Protocol::DispatchRequest( uint8_t FC,
                                                  const uint8_t* Data, int DataLen )
{
    switch ( FC ) {
        case 0x01: return HandleFC01( Data, DataLen );
        case 0x02: return HandleFC02( Data, DataLen );
        case 0x03: return HandleFC03( Data, DataLen );
        case 0x04: return HandleFC04( Data, DataLen );
        case 0x05: return HandleFC05( Data, DataLen );
        case 0x06: return HandleFC06( Data, DataLen );
        case 0x07: return HandleFC07( Data, DataLen );
        case 0x08: return HandleFC08( Data, DataLen );
        case 0x0F: return HandleFC15( Data, DataLen );
        case 0x10: return HandleFC16( Data, DataLen );
        case 0x14: return HandleFC20( Data, DataLen );
        case 0x15: return HandleFC21( Data, DataLen );
        case 0x16: return HandleFC22( Data, DataLen );
        case 0x17: return HandleFC23( Data, DataLen );
        case 0x18: return HandleFC24( Data, DataLen );
        default:   return MakeErrorPDU( FC, ExceptionCode::IllegalFunction );
    }
}

//---------------------------------------------------------------------------
// Protocol — FC handlers
//---------------------------------------------------------------------------

std::vector<uint8_t> Protocol::HandleFC01( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x01, ExceptionCode::IllegalDataValue );
    CoilAddrType  addr  = Get16( d );
    CoilCountType count = Get16( d + 2 );
    if ( count == 0 || count > 2000 ) return MakeErrorPDU( 0x01, ExceptionCode::IllegalDataValue );

    const uint8_t byteCount = static_cast<uint8_t>( ( count + 7 ) / 8 );
    std::vector<uint8_t> data( byteCount, 0 );

    auto ex = handler_.OnReadCoilStatus( addr, count, data.data() );
    if ( ex ) return MakeErrorPDU( 0x01, *ex );

    std::vector<uint8_t> pdu;
    pdu.reserve( 2 + byteCount );
    pdu.push_back( 0x01 );
    pdu.push_back( byteCount );
    pdu.insert( pdu.end(), data.begin(), data.end() );
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC02( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x02, ExceptionCode::IllegalDataValue );
    CoilAddrType  addr  = Get16( d );
    CoilCountType count = Get16( d + 2 );
    if ( count == 0 || count > 2000 ) return MakeErrorPDU( 0x02, ExceptionCode::IllegalDataValue );

    const uint8_t byteCount = static_cast<uint8_t>( ( count + 7 ) / 8 );
    std::vector<uint8_t> data( byteCount, 0 );

    auto ex = handler_.OnReadInputStatus( addr, count, data.data() );
    if ( ex ) return MakeErrorPDU( 0x02, *ex );

    std::vector<uint8_t> pdu;
    pdu.reserve( 2 + byteCount );
    pdu.push_back( 0x02 );
    pdu.push_back( byteCount );
    pdu.insert( pdu.end(), data.begin(), data.end() );
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC03( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x03, ExceptionCode::IllegalDataValue );
    RegAddrType  addr  = Get16( d );
    RegCountType count = Get16( d + 2 );
    if ( count == 0 || count > 125 ) return MakeErrorPDU( 0x03, ExceptionCode::IllegalDataValue );

    std::vector<uint16_t> data( count, 0 );
    auto ex = handler_.OnReadHoldingRegisters( addr, count, data.data() );
    if ( ex ) return MakeErrorPDU( 0x03, *ex );

    std::vector<uint8_t> pdu;
    pdu.reserve( 2 + count * 2 );
    pdu.push_back( 0x03 );
    pdu.push_back( static_cast<uint8_t>( count * 2 ) );
    for ( uint16_t v : data ) {
        pdu.push_back( static_cast<uint8_t>( v >> 8 ) );
        pdu.push_back( static_cast<uint8_t>( v & 0xFF ) );
    }
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC04( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x04, ExceptionCode::IllegalDataValue );
    RegAddrType  addr  = Get16( d );
    RegCountType count = Get16( d + 2 );
    if ( count == 0 || count > 125 ) return MakeErrorPDU( 0x04, ExceptionCode::IllegalDataValue );

    std::vector<uint16_t> data( count, 0 );
    auto ex = handler_.OnReadInputRegisters( addr, count, data.data() );
    if ( ex ) return MakeErrorPDU( 0x04, *ex );

    std::vector<uint8_t> pdu;
    pdu.reserve( 2 + count * 2 );
    pdu.push_back( 0x04 );
    pdu.push_back( static_cast<uint8_t>( count * 2 ) );
    for ( uint16_t v : data ) {
        pdu.push_back( static_cast<uint8_t>( v >> 8 ) );
        pdu.push_back( static_cast<uint8_t>( v & 0xFF ) );
    }
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC05( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x05, ExceptionCode::IllegalDataValue );
    CoilAddrType addr  = Get16( d );
    uint16_t     value = Get16( d + 2 );
    if ( value != 0xFF00 && value != 0x0000 )
        return MakeErrorPDU( 0x05, ExceptionCode::IllegalDataValue );

    auto ex = handler_.OnForceSingleCoil( addr, value == 0xFF00 );
    if ( ex ) return MakeErrorPDU( 0x05, *ex );

    // Echo request
    return { 0x05, d[0], d[1], d[2], d[3] };
}

std::vector<uint8_t> Protocol::HandleFC06( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x06, ExceptionCode::IllegalDataValue );
    RegAddrType  addr  = Get16( d );
    RegDataType  value = Get16( d + 2 );

    auto ex = handler_.OnPresetSingleRegister( addr, value );
    if ( ex ) return MakeErrorPDU( 0x06, *ex );

    // Echo request
    return { 0x06, d[0], d[1], d[2], d[3] };
}

std::vector<uint8_t> Protocol::HandleFC07( const uint8_t* /*d*/, int /*len*/ )
{
    ExceptionStatusDataType status = 0;
    auto ex = handler_.OnReadExceptionStatus( status );
    if ( ex ) return MakeErrorPDU( 0x07, *ex );

    return { 0x07, status };
}

std::vector<uint8_t> Protocol::HandleFC08( const uint8_t* d, int len )
{
    if ( len < 4 ) return MakeErrorPDU( 0x08, ExceptionCode::IllegalDataValue );
    DiagSubFnType subFn = Get16( d );
    RegDataType   data  = Get16( d + 2 );
    RegDataType   reply = 0;

    auto ex = handler_.OnDiagnostics( subFn, data, reply );
    if ( ex ) return MakeErrorPDU( 0x08, *ex );

    std::vector<uint8_t> pdu( 5 );
    pdu[0] = 0x08;
    Put16( pdu.data() + 1, subFn );
    Put16( pdu.data() + 3, reply );
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC15( const uint8_t* d, int len )
{
    if ( len < 5 ) return MakeErrorPDU( 0x0F, ExceptionCode::IllegalDataValue );
    CoilAddrType  addr      = Get16( d );
    CoilCountType count     = Get16( d + 2 );
    uint8_t       byteCount = d[4];
    if ( count == 0 || count > 1968 )        return MakeErrorPDU( 0x0F, ExceptionCode::IllegalDataValue );
    if ( byteCount != ( count + 7 ) / 8 )   return MakeErrorPDU( 0x0F, ExceptionCode::IllegalDataValue );
    if ( len < 5 + static_cast<int>( byteCount ) ) return MakeErrorPDU( 0x0F, ExceptionCode::IllegalDataValue );

    auto ex = handler_.OnForceMultipleCoils( addr, count, d + 5 );
    if ( ex ) return MakeErrorPDU( 0x0F, *ex );

    // Response: FC + StartAddr(2) + Count(2)
    return { 0x0F, d[0], d[1], d[2], d[3] };
}

std::vector<uint8_t> Protocol::HandleFC16( const uint8_t* d, int len )
{
    if ( len < 5 ) return MakeErrorPDU( 0x10, ExceptionCode::IllegalDataValue );
    RegAddrType  addr      = Get16( d );
    RegCountType count     = Get16( d + 2 );
    uint8_t      byteCount = d[4];
    if ( count == 0 || count > 123 )                return MakeErrorPDU( 0x10, ExceptionCode::IllegalDataValue );
    if ( byteCount != count * 2 )                   return MakeErrorPDU( 0x10, ExceptionCode::IllegalDataValue );
    if ( len < 5 + static_cast<int>( byteCount ) ) return MakeErrorPDU( 0x10, ExceptionCode::IllegalDataValue );

    std::vector<uint16_t> data( count );
    for ( int i = 0; i < count; ++i )
        data[i] = Get16( d + 5 + i * 2 );

    auto ex = handler_.OnPresetMultipleRegisters( addr, count, data.data() );
    if ( ex ) return MakeErrorPDU( 0x10, *ex );

    return { 0x10, d[0], d[1], d[2], d[3] };
}

std::vector<uint8_t> Protocol::HandleFC20( const uint8_t* d, int len )
{
    if ( len < 1 ) return MakeErrorPDU( 0x14, ExceptionCode::IllegalDataValue );
    uint8_t byteCount = d[0];
    if ( byteCount < 7 || byteCount > static_cast<uint8_t>( len - 1 ) )
        return MakeErrorPDU( 0x14, ExceptionCode::IllegalDataValue );

    std::vector<FileSubRequest> subReqs;
    size_t totalWords = 0;
    int off = 1;
    while ( off + 7 <= 1 + static_cast<int>( byteCount ) ) {
        uint8_t refType = d[off++];
        if ( refType != 0x06 ) return MakeErrorPDU( 0x14, ExceptionCode::IllegalDataValue );
        FileSubRequest sr;
        sr.FileNumber   = Get16( d + off ); off += 2;
        sr.RecordNumber = Get16( d + off ); off += 2;
        sr.RecordLength = Get16( d + off ); off += 2;
        subReqs.push_back( sr );
        totalWords += sr.RecordLength;
    }

    std::vector<uint16_t> data( totalWords, 0 );
    auto ex = handler_.OnReadGeneralReference( subReqs.data(), subReqs.size(), data.data() );
    if ( ex ) return MakeErrorPDU( 0x14, *ex );

    std::vector<uint8_t> pdu;
    pdu.push_back( 0x14 );
    pdu.push_back( 0 ); // placeholder for RespDataLen

    size_t dataOff = 0;
    for ( const auto& sr : subReqs ) {
        uint8_t subRespLen = static_cast<uint8_t>( 1 + sr.RecordLength * 2 );
        pdu.push_back( subRespLen );
        pdu.push_back( 0x06 ); // reference type
        for ( uint16_t r = 0; r < sr.RecordLength; ++r ) {
            uint16_t v = data[dataOff++];
            pdu.push_back( static_cast<uint8_t>( v >> 8 ) );
            pdu.push_back( static_cast<uint8_t>( v & 0xFF ) );
        }
    }
    pdu[1] = static_cast<uint8_t>( pdu.size() - 2 );
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC21( const uint8_t* d, int len )
{
    if ( len < 1 ) return MakeErrorPDU( 0x15, ExceptionCode::IllegalDataValue );
    uint8_t byteCount = d[0];
    if ( byteCount < 7 || byteCount > static_cast<uint8_t>( len - 1 ) )
        return MakeErrorPDU( 0x15, ExceptionCode::IllegalDataValue );

    std::vector<FileSubRequest> subReqs;
    std::vector<uint16_t>       data;
    int off = 1;
    while ( off + 7 <= 1 + static_cast<int>( byteCount ) ) {
        uint8_t refType = d[off++];
        if ( refType != 0x06 ) return MakeErrorPDU( 0x15, ExceptionCode::IllegalDataValue );
        FileSubRequest sr;
        sr.FileNumber   = Get16( d + off ); off += 2;
        sr.RecordNumber = Get16( d + off ); off += 2;
        sr.RecordLength = Get16( d + off ); off += 2;
        if ( off + sr.RecordLength * 2 > 1 + static_cast<int>( byteCount ) )
            return MakeErrorPDU( 0x15, ExceptionCode::IllegalDataValue );
        for ( uint16_t r = 0; r < sr.RecordLength; ++r ) {
            data.push_back( Get16( d + off ) );
            off += 2;
        }
        subReqs.push_back( sr );
    }

    auto ex = handler_.OnWriteGeneralReference( subReqs.data(), subReqs.size(), data.data() );
    if ( ex ) return MakeErrorPDU( 0x15, *ex );

    // Response is echo of request
    std::vector<uint8_t> pdu;
    pdu.push_back( 0x15 );
    pdu.insert( pdu.end(), d, d + 1 + byteCount );
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC22( const uint8_t* d, int len )
{
    if ( len < 6 ) return MakeErrorPDU( 0x16, ExceptionCode::IllegalDataValue );
    RegAddrType  addr    = Get16( d );
    RegDataType  andMask = Get16( d + 2 );
    RegDataType  orMask  = Get16( d + 4 );

    auto ex = handler_.OnMaskWrite4XRegister( addr, andMask, orMask );
    if ( ex ) return MakeErrorPDU( 0x16, *ex );

    // Echo request
    return { 0x16, d[0], d[1], d[2], d[3], d[4], d[5] };
}

std::vector<uint8_t> Protocol::HandleFC23( const uint8_t* d, int len )
{
    if ( len < 9 ) return MakeErrorPDU( 0x17, ExceptionCode::IllegalDataValue );
    RegAddrType  rAddr  = Get16( d );
    RegCountType rCount = Get16( d + 2 );
    RegAddrType  wAddr  = Get16( d + 4 );
    RegCountType wCount = Get16( d + 6 );
    uint8_t      wBytes = d[8];
    if ( rCount == 0 || rCount > 125 )              return MakeErrorPDU( 0x17, ExceptionCode::IllegalDataValue );
    if ( wCount == 0 || wCount > 121 )              return MakeErrorPDU( 0x17, ExceptionCode::IllegalDataValue );
    if ( wBytes != wCount * 2 )                     return MakeErrorPDU( 0x17, ExceptionCode::IllegalDataValue );
    if ( len < 9 + static_cast<int>( wBytes ) )    return MakeErrorPDU( 0x17, ExceptionCode::IllegalDataValue );

    std::vector<uint16_t> writeData( wCount );
    for ( int i = 0; i < wCount; ++i )
        writeData[i] = Get16( d + 9 + i * 2 );

    std::vector<uint16_t> readData( rCount, 0 );
    auto ex = handler_.OnReadWrite4XRegisters(
        rAddr, rCount, readData.data(),
        wAddr, wCount, writeData.data() );
    if ( ex ) return MakeErrorPDU( 0x17, *ex );

    std::vector<uint8_t> pdu;
    pdu.reserve( 2 + rCount * 2 );
    pdu.push_back( 0x17 );
    pdu.push_back( static_cast<uint8_t>( rCount * 2 ) );
    for ( uint16_t v : readData ) {
        pdu.push_back( static_cast<uint8_t>( v >> 8 ) );
        pdu.push_back( static_cast<uint8_t>( v & 0xFF ) );
    }
    return pdu;
}

std::vector<uint8_t> Protocol::HandleFC24( const uint8_t* d, int len )
{
    if ( len < 2 ) return MakeErrorPDU( 0x18, ExceptionCode::IllegalDataValue );
    FIFOAddrType  addr  = Get16( d );
    RegDataType   data[31] = {};
    FIFOCountType count = 0;

    auto ex = handler_.OnReadFIFOQueue( addr, data, count );
    if ( ex ) return MakeErrorPDU( 0x18, *ex );

    if ( count > 31 ) return MakeErrorPDU( 0x18, ExceptionCode::IllegalDataValue );

    uint16_t byteCount = static_cast<uint16_t>( 2 + count * 2 );
    std::vector<uint8_t> pdu;
    pdu.reserve( 5 + count * 2 );
    pdu.push_back( 0x18 );
    pdu.push_back( static_cast<uint8_t>( byteCount >> 8 ) );
    pdu.push_back( static_cast<uint8_t>( byteCount & 0xFF ) );
    pdu.push_back( static_cast<uint8_t>( count >> 8 ) );
    pdu.push_back( static_cast<uint8_t>( count & 0xFF ) );
    for ( uint16_t i = 0; i < count; ++i ) {
        pdu.push_back( static_cast<uint8_t>( data[i] >> 8 ) );
        pdu.push_back( static_cast<uint8_t>( data[i] & 0xFF ) );
    }
    return pdu;
}

//---------------------------------------------------------------------------
// TCPIPProtocol
//---------------------------------------------------------------------------

TCPIPProtocol::TCPIPProtocol( RequestHandler& Handler )
    : Protocol( Handler )
{}

TCPIPProtocol::~TCPIPProtocol()
{}

void TCPIPProtocol::Start( uint16_t Port )
{
    if ( IsRunning() ) return;
    DoStart( Port );
}

void TCPIPProtocol::Stop()
{
    if ( !IsRunning() ) return;
    DoStop();
}

//---------------------------------------------------------------------------

std::vector<uint8_t> TCPIPProtocol::WrapInMBAP( uint16_t Tid, uint8_t UnitId,
                                                  const std::vector<uint8_t>& PDU ) const
{
    // MBAP header: TID(2) + PID(2) + LEN(2) + UnitId(1) + PDU
    uint16_t mbapLen = static_cast<uint16_t>( 1 + PDU.size() ); // UnitId + PDU
    std::vector<uint8_t> frame;
    frame.reserve( 7 + PDU.size() );
    frame.push_back( static_cast<uint8_t>( Tid >> 8 ) );
    frame.push_back( static_cast<uint8_t>( Tid & 0xFF ) );
    frame.push_back( 0x00 ); // Protocol Id Hi
    frame.push_back( 0x00 ); // Protocol Id Lo
    frame.push_back( static_cast<uint8_t>( mbapLen >> 8 ) );
    frame.push_back( static_cast<uint8_t>( mbapLen & 0xFF ) );
    frame.push_back( UnitId );
    frame.insert( frame.end(), PDU.begin(), PDU.end() );
    return frame;
}

//---------------------------------------------------------------------------
// ProcessFrame — main entry point for the TCP serve loop
//---------------------------------------------------------------------------

std::vector<uint8_t> TCPIPProtocol::ProcessFrame( const std::vector<uint8_t>& InFrame )
{
    // Minimum: 6 bytes MBAP prefix + 1 UnitId + 1 FC = 8 bytes
    if ( InFrame.size() < 8 )
        return {}; // malformed; caller should close connection

    const uint8_t* p = InFrame.data();

    uint16_t tid      = Get16( p + 0 );
    // protocol id at p+2 — we accept any (don't validate on server side)
    uint16_t mbapLen  = Get16( p + 4 );
    uint8_t  unitId   = p[6];

    // mbapLen covers UnitId(1) + PDU; PDU starts at byte 7
    if ( mbapLen < 2 || static_cast<size_t>( 6 + mbapLen ) > InFrame.size() )
        return {};

    uint8_t        fc      = p[7];
    const uint8_t* data    = p + 8;
    int            dataLen = static_cast<int>( mbapLen ) - 2; // subtract UnitId + FC

    std::vector<uint8_t> pdu = DispatchRequest( fc, data, dataLen );
    return WrapInMBAP( tid, unitId, pdu );
}

//---------------------------------------------------------------------------
}; // End of namespace Server
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
