//---------------------------------------------------------------------------
// Modbus TCP test suite — Boost.Test 1.89, static library variant.
//
// A Modbus TCP slave runs via Modbus::Server::TCPProtocolWinSock on 127.0.0.1:5020.
// ServerFixture (global fixture) starts it before any test runs and stops
// it cleanly after the last test completes.
//
// Server initial register state:
//   coilRegs[i]   = (i & 1)        (FC01)
//   inputBits[i]  = ((i % 3) == 0) (FC02)
//   holdingRegs[i] = i          (FC03 / FC06 / FC16 / FC22)
//   inputRegs[i]   = 0x1000 + i (FC04, read-only)
//   fileRecords[f][r] = ((f+1)<<8)|r  (FC20/FC21, 4 files x 32 records)
//---------------------------------------------------------------------------

#pragma hdrstop

#include <System.SysUtils.hpp>

#include <cstdint>
#include <tchar.h>

#include "ModbusTCP_IP.h"
#include "ModbusTCP_WinSock.h"
#include "ModbusDummy.h"
#include "ModbusRTU.h"
#include "ModbusServerTCP_WinSock.h"
#include "ModbusServerRTU.h"

// --- Boost.Test static-link -----------------------------------------------
// Keep Boost-provided main() and use a Unicode _tmain wrapper at the end
// of this file to forward wide argv to the narrow main entry point.
#define BOOST_TEST_MODULE ModbusTCP
#include <boost/test/included/unit_test.hpp>

#pragma comment(lib, "ws2_32")

using namespace Modbus;
using namespace Modbus::Master;

//---------------------------------------------------------------------------
// Shared register state accessed by both server handler and tests directly
//---------------------------------------------------------------------------

static const uint16_t SERVER_PORT = 5020;
static const int      REG_COUNT   = 256;

static const int      FIFO_MAX    = 31;
static const int      FILE_COUNT  = 4;     // FC20/FC21: number of files
static const int      FILE_RECS   = 32;    // FC20/FC21: records per file

static uint8_t  coilRegs[REG_COUNT];
static uint8_t  inputBits[REG_COUNT];
static uint16_t holdingRegs[REG_COUNT];
static uint16_t inputRegs[REG_COUNT];
static uint8_t  exceptionStatus;          // FC07
static uint16_t fifoQueue[FIFO_MAX];      // FC24
static uint16_t fifoCount;
static uint16_t fileRecords[FILE_COUNT][FILE_RECS]; // FC20/FC21

static void initRegisters()
{
    for ( int i = 0; i < REG_COUNT; ++i ) {
        coilRegs[i]    = static_cast<uint8_t>( i & 1 );          // 0,1,0,1,...
        inputBits[i]   = static_cast<uint8_t>( ( i % 3 ) == 0 ); // 1,0,0,1,0,0,...
        holdingRegs[i] = static_cast<uint16_t>( i );
        inputRegs[i]   = static_cast<uint16_t>( 0x1000 + i );
    }
    exceptionStatus = 0x6D;  // FC07: arbitrary known pattern
    fifoCount = 5;           // FC24: 5 values in the FIFO
    for ( int i = 0; i < FIFO_MAX; ++i )
        fifoQueue[i] = static_cast<uint16_t>( 0x100 + i );
    // FC20/FC21: file records — file F, record R = 0x(F+1)(R) pattern
    for ( int f = 0; f < FILE_COUNT; ++f )
        for ( int r = 0; r < FILE_RECS; ++r )
            fileRecords[f][r] = static_cast<uint16_t>( ( ( f + 1 ) << 8 ) | r );
}

//---------------------------------------------------------------------------
// TestRequestHandler — drives all FC calls through the shared globals above
//---------------------------------------------------------------------------

