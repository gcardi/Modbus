//---------------------------------------------------------------------------
#if defined(__BORLANDC__)
#pragma hdrstop
#endif

#include "ModbusServerTCP_WinSock.h"

#pragma comment(lib, "ws2_32")

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Server {
//---------------------------------------------------------------------------

TCPProtocolWinSock::TCPProtocolWinSock( RequestHandler& Handler )
    : TCPIPProtocol( Handler )
{
    WSADATA wsaData;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 )
        throw EBaseException( _D( "WSAStartup failed" ) );
}

TCPProtocolWinSock::~TCPProtocolWinSock()
{
    Stop();
    WSACleanup();
}

//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoStart( uint16_t Port )
{
    stop_.store( false );
    running_.store( true );
    std::promise<bool> readyPromise;
    auto readyFuture = readyPromise.get_future();
    thread_ = std::thread( &TCPProtocolWinSock::ServeLoop, this, Port, std::move( readyPromise ) );
    readyFuture.wait(); // block until the listen socket is bound and listening
}

void TCPProtocolWinSock::DoStop()
{
    stop_.store( true );
    if ( thread_.joinable() )
        thread_.join();
    running_.store( false );
}

bool TCPProtocolWinSock::DoIsRunning() const
{
    return running_.load();
}

//---------------------------------------------------------------------------

void TCPProtocolWinSock::ServeLoop( uint16_t Port, std::promise<bool> ReadyPromise )
{
    SOCKET listenSock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( listenSock == INVALID_SOCKET ) {
        running_.store( false );
        ReadyPromise.set_value( false );
        return;
    }

    int reuseAddr = 1;
    setsockopt( listenSock, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>( &reuseAddr ), sizeof( reuseAddr ) );

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons( Port );
    addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if ( bind( listenSock, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ) == SOCKET_ERROR ||
         listen( listenSock, SOMAXCONN ) == SOCKET_ERROR )
    {
        closesocket( listenSock );
        running_.store( false );
        ReadyPromise.set_value( false );
        return;
    }

    ReadyPromise.set_value( true ); // port is bound and listening — DoStart() may return

    while ( !stop_.load() ) {
        fd_set readSet;
        FD_ZERO( &readSet );
        FD_SET( listenSock, &readSet );
        timeval tv = { 0, 100'000 }; // 100 ms poll interval

        if ( select( 0, &readSet, nullptr, nullptr, &tv ) > 0 ) {
            sockaddr_in clientAddr = {};
            int         addrLen    = sizeof( clientAddr );
            SOCKET      clientSock = accept( listenSock,
                                             reinterpret_cast<sockaddr*>( &clientAddr ),
                                             &addrLen );
            if ( clientSock != INVALID_SOCKET ) {
                DWORD recvTimeout = 2000; // 2 s
                setsockopt( clientSock, SOL_SOCKET, SO_RCVTIMEO,
                            reinterpret_cast<const char*>( &recvTimeout ),
                            sizeof( recvTimeout ) );
                while ( !stop_.load() && ServeClient( clientSock ) ) {}
                closesocket( clientSock );
            }
        }
    }

    closesocket( listenSock );
    running_.store( false );
}

//---------------------------------------------------------------------------

static bool RecvAll( SOCKET s, uint8_t* buf, int len )
{
    int done = 0;
    while ( done < len ) {
        int r = recv( s, reinterpret_cast<char*>( buf + done ), len - done, 0 );
        if ( r <= 0 ) return false;
        done += r;
    }
    return true;
}

static bool SendAll( SOCKET s, const uint8_t* buf, int len )
{
    int done = 0;
    while ( done < len ) {
        int r = send( s, reinterpret_cast<const char*>( buf + done ), len - done, 0 );
        if ( r == SOCKET_ERROR ) return false;
        done += r;
    }
    return true;
}

bool TCPProtocolWinSock::ServeClient( SOCKET ClientSock )
{
    // Read 6-byte MBAP prefix: TID(2) + PID(2) + LEN(2)
    uint8_t prefix[6];
    if ( !RecvAll( ClientSock, prefix, 6 ) ) return false;

    uint16_t mbapLen = static_cast<uint16_t>( ( prefix[4] << 8 ) | prefix[5] );
    if ( mbapLen < 2 ) return false; // at minimum UnitId + FC

    // Read remainder of frame (UnitId + PDU)
    std::vector<uint8_t> frame;
    frame.resize( 6 + mbapLen );
    std::copy( prefix, prefix + 6, frame.begin() );
    if ( !RecvAll( ClientSock, frame.data() + 6, static_cast<int>( mbapLen ) ) )
        return false;

    std::vector<uint8_t> response = ProcessFrame( frame );
    if ( response.empty() ) return false;

    return SendAll( ClientSock, response.data(), static_cast<int>( response.size() ) );
}

//---------------------------------------------------------------------------
}; // End of namespace Server
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
