//---------------------------------------------------------------------------

#pragma hdrstop

#include "ModbusTCP_WinSock.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma comment( lib, "ws2_32" )

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

TCPProtocolWinSock::TCPProtocolWinSock( String Host, uint16_t Port )
    : host_( Host ), port_( Port ), socket_( INVALID_SOCKET )
{
    WSADATA wsaData;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
        throw EBaseException( _D( "TCP: WSAStartup failed" ) );
    }
}
//---------------------------------------------------------------------------

TCPProtocolWinSock::~TCPProtocolWinSock()
{
    try {
        DoClose();
        WSACleanup();
    }
    catch ( ... ) {
    }
}
//---------------------------------------------------------------------------

String TCPProtocolWinSock::DoGetHost() const
{
    return host_;
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoSetHost( String Val )
{
    host_ = Val;
}
//---------------------------------------------------------------------------

uint16_t TCPProtocolWinSock::DoGetPort() const noexcept
{
    return port_;
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoSetPort( uint16_t Val )
{
    port_ = Val;
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoOpen()
{
    DoClose();

    ADDRINFOW hints = {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ADDRINFOW* result = nullptr;
    if ( GetAddrInfoW( host_.c_str(), String( port_ ).c_str(), &hints, &result ) != 0 ) {
        throw EBaseException( _D( "TCP: GetAddrInfoW failed" ) );
    }

    SOCKET sock = INVALID_SOCKET;
    for ( ADDRINFOW* ptr = result; ptr != nullptr; ptr = ptr->ai_next ) {
        sock = ::socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
        if ( sock == INVALID_SOCKET )
            continue;

        // Use non-blocking connect so we can apply a connect timeout
        u_long nonBlocking = 1;
        ioctlsocket( sock, FIONBIO, &nonBlocking );

        ::connect( sock, ptr->ai_addr, static_cast<int>( ptr->ai_addrlen ) );

        fd_set writeSet, errSet;
        FD_ZERO( &writeSet );
        FD_ZERO( &errSet );
        FD_SET( sock, &writeSet );
        FD_SET( sock, &errSet );
        timeval tv = { 5, 0 }; // 5 second connect timeout
        int sel = select( 0, nullptr, &writeSet, &errSet, &tv );

        u_long blocking = 0;
        ioctlsocket( sock, FIONBIO, &blocking );

        if ( sel <= 0 || FD_ISSET( sock, &errSet ) ) {
            closesocket( sock );
            sock = INVALID_SOCKET;
            continue;
        }
        break; // connected
    }
    FreeAddrInfoW( result );

    if ( sock == INVALID_SOCKET ) {
        throw EBaseException( _D( "TCP: connection failed" ) );
    }

    socket_ = sock;

    // 2 second read timeout
    DWORD readTimeout = 2000;
    setsockopt( socket_, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>( &readTimeout ), sizeof( readTimeout ) );
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoClose()
{
    if ( socket_ != INVALID_SOCKET ) {
        shutdown( socket_, SD_BOTH );
        closesocket( socket_ );
        socket_ = INVALID_SOCKET;
    }
}
//---------------------------------------------------------------------------

bool TCPProtocolWinSock::DoIsConnected() const noexcept
{
    return socket_ != INVALID_SOCKET;
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoInputBufferClear()
{
    u_long available = 0;
    ioctlsocket( socket_, FIONREAD, &available );
    while ( available > 0 ) {
        char buf[256];
        recv( socket_, buf, sizeof( buf ), 0 );
        ioctlsocket( socket_, FIONREAD, &available );
    }
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoWrite( TBytes const OutBuffer )
{
    const char* data   = reinterpret_cast<const char*>( &OutBuffer[0] );
    int         total  = 0;
    int         length = OutBuffer.Length;

    while ( total < length ) {
        int sent = send( socket_, data + total, length - total, 0 );
        if ( sent == SOCKET_ERROR ) {
            throw EBaseException( _D( "TCP: send failed" ) );
        }
        total += sent;
    }
}
//---------------------------------------------------------------------------

void TCPProtocolWinSock::DoRead( TBytes & InBuffer, size_t Length )
{
    InBuffer.Length = static_cast<int>( Length );
    char*  data     = reinterpret_cast<char*>( &InBuffer[0] );
    int    received = 0;

    while ( static_cast<size_t>( received ) < Length ) {
        int result = recv( socket_, data + received,
                           static_cast<int>( Length ) - received, 0 );
        if ( result <= 0 ) {
            throw EBaseException( _D( "TCP: read timeout or connection closed" ) );
        }
        received += result;
    }
}

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
