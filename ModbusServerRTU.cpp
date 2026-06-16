//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include "ModbusServerRTU.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------

RTUProtocol::RTUProtocol( RequestHandler& Handler )
    : Protocol( Handler )
{}

RTUProtocol::~RTUProtocol()
{
    Stop();
}

//---------------------------------------------------------------------------

void RTUProtocol::Start( const String& ComPort, DWORD BaudRate, uint8_t SlaveAddress )
{
    if ( IsRunning() ) return;
    stop_.store( false );
    std::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();
    thread_ = std::thread( &RTUProtocol::ServeLoop, this,
                           ComPort, BaudRate, SlaveAddress,
                           std::move( readyPromise ) );
    readyFuture.wait(); // block until port is open and thread is running
}

void RTUProtocol::Stop()
{
    stop_.store( true );
    if ( thread_.joinable() )
        thread_.join();
    running_.store( false );
}

//---------------------------------------------------------------------------

uint16_t RTUProtocol::ComputeCRC( const uint8_t* data, size_t len ) noexcept
{
    uint16_t crc = 0xFFFF;
    for ( size_t i = 0; i < len; ++i ) {
        crc ^= data[i];
        for ( int b = 0; b < 8; ++b )
            crc = ( crc & 1 ) ? ( crc >> 1 ) ^ 0xA001u : ( crc >> 1 );
    }
    return crc;
}

//---------------------------------------------------------------------------

// Reads one RTU frame from the COM port.
//
// Phase 1 — block for the first byte with a 100ms total read timeout so the
//            loop can check stop_ at a reasonable cadence.
// Phase 2 — switch to a 20ms inter-character gap timeout and accumulate bytes
//            until no new byte arrives within the gap (= end of frame).
//
// Using SetCommTimeouts on the raw HANDLE (obtained from TCommPort::GetHandle)
// gives us full control without fighting TCommPort's internal timeout state.
std::vector<uint8_t> RTUProtocol::ReadRTUFrame( HANDLE hCom )
{
    std::vector<uint8_t> frame;

    // Phase 1: wait for first byte, 100ms total timeout per ReadFile call
    {
        COMMTIMEOUTS ct = {};
        ct.ReadTotalTimeoutConstant = 100; // 100ms; loop re-checks stop_ on each timeout
        SetCommTimeouts( hCom, &ct );

        uint8_t byte;
        DWORD   nRead = 0;
        while ( !stop_.load() ) {
            ReadFile( hCom, &byte, 1, &nRead, nullptr );
            if ( nRead == 1 ) {
                frame.push_back( byte );
                break;
            }
        }
    }

    if ( frame.empty() ) return frame; // stop_ was set

    // Phase 2: accumulate remaining bytes; 20ms inter-character gap = end-of-frame.
    //
    // ReadIntervalTimeout alone is not enough: the timer only starts after the first
    // byte of a ReadFile call is received. If the buffer is already empty (all frame
    // bytes were absorbed by Phase 1 or an earlier Phase 2 call), ReadFile would block
    // forever. ReadTotalTimeoutConstant = 50ms provides the ceiling for that case.
    {
        COMMTIMEOUTS ct = {};
        ct.ReadIntervalTimeout        = 20; // 20ms gap between chars = end of frame
        ct.ReadTotalTimeoutMultiplier = 0;
        ct.ReadTotalTimeoutConstant   = 50; // 50ms ceiling when buffer is already empty
        SetCommTimeouts( hCom, &ct );

        uint8_t buf[256];
        DWORD   nRead = 0;
        while ( !stop_.load() ) {
            ReadFile( hCom, buf, sizeof( buf ), &nRead, nullptr );
            if ( nRead == 0 ) break; // gap detected — frame is complete
            frame.insert( frame.end(), buf, buf + nRead );
        }
    }

    return frame;
}

//---------------------------------------------------------------------------

void RTUProtocol::ServeLoop( String ComPort, DWORD BaudRate, uint8_t SlaveAddress,
                              std::promise<bool> ReadyPromise )
{
    TCommPort port;
    port.SetCommPort( ComPort.c_str() );
    port.SetBaudRate( BaudRate );
    port.SetParity( NOPARITY );
    port.SetByteSize( 8 );
    port.SetStopBits( ONESTOPBIT );

    try {
        port.OpenCommPort();
    }
    catch ( ... ) {
        ReadyPromise.set_value( false );
        return;
    }

    running_.store( true );
    ReadyPromise.set_value( true ); // signal Start() to unblock

    port.PurgeCommPort();
    HANDLE hCom = port.GetHandle();

    while ( !stop_.load() ) {
        std::vector<uint8_t> frame = ReadRTUFrame( hCom );

        // Minimum valid RTU frame: addr(1) + fc(1) + crc(2) = 4 bytes
        if ( frame.size() < 4 ) continue;

        // Validate CRC: CRC of all bytes except the last two must equal last two (Lo, Hi)
        uint16_t crcCalc = ComputeCRC( frame.data(), frame.size() - 2 );
        uint16_t crcRecv = static_cast<uint16_t>( frame[frame.size() - 2] ) |
                           ( static_cast<uint16_t>( frame[frame.size() - 1] ) << 8 );
        if ( crcCalc != crcRecv ) continue;

        uint8_t addr = frame[0];

        // Accept only frames addressed to us or the broadcast address (0)
        if ( addr != SlaveAddress && addr != 0 ) continue;

        uint8_t        fc      = frame[1];
        const uint8_t* data    = frame.data() + 2;
        int            dataLen = static_cast<int>( frame.size() ) - 4; // addr+fc+crc(2)

        std::vector<uint8_t> pdu = DispatchRequest( fc, data, dataLen );

        // Do not respond to broadcast frames (per Modbus spec)
        if ( addr == 0 ) continue;

        // Build RTU response: SlaveAddr(1) + PDU + CRC_Lo(1) + CRC_Hi(1)
        std::vector<uint8_t> response;
        response.reserve( 1 + pdu.size() + 2 );
        response.push_back( SlaveAddress );
        response.insert( response.end(), pdu.begin(), pdu.end() );

        uint16_t crc = ComputeCRC( response.data(), response.size() );
        response.push_back( static_cast<uint8_t>( crc & 0xFF ) );        // CRC Lo
        response.push_back( static_cast<uint8_t>( ( crc >> 8 ) & 0xFF ) ); // CRC Hi

        DWORD written = 0;
        WriteFile( hCom, response.data(), static_cast<DWORD>( response.size() ),
                   &written, nullptr );
    }

    port.CloseCommPort();
    running_.store( false );
}

//---------------------------------------------------------------------------
}; // End of namespace Server
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