class TestRequestHandler : public Modbus::Server::RequestHandler {
public:
    std::optional<ExceptionCode> OnReadCoilStatus(
        CoilAddrType addr, CoilCountType count, CoilDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( CoilCountType i = 0; i < count; ++i )
            if ( coilRegs[addr + i] )
                data[i / 8] = static_cast<uint8_t>( data[i / 8] | ( 1u << ( i % 8 ) ) );
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadInputStatus(
        CoilAddrType addr, CoilCountType count, CoilDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( CoilCountType i = 0; i < count; ++i )
            if ( inputBits[addr + i] )
                data[i / 8] = static_cast<uint8_t>( data[i / 8] | ( 1u << ( i % 8 ) ) );
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadHoldingRegisters(
        RegAddrType addr, RegCountType count, RegDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( RegCountType i = 0; i < count; ++i )
            data[i] = holdingRegs[addr + i];
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadInputRegisters(
        RegAddrType addr, RegCountType count, RegDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( RegCountType i = 0; i < count; ++i )
            data[i] = inputRegs[addr + i];
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnForceSingleCoil(
        CoilAddrType addr, bool value ) override
    {
        if ( addr >= REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        coilRegs[addr] = value ? 1 : 0;
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnPresetSingleRegister(
        RegAddrType addr, RegDataType value ) override
    {
        if ( addr >= REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        holdingRegs[addr] = value;
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadExceptionStatus(
        ExceptionStatusDataType& status ) override
    {
        status = exceptionStatus;
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnDiagnostics(
        DiagSubFnType /*subFn*/, RegDataType data, RegDataType& reply ) override
    {
        reply = data; // echo data for all sub-functions (simplified slave)
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnForceMultipleCoils(
        CoilAddrType addr, CoilCountType count, const CoilDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( CoilCountType i = 0; i < count; ++i )
            coilRegs[addr + i] = static_cast<uint8_t>( ( data[i / 8] >> ( i % 8 ) ) & 1 );
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnPresetMultipleRegisters(
        RegAddrType addr, RegCountType count, const RegDataType* data ) override
    {
        if ( addr + count > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        for ( RegCountType i = 0; i < count; ++i )
            holdingRegs[addr + i] = data[i];
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadGeneralReference(
        const FileSubRequest* subs, size_t subCount, RegDataType* data ) override
    {
        size_t off = 0;
        for ( size_t s = 0; s < subCount; ++s ) {
            uint16_t fileNo = subs[s].FileNumber;
            uint16_t recNo  = subs[s].RecordNumber;
            uint16_t recLen = subs[s].RecordLength;
            if ( fileNo < 1 || fileNo > FILE_COUNT )  return ExceptionCode::IllegalDataAddress;
            if ( recNo + recLen > FILE_RECS )          return ExceptionCode::IllegalDataAddress;
            for ( uint16_t r = 0; r < recLen; ++r )
                data[off++] = fileRecords[fileNo - 1][recNo + r];
        }
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnWriteGeneralReference(
        const FileSubRequest* subs, size_t subCount, const RegDataType* data ) override
    {
        // Validate all sub-requests before writing
        size_t totalWords = 0;
        for ( size_t s = 0; s < subCount; ++s ) {
            uint16_t fileNo = subs[s].FileNumber;
            uint16_t recNo  = subs[s].RecordNumber;
            uint16_t recLen = subs[s].RecordLength;
            if ( fileNo < 1 || fileNo > FILE_COUNT )  return ExceptionCode::IllegalDataAddress;
            if ( recNo + recLen > FILE_RECS )          return ExceptionCode::IllegalDataAddress;
            totalWords += recLen;
        }
        size_t off = 0;
        for ( size_t s = 0; s < subCount; ++s ) {
            uint16_t fileNo = subs[s].FileNumber;
            uint16_t recNo  = subs[s].RecordNumber;
            uint16_t recLen = subs[s].RecordLength;
            for ( uint16_t r = 0; r < recLen; ++r )
                fileRecords[fileNo - 1][recNo + r] = data[off++];
        }
        (void)totalWords;
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnMaskWrite4XRegister(
        RegAddrType addr, RegDataType andMask, RegDataType orMask ) override
    {
        if ( addr >= REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        holdingRegs[addr] = static_cast<uint16_t>(
            ( holdingRegs[addr] & andMask ) | ( orMask & ~andMask ) );
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadWrite4XRegisters(
        RegAddrType rAddr, RegCountType rCount, RegDataType* rData,
        RegAddrType wAddr, RegCountType wCount, const RegDataType* wData ) override
    {
        if ( rAddr + rCount > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        if ( wAddr + wCount > REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        // Write first (per Modbus spec)
        for ( RegCountType i = 0; i < wCount; ++i )
            holdingRegs[wAddr + i] = wData[i];
        for ( RegCountType i = 0; i < rCount; ++i )
            rData[i] = holdingRegs[rAddr + i];
        return std::nullopt;
    }

    std::optional<ExceptionCode> OnReadFIFOQueue(
        FIFOAddrType addr, RegDataType* data, FIFOCountType& count ) override
    {
        if ( addr >= REG_COUNT ) return ExceptionCode::IllegalDataAddress;
        count = fifoCount;
        for ( FIFOCountType i = 0; i < fifoCount; ++i )
            data[i] = fifoQueue[i];
        return std::nullopt;
    }
};

//---------------------------------------------------------------------------
// Global fixture — starts/stops the server around the entire test run
//---------------------------------------------------------------------------

struct ServerFixture {
    ServerFixture()
    {
        initRegisters();
        server_.Start( SERVER_PORT );
        BOOST_TEST_MESSAGE( "Embedded Modbus server listening on 127.0.0.1:" << SERVER_PORT );
    }
    ~ServerFixture()
    {
        try {
            server_.Stop();
        }
        catch ( ... ) {
        }
    }
private:
    TestRequestHandler              handler_;
    Modbus::Server::TCPProtocolWinSock server_{ handler_ };
};

BOOST_TEST_GLOBAL_FIXTURE( ServerFixture );

//---------------------------------------------------------------------------
// RTU test infrastructure — optional, activated by the user at startup
//---------------------------------------------------------------------------

static bool   gRTUEnabled    = false;
static String gRTUMasterPort;
static String gRTUSlavePort;

// Global fixture: asks the user (once, before all tests) whether to run the
// RTU round-trip suite and — if yes — which COM ports to use.  In a
// non-interactive session (piped stdin / CI) the fixture falls back to the
// environment variables MODBUS_RTU_MASTER and MODBUS_RTU_SLAVE so the RTU
// tests can still be driven from a script without modification.
struct RTUServerFixture {
    RTUServerFixture()
    {
        HANDLE hStdin = GetStdHandle( STD_INPUT_HANDLE );
        DWORD  mode   = 0;
        bool   isConsole = ( GetConsoleMode( hStdin, &mode ) != FALSE );

        // Env vars take precedence — supports both scripted and CI runs.
        // Interactive prompt is the fallback when no env vars are set and
        // we are attached to a real console.
        const char* envMaster = getenv( "MODBUS_RTU_MASTER" );
        const char* envSlave  = getenv( "MODBUS_RTU_SLAVE" );
        if ( envMaster && envSlave && *envMaster && *envSlave ) {
            gRTUMasterPort = envMaster;
            gRTUSlavePort  = envSlave;
            gRTUEnabled    = true;
        } else if ( isConsole ) {
            std::cout << "\nRun RTU COM port tests? [y/N] " << std::flush;
            std::string answer;
            if ( std::getline( std::cin, answer ) && !answer.empty()
                     && ( answer[0] == 'y' || answer[0] == 'Y' ) ) {
                std::string master, slave;
                std::cout << "Master COM port (e.g. COM8): " << std::flush;
                std::getline( std::cin, master );
                std::cout << "Slave  COM port (e.g. COM9): " << std::flush;
                std::getline( std::cin, slave );
                if ( !master.empty() && !slave.empty() ) {
                    gRTUMasterPort = master.c_str();
                    gRTUSlavePort  = slave.c_str();
                    gRTUEnabled    = true;
                }
            }
        }

        if ( gRTUEnabled ) {
            handler_ = std::make_unique<TestRequestHandler>();
            server_  = std::make_unique<Modbus::Server::RTUProtocol>( *handler_ );
            try {
                server_->Start( gRTUSlavePort );
                BOOST_TEST_MESSAGE( "RTU slave listening on "
                    << AnsiString( gRTUSlavePort ).c_str() );
            }
            catch ( ... ) {
                gRTUEnabled = false;
                server_.reset();
                handler_.reset();
                BOOST_TEST_MESSAGE( "RTU slave failed to start — RTU tests skipped" );
            }
        }
    }

    ~RTUServerFixture()
    {
        if ( server_ )
            try { server_->Stop(); } catch ( ... ) {}
    }

private:
    std::unique_ptr<TestRequestHandler>          handler_;
    std::unique_ptr<Modbus::Server::RTUProtocol> server_;
};

BOOST_TEST_GLOBAL_FIXTURE( RTUServerFixture );

//---------------------------------------------------------------------------
// Per-suite fixture — creates a fresh protocol connection for each suite
// and resets the register bank so suites are independent
//---------------------------------------------------------------------------

struct ProtoFixture {
    ProtoFixture()
        : proto_( _D( "127.0.0.1" ), SERVER_PORT )
        , session_( proto_ )
    {
        initRegisters(); // fresh state for every suite
    }
    TCPProtocolWinSock proto_;
    SessionManager     session_;
};

//---------------------------------------------------------------------------
// Helpers
//---------------------------------------------------------------------------

static TCPIPContext ctx( uint8_t slave = 1, uint16_t tid = 1 )
{
    return TCPIPContext( slave, tid );
}

static uint16_t readH( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    RegDataType val = 0;
    p.ReadHoldingRegisters( ctx( slave ), addr, 1, &val );
    return val;
}

static uint16_t readI( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    RegDataType val = 0;
    p.ReadInputRegisters( ctx( slave ), addr, 1, &val );
    return val;
}

static uint8_t readC( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    CoilDataType packed = 0;
    p.ReadCoilStatus( ctx( slave ), addr, 1, &packed );
    return static_cast<uint8_t>( packed & 0x01u );
}

static uint8_t readD( TCPIPProtocol& p, uint16_t addr, uint8_t slave = 1 )
{
    CoilDataType packed = 0;
    p.ReadInputStatus( ctx( slave ), addr, 1, &packed );
    return static_cast<uint8_t>( packed & 0x01u );
}

//---------------------------------------------------------------------------
// Test suites
//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC01_ReadCoilStatus, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleCoilAlternatingPattern )
    {
        BOOST_TEST( readC( proto_, 0 ) == 0u );
        BOOST_TEST( readC( proto_, 1 ) == 1u );
        BOOST_TEST( readC( proto_, 2 ) == 0u );
        BOOST_TEST( readC( proto_, 3 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( PackedBitOrderLSBFirst )
    {
        CoilDataType buf[2] = { 0, 0 };
        proto_.ReadCoilStatus( ctx(), 0, 16, buf );
        BOOST_TEST( buf[0] == 0xAAu );
        BOOST_TEST( buf[1] == 0xAAu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto_.ReadCoilStatus( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC02_ReadInputStatus, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleInputModuloPattern )
    {
        BOOST_TEST( readD( proto_, 0 ) == 1u );
        BOOST_TEST( readD( proto_, 1 ) == 0u );
        BOOST_TEST( readD( proto_, 2 ) == 0u );
        BOOST_TEST( readD( proto_, 3 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( PackedBitOrderLSBFirst )
    {
        CoilDataType buf[2] = { 0, 0 };
        proto_.ReadInputStatus( ctx(), 0, 16, buf );
        BOOST_TEST( buf[0] == 0x49u );
        BOOST_TEST( buf[1] == 0x92u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto_.ReadInputStatus( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC03_ReadHoldingRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleRegisterEqualsAddress )
    {
        BOOST_TEST( readH( proto_, 0 )  == 0  );
        BOOST_TEST( readH( proto_, 1 )  == 1  );
        BOOST_TEST( readH( proto_, 10 ) == 10 );
    }

    BOOST_AUTO_TEST_CASE( BlockReadSequentialValues )
    {
        const uint16_t START = 20, COUNT = 8;
        std::vector<RegDataType> buf( COUNT );
        proto_.ReadHoldingRegisters( ctx(), START, COUNT, buf.data() );
        for ( int i = 0; i < COUNT; ++i )
            BOOST_TEST( buf[i] == START + i );
    }

    BOOST_AUTO_TEST_CASE( BoundaryAddress )
    {
        BOOST_TEST( readH( proto_, 255 ) == 255 );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2];
        BOOST_CHECK_THROW(
            proto_.ReadHoldingRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC04_ReadInputRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleRegisterEqualsBase )
    {
        BOOST_TEST( readI( proto_, 0 )   == 0x1000u );
        BOOST_TEST( readI( proto_, 1 )   == 0x1001u );
        BOOST_TEST( readI( proto_, 255 ) == 0x10FFu );
    }

    BOOST_AUTO_TEST_CASE( BlockReadSequentialValues )
    {
        const uint16_t START = 5, COUNT = 4;
        std::vector<RegDataType> buf( COUNT );
        proto_.ReadInputRegisters( ctx(), START, COUNT, buf.data() );
        for ( int i = 0; i < COUNT; ++i )
            BOOST_TEST( buf[i] == 0x1000u + START + i );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2];
        BOOST_CHECK_THROW(
            proto_.ReadInputRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC06_PresetSingleRegister, ProtoFixture )

    BOOST_AUTO_TEST_CASE( WriteAndReadBack )
    {
        proto_.PresetSingleRegister( ctx(), 50, 0xABCD );
        BOOST_TEST( readH( proto_, 50 ) == 0xABCDu );
    }

    BOOST_AUTO_TEST_CASE( OverwriteWithZero )
    {
        proto_.PresetSingleRegister( ctx(), 50, 0xABCD );
        proto_.PresetSingleRegister( ctx(), 50, 0x0000 );
        BOOST_TEST( readH( proto_, 50 ) == 0x0000u );
    }

    BOOST_AUTO_TEST_CASE( BoundaryAddress )
    {
        proto_.PresetSingleRegister( ctx(), 255, 0x1234 );
        BOOST_TEST( readH( proto_, 255 ) == 0x1234u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        BOOST_CHECK_THROW(
            proto_.PresetSingleRegister( ctx(), 256, 0xFF ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC16_PresetMultipleRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( BlockRoundTrip )
    {
        RegDataType writeVals[] = { 0x0100, 0x0200, 0x0300 };
        proto_.PresetMultipleRegisters( ctx(), 60, 3, writeVals );
        std::vector<RegDataType> readVals( 3 );
        proto_.ReadHoldingRegisters( ctx(), 60, 3, readVals.data() );
        BOOST_TEST( readVals[0] == 0x0100u );
        BOOST_TEST( readVals[1] == 0x0200u );
        BOOST_TEST( readVals[2] == 0x0300u );
    }

    BOOST_AUTO_TEST_CASE( SingleRegisterViaFC16 )
    {
        RegDataType val = 0xBEEF;
        proto_.PresetMultipleRegisters( ctx(), 70, 1, &val );
        BOOST_TEST( readH( proto_, 70 ) == 0xBEEFu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        RegDataType v[2] = { 1, 2 };
        BOOST_CHECK_THROW(
            proto_.PresetMultipleRegisters( ctx(), 255, 2, v ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC22_MaskWrite4XRegister, ProtoFixture )

    BOOST_AUTO_TEST_CASE( MaskApplied )
    {
        // (0xAA55 & 0xFF00) | (0x00AA & 0x00FF) = 0xAA00 | 0x00AA = 0xAAAA
        proto_.PresetSingleRegister( ctx(), 80, 0xAA55 );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0xFF00, 0x00AA );
        BOOST_TEST( readH( proto_, 80 ) == 0xAAAAu );
    }

    BOOST_AUTO_TEST_CASE( ChainedMasks )
    {
        // starting from 0xAAAA: (0xAAAA & 0x0F0F) | (0xF0F0 & 0xF0F0) = 0x0A0A | 0xF0F0 = 0xFAFA
        proto_.PresetSingleRegister( ctx(), 80, 0xAAAA );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0x0F0F, 0xF0F0 );
        BOOST_TEST( readH( proto_, 80 ) == 0xFAFAu );
    }

    BOOST_AUTO_TEST_CASE( IdentityMaskLeavesValueUnchanged )
    {
        proto_.PresetSingleRegister( ctx(), 80, 0x5A5A );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0xFFFF, 0x0000 );
        BOOST_TEST( readH( proto_, 80 ) == 0x5A5Au );
    }

    BOOST_AUTO_TEST_CASE( ZeroAndMaskForcesOrMask )
    {
        proto_.PresetSingleRegister( ctx(), 80, 0xFFFF );
        proto_.MaskWrite4XRegister ( ctx(), 80, 0x0000, 0x1234 );
        BOOST_TEST( readH( proto_, 80 ) == 0x1234u );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC05_ForceSingleCoil, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ForceOnAndReadBack )
    {
        // coilRegs[0] starts as 0 (even index)
        proto_.ForceSingleCoil( ctx(), 0, true );
        BOOST_TEST( readC( proto_, 0 ) == 1u );
    }

    BOOST_AUTO_TEST_CASE( ForceOffAndReadBack )
    {
        // coilRegs[1] starts as 1 (odd index)
        proto_.ForceSingleCoil( ctx(), 1, false );
        BOOST_TEST( readC( proto_, 1 ) == 0u );
    }

    BOOST_AUTO_TEST_CASE( ToggleCoil )
    {
        proto_.ForceSingleCoil( ctx(), 10, true );
        BOOST_TEST( readC( proto_, 10 ) == 1u );
        proto_.ForceSingleCoil( ctx(), 10, false );
        BOOST_TEST( readC( proto_, 10 ) == 0u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        BOOST_CHECK_THROW(
            proto_.ForceSingleCoil( ctx(), 256, true ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC15_ForceMultipleCoils, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ForceBlockAndReadBack )
    {
        // Force coils 0..7 to all ON (0xFF)
        CoilDataType data[1] = { 0xFF };
        proto_.ForceMultipleCoils( ctx(), 0, 8, data );
        CoilDataType readBuf[1] = { 0 };
        proto_.ReadCoilStatus( ctx(), 0, 8, readBuf );
        BOOST_TEST( readBuf[0] == 0xFFu );
    }

    BOOST_AUTO_TEST_CASE( ForcePatternAndReadBack )
    {
        // Force coils 16..31 to alternating pattern 0xA5, 0x5A
        CoilDataType data[2] = { 0xA5, 0x5A };
        proto_.ForceMultipleCoils( ctx(), 16, 16, data );
        CoilDataType readBuf[2] = { 0, 0 };
        proto_.ReadCoilStatus( ctx(), 16, 16, readBuf );
        BOOST_TEST( readBuf[0] == 0xA5u );
        BOOST_TEST( readBuf[1] == 0x5Au );
    }

    BOOST_AUTO_TEST_CASE( ForceAllOff )
    {
        CoilDataType data[1] = { 0x00 };
        proto_.ForceMultipleCoils( ctx(), 0, 8, data );
        CoilDataType readBuf[1] = { 0xFF };
        proto_.ReadCoilStatus( ctx(), 0, 8, readBuf );
        BOOST_TEST( readBuf[0] == 0x00u );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeThrows )
    {
        CoilDataType data[1] = { 0xFF };
        BOOST_CHECK_THROW(
            proto_.ForceMultipleCoils( ctx(), 255, 2, data ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC23_ReadWrite4XRegisters, ProtoFixture )

    BOOST_AUTO_TEST_CASE( WriteAndReadSameRegion )
    {
        // Write 0xAAAA to addr 100, then read it back atomically
        RegDataType writeVals[] = { 0xAAAA };
        RegDataType readVal = 0;
        proto_.ReadWrite4XRegisters( ctx(),
            100, 1, &readVal,    // read addr 100
            100, 1, writeVals ); // write addr 100
        // Write happens first, so read should see the written value
        BOOST_TEST( readVal == 0xAAAAu );
    }

    BOOST_AUTO_TEST_CASE( WriteThenReadDifferentRegions )
    {
        RegDataType writeVals[] = { 0x1111, 0x2222 };
        RegDataType readVals[3] = { 0, 0, 0 };
        proto_.ReadWrite4XRegisters( ctx(),
            0, 3, readVals,          // read holding regs 0..2
            50, 2, writeVals );      // write to 50..51
        // Regs 0..2 were not modified, should still be 0,1,2
        BOOST_TEST( readVals[0] == 0u );
        BOOST_TEST( readVals[1] == 1u );
        BOOST_TEST( readVals[2] == 2u );
        // Verify write took effect
        BOOST_TEST( readH( proto_, 50 ) == 0x1111u );
        BOOST_TEST( readH( proto_, 51 ) == 0x2222u );
    }

    BOOST_AUTO_TEST_CASE( WriteBeforeReadSemantics )
    {
        // Write 0xBEEF to addr 200, then read addr 200 in same call
        // Per spec, write is performed before read
        RegDataType writeVals[] = { 0xBEEF };
        RegDataType readVal = 0;
        proto_.ReadWrite4XRegisters( ctx(),
            200, 1, &readVal,
            200, 1, writeVals );
        BOOST_TEST( readVal == 0xBEEFu );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeReadThrows )
    {
        RegDataType writeVals[] = { 0x0001 };
        RegDataType readVals[2] = { 0, 0 };
        BOOST_CHECK_THROW(
            proto_.ReadWrite4XRegisters( ctx(),
                255, 2, readVals,
                0, 1, writeVals ),
            EBaseException );
    }

    BOOST_AUTO_TEST_CASE( OutOfRangeWriteThrows )
    {
        RegDataType writeVals[] = { 0x0001, 0x0002 };
        RegDataType readVal = 0;
        BOOST_CHECK_THROW(
            proto_.ReadWrite4XRegisters( ctx(),
                0, 1, &readVal,
                255, 2, writeVals ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC07_ReadExceptionStatus, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ReturnsKnownPattern )
    {
        auto status = proto_.ReadExceptionStatus( ctx() );
        BOOST_TEST( status == 0x6Du );
    }

    BOOST_AUTO_TEST_CASE( ReturnTypeIsUint8 )
    {
        ExceptionStatusDataType status = proto_.ReadExceptionStatus( ctx() );
        BOOST_TEST( sizeof( status ) == 1u );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC08_Diagnostics, ProtoFixture )

    BOOST_AUTO_TEST_CASE( EchoTestReturnsData )
    {
        auto result = proto_.Diagnostics(
            ctx(),
            static_cast<DiagSubFnType>( DiagnosticsSubFunction::ReturnQueryData ),
            0xABCD
        );
        BOOST_TEST( result == 0xABCDu );
    }

    BOOST_AUTO_TEST_CASE( EchoTestZero )
    {
        auto result = proto_.Diagnostics(
            ctx(),
            static_cast<DiagSubFnType>( DiagnosticsSubFunction::ReturnQueryData ),
            0x0000
        );
        BOOST_TEST( result == 0x0000u );
    }

    BOOST_AUTO_TEST_CASE( EchoTestMaxValue )
    {
        auto result = proto_.Diagnostics(
            ctx(),
            static_cast<DiagSubFnType>( DiagnosticsSubFunction::ReturnQueryData ),
            0xFFFF
        );
        BOOST_TEST( result == 0xFFFFu );
    }

    BOOST_AUTO_TEST_CASE( SubFunctionEchoed )
    {
        // Our test server echoes all sub-functions; just verify no exception
        BOOST_CHECK_NO_THROW(
            proto_.Diagnostics(
                ctx(),
                static_cast<DiagSubFnType>( DiagnosticsSubFunction::ReturnBusMessageCount ),
                0x0000
            )
        );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC24_ReadFIFOQueue, ProtoFixture )

    BOOST_AUTO_TEST_CASE( ReadsExpectedCount )
    {
        RegDataType buf[FIFO_MAX] = {};
        FIFOCountType count = proto_.ReadFIFOQueue( ctx(), 0, buf );
        BOOST_TEST( count == 5u );
    }

    BOOST_AUTO_TEST_CASE( ReadsExpectedValues )
    {
        RegDataType buf[FIFO_MAX] = {};
        FIFOCountType count = proto_.ReadFIFOQueue( ctx(), 0, buf );
        BOOST_TEST( count == 5u );
        for ( int i = 0; i < count; ++i ) {
            BOOST_TEST( buf[i] == static_cast<uint16_t>( 0x100 + i ) );
        }
    }

    BOOST_AUTO_TEST_CASE( EmptyFIFO )
    {
        fifoCount = 0;  // empty the FIFO for this test
        RegDataType buf[FIFO_MAX] = {};
        buf[0] = 0xDEAD;  // sentinel
        FIFOCountType count = proto_.ReadFIFOQueue( ctx(), 0, buf );
        BOOST_TEST( count == 0u );
        BOOST_TEST( buf[0] == 0xDEADu );  // buffer untouched
    }

    BOOST_AUTO_TEST_CASE( InvalidAddressThrows )
    {
        RegDataType buf[FIFO_MAX] = {};
        BOOST_CHECK_THROW(
            proto_.ReadFIFOQueue( ctx(), 256, buf ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC20_ReadGeneralReference, ProtoFixture )

    BOOST_AUTO_TEST_CASE( SingleSubRequest_ReadsCorrectValues )
    {
        // Read 3 records starting at record 0 from file 1
        FileSubRequest sub { 1, 0, 3 };
        RegDataType data[3] = {};
        proto_.ReadGeneralReference( ctx(), &sub, 1, data );
        // fileRecords[0][0..2] = 0x0100, 0x0101, 0x0102
        BOOST_TEST( data[0] == 0x0100u );
        BOOST_TEST( data[1] == 0x0101u );
        BOOST_TEST( data[2] == 0x0102u );
    }

    BOOST_AUTO_TEST_CASE( MultipleSubRequests_ConcatenatedOutput )
    {
        // Sub-request 1: file 1, record 0, length 2
        // Sub-request 2: file 2, record 4, length 3
        FileSubRequest subs[2] = { { 1, 0, 2 }, { 2, 4, 3 } };
        RegDataType data[5] = {};
        proto_.ReadGeneralReference( ctx(), subs, 2, data );
        // File 1: 0x0100, 0x0101
        BOOST_TEST( data[0] == 0x0100u );
        BOOST_TEST( data[1] == 0x0101u );
        // File 2: 0x0204, 0x0205, 0x0206
        BOOST_TEST( data[2] == 0x0204u );
        BOOST_TEST( data[3] == 0x0205u );
        BOOST_TEST( data[4] == 0x0206u );
    }

    BOOST_AUTO_TEST_CASE( InvalidFileNumber_Throws )
    {
        FileSubRequest sub { 99, 0, 1 }; // file 99 does not exist
        RegDataType data[1] = {};
        BOOST_CHECK_THROW(
            proto_.ReadGeneralReference( ctx(), &sub, 1, data ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( FC21_WriteGeneralReference, ProtoFixture )

    BOOST_AUTO_TEST_CASE( WriteThenReadBack )
    {
        // Write 2 records to file 1, starting at record 10
        FileSubRequest sub { 1, 10, 2 };
        RegDataType wData[2] = { 0xAAAAu, 0xBBBBu };
        proto_.WriteGeneralReference( ctx(), &sub, 1, wData );

        // Read back the same records via FC20
        RegDataType rData[2] = {};
        proto_.ReadGeneralReference( ctx(), &sub, 1, rData );
        BOOST_TEST( rData[0] == 0xAAAAu );
        BOOST_TEST( rData[1] == 0xBBBBu );
    }

    BOOST_AUTO_TEST_CASE( MultipleSubRequests_WriteThenVerify )
    {
        FileSubRequest subs[2] = { { 1, 20, 1 }, { 2, 20, 1 } };
        RegDataType wData[2] = { 0x1234u, 0x5678u };
        proto_.WriteGeneralReference( ctx(), subs, 2, wData );

        // Verify each independently
        RegDataType r1 = 0, r2 = 0;
        FileSubRequest s1 { 1, 20, 1 }, s2 { 2, 20, 1 };
        proto_.ReadGeneralReference( ctx(), &s1, 1, &r1 );
        proto_.ReadGeneralReference( ctx(), &s2, 1, &r2 );
        BOOST_TEST( r1 == 0x1234u );
        BOOST_TEST( r2 == 0x5678u );
    }

    BOOST_AUTO_TEST_CASE( InvalidFileNumber_Throws )
    {
        FileSubRequest sub { 99, 0, 1 };
        RegDataType data[1] = { 0x0000u };
        BOOST_CHECK_THROW(
            proto_.WriteGeneralReference( ctx(), &sub, 1, data ),
            EBaseException );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE( FC01_FC02_DummyEndpoint )

    BOOST_AUTO_TEST_CASE( DummyReadCoilsLeavesBufferUnchanged )
    {
        DummyProtocol proto;
        SessionManager session( proto );

        CoilDataType v[2] = { 0x5Au, 0xA5u };
        proto.ReadCoilStatus( Context( 1 ), 0, 16, v );
        BOOST_TEST( v[0] == 0x5Au );
        BOOST_TEST( v[1] == 0xA5u );
    }

    BOOST_AUTO_TEST_CASE( DummyReadInputsLeavesBufferUnchanged )
    {
        DummyProtocol proto;
        SessionManager session( proto );

        CoilDataType v[2] = { 0x33u, 0xCCu };
        proto.ReadInputStatus( Context( 1 ), 0, 16, v );
        BOOST_TEST( v[0] == 0x33u );
        BOOST_TEST( v[1] == 0xCCu );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE( FC01_FC02_RTUEndpoint )

    BOOST_AUTO_TEST_CASE( ReadCoilsWithoutOpenPortThrows )
    {
        RTUProtocol proto;
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto.ReadCoilStatus( Context( 1 ), 0, 1, v ),
            Exception );
    }

    BOOST_AUTO_TEST_CASE( ReadInputsWithoutOpenPortThrows )
    {
        RTUProtocol proto;
        CoilDataType v[1] = { 0 };
        BOOST_CHECK_THROW(
            proto.ReadInputStatus( Context( 1 ), 0, 1, v ),
            Exception );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------
// RTU round-trip tests — skipped unless the user enabled COM ports at startup
//---------------------------------------------------------------------------

struct RTUMasterFixture {
    RTUMasterFixture()
    {
        if ( !gRTUEnabled ) return;
        initRegisters();
        proto_.SetCommPort( gRTUMasterPort );
        proto_.SetCommSpeed( 9600 );
        proto_.Open();
    }
    ~RTUMasterFixture()
    {
        if ( !gRTUEnabled ) return;
        try { proto_.Close(); } catch ( ... ) {}
    }
    RTUProtocol proto_;
};

BOOST_FIXTURE_TEST_SUITE( RTU_RoundTrip, RTUMasterFixture )

    BOOST_AUTO_TEST_CASE( ReadCoilStatus )
    {
        if ( !gRTUEnabled ) return;
        CoilDataType v[2] = {};
        proto_.ReadCoilStatus( Context( 1 ), 0, 16, v );
        // coilRegs[i] = i & 1 → packed LSB-first: 0,1,0,1,... → 0xAA per byte
        BOOST_TEST( v[0] == 0xAAu );
        BOOST_TEST( v[1] == 0xAAu );
    }

    BOOST_AUTO_TEST_CASE( ReadInputStatus )
    {
        if ( !gRTUEnabled ) return;
        CoilDataType v[2] = {};
        proto_.ReadInputStatus( Context( 1 ), 0, 16, v );
        // inputBits[i] = (i%3)==0 → bits 0-7: 1,0,0,1,0,0,1,0 = 0x49
        //                          → bits 8-15: 0,1,0,0,1,0,0,1 = 0x92
        BOOST_TEST( v[0] == 0x49u );
        BOOST_TEST( v[1] == 0x92u );
    }

    BOOST_AUTO_TEST_CASE( ReadHoldingRegisters )
    {
        if ( !gRTUEnabled ) return;
        RegDataType v[4] = {};
        proto_.ReadHoldingRegisters( Context( 1 ), 10, 4, v );
        BOOST_TEST( v[0] == 10u );
        BOOST_TEST( v[1] == 11u );
        BOOST_TEST( v[2] == 12u );
        BOOST_TEST( v[3] == 13u );
    }

    BOOST_AUTO_TEST_CASE( ReadInputRegisters )
    {
        if ( !gRTUEnabled ) return;
        RegDataType v[3] = {};
        proto_.ReadInputRegisters( Context( 1 ), 5, 3, v );
        BOOST_TEST( v[0] == 0x1005u );
        BOOST_TEST( v[1] == 0x1006u );
        BOOST_TEST( v[2] == 0x1007u );
    }

    BOOST_AUTO_TEST_CASE( ForceSingleCoil )
    {
        if ( !gRTUEnabled ) return;
        proto_.ForceSingleCoil( Context( 1 ), 4, true );
        CoilDataType v = 0;
        proto_.ReadCoilStatus( Context( 1 ), 4, 1, &v );
        BOOST_TEST( ( v & 1u ) == 1u );
        proto_.ForceSingleCoil( Context( 1 ), 4, false );
        v = 0;
        proto_.ReadCoilStatus( Context( 1 ), 4, 1, &v );
        BOOST_TEST( ( v & 1u ) == 0u );
    }

    BOOST_AUTO_TEST_CASE( PresetSingleRegister )
    {
        if ( !gRTUEnabled ) return;
        proto_.PresetSingleRegister( Context( 1 ), 20, 0xABCDu );
        RegDataType v = 0;
        proto_.ReadHoldingRegisters( Context( 1 ), 20, 1, &v );
        BOOST_TEST( v == 0xABCDu );
    }

    BOOST_AUTO_TEST_CASE( ReadExceptionStatus )
    {
        if ( !gRTUEnabled ) return;
        auto status = proto_.ReadExceptionStatus( Context( 1 ) );
        BOOST_TEST( status == 0x6Du );
    }

    BOOST_AUTO_TEST_CASE( Diagnostics )
    {
        if ( !gRTUEnabled ) return;
        auto reply = proto_.Diagnostics(
            Context( 1 ),
            static_cast<DiagSubFnType>( DiagnosticsSubFunction::ReturnQueryData ),
            0x1234u );
        BOOST_TEST( reply == 0x1234u );
    }

    BOOST_AUTO_TEST_CASE( ForceMultipleCoils )
    {
        if ( !gRTUEnabled ) return;
        CoilDataType src[2] = { 0xF0u, 0x0Fu };
        proto_.ForceMultipleCoils( Context( 1 ), 0, 16, src );
        CoilDataType v[2] = {};
        proto_.ReadCoilStatus( Context( 1 ), 0, 16, v );
        BOOST_TEST( v[0] == 0xF0u );
        BOOST_TEST( v[1] == 0x0Fu );
    }

    BOOST_AUTO_TEST_CASE( PresetMultipleRegisters )
    {
        if ( !gRTUEnabled ) return;
        RegDataType src[3] = { 100u, 200u, 300u };
        proto_.PresetMultipleRegisters( Context( 1 ), 30, 3, src );
        RegDataType v[3] = {};
        proto_.ReadHoldingRegisters( Context( 1 ), 30, 3, v );
        BOOST_TEST( v[0] == 100u );
        BOOST_TEST( v[1] == 200u );
        BOOST_TEST( v[2] == 300u );
    }

    BOOST_AUTO_TEST_CASE( MaskWrite4XRegister )
    {
        if ( !gRTUEnabled ) return;
        // holdingRegs[10] = 10 = 0x000A
        // (0x000A & 0xFF00) | (0x00F0 & ~0xFF00) = 0x0000 | 0x00F0 = 0x00F0
        proto_.MaskWrite4XRegister( Context( 1 ), 10, 0xFF00u, 0x00F0u );
        RegDataType v = 0;
        proto_.ReadHoldingRegisters( Context( 1 ), 10, 1, &v );
        BOOST_TEST( v == 0x00F0u );
    }

    BOOST_AUTO_TEST_CASE( ReadWrite4XRegisters )
    {
        if ( !gRTUEnabled ) return;
        RegDataType wData[2] = { 0xABCDu, 0xEF01u };
        RegDataType rData[2] = {};
        proto_.ReadWrite4XRegisters( Context( 1 ),
            50, 2, rData,
            50, 2, wData );
        BOOST_TEST( rData[0] == 0xABCDu );
        BOOST_TEST( rData[1] == 0xEF01u );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------

BOOST_FIXTURE_TEST_SUITE( TransactionId, ProtoFixture )

    BOOST_AUTO_TEST_CASE( LowTidEchoed )
    {
        RegDataType v = 0;
        // base class throws if the server does not echo the TID correctly
        BOOST_CHECK_NO_THROW(
            proto_.ReadHoldingRegisters( TCPIPContext( 1, 0x0001 ), 0, 1, &v ) );
    }

    BOOST_AUTO_TEST_CASE( MaxTidEchoed )
    {
        RegDataType v = 0;
        BOOST_CHECK_NO_THROW(
            proto_.ReadHoldingRegisters( TCPIPContext( 1, 0xFFFF ), 0, 1, &v ) );
    }

BOOST_AUTO_TEST_SUITE_END()

//---------------------------------------------------------------------------
// Entry point bridge for Unicode startup.
// When _TCHAR maps to wchar_t, startup calls wmain; Boost.Test provides main.
//---------------------------------------------------------------------------

int _tmain( int argc, _TCHAR* argv[] )
{
    (void)argc;
    (void)argv;
    extern int main( int, char** );
    return main( __argc, __argv );
}
