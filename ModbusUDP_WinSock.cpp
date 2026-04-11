//---------------------------------------------------------------------------

#pragma hdrstop

#include <algorithm>

#include "ModbusUDP_WinSock.h"

using std::min;

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma comment( lib, "ws2_32" )

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

UDPProtocolWinSock::UDPProtocolWinSock( String Host, uint16_t Port )
    : host_( Host ), port_( Port ), socket_( INVALID_SOCKET )
    , serverAddrLen_( 0 )
    , recvBufferPos_( 0 ), recvBufferSize_( 0 )
{
    ZeroMemory( &serverAddr_, sizeof( serverAddr_ ) );
    recvBuffer_.Length = 2048;

    WSADATA wsaData;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
        throw EBaseException( _D( "UDP: WSAStartup failed" ) );
    }
}
//---------------------------------------------------------------------------

UDPProtocolWinSock::~UDPProtocolWinSock()
{
    try {
        DoClose();
        WSACleanup();
    }
    catch ( ... ) {
    }
}
//---------------------------------------------------------------------------

String UDPProtocolWinSock::DoGetHost() const
{
    return host_;
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoSetHost( String Val )
{
    host_ = Val;
}
//---------------------------------------------------------------------------

uint16_t UDPProtocolWinSock::DoGetPort() const noexcept
{
    return port_;
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoSetPort( uint16_t Val )
{
    port_ = Val;
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoOpen()
{
    DoClose();

    ADDRINFOW hints = {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    ADDRINFOW* result = nullptr;
    if ( GetAddrInfoW( host_.c_str(), String( port_ ).c_str(), &hints, &result ) != 0 ) {
        throw EBaseException( _D( "UDP: GetAddrInfoW failed" ) );
    }

    SOCKET sock = INVALID_SOCKET;
    for ( ADDRINFOW* ptr = result; ptr != nullptr; ptr = ptr->ai_next ) {
        sock = ::socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
        if ( sock == INVALID_SOCKET )
            continue;

        // Store resolved server address for sendto — guard against oversized entries
        if ( ptr->ai_addrlen > sizeof( serverAddr_ ) ) {
            closesocket( sock );
            sock = INVALID_SOCKET;
            continue;
        }
        memcpy( &serverAddr_, ptr->ai_addr, ptr->ai_addrlen );
        serverAddrLen_ = static_cast<int>( ptr->ai_addrlen );
        break;
    }
    FreeAddrInfoW( result );

    if ( sock == INVALID_SOCKET ) {
        throw EBaseException( _D( "UDP: socket creation failed" ) );
    }

    socket_ = sock;

    // 2 second receive timeout
    DWORD readTimeout = 2000;
    setsockopt( socket_, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>( &readTimeout ), sizeof( readTimeout ) );
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoClose()
{
    if ( socket_ != INVALID_SOCKET ) {
        closesocket( socket_ );
        socket_ = INVALID_SOCKET;
    }
}
//---------------------------------------------------------------------------

bool UDPProtocolWinSock::DoIsConnected() const noexcept
{
    return socket_ != INVALID_SOCKET;
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoInputBufferClear()
{
    recvBufferPos_  = 0;
    recvBufferSize_ = 0;

    // Drain any pending datagrams without blocking
    u_long available = 0;
    ioctlsocket( socket_, FIONREAD, &available );
    while ( available > 0 ) {
        char buf[2048];
        sockaddr_storage from;
        int fromLen = sizeof( from );
        recvfrom( socket_, buf, sizeof( buf ), 0,
                  reinterpret_cast<sockaddr*>( &from ), &fromLen );
        ioctlsocket( socket_, FIONREAD, &available );
    }
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoWrite( TBytes const OutBuffer )
{
#if defined( _DEBUG )
    DebugBytesToHex( _D( "UDP TX: " ), OutBuffer );
#endif
    const char* data   = reinterpret_cast<const char*>( &OutBuffer[0] );
    int         length = OutBuffer.Length;

    if ( sendto( socket_, data, length, 0,
                 reinterpret_cast<const sockaddr*>( &serverAddr_ ),
                 serverAddrLen_ ) == SOCKET_ERROR ) {
        throw EBaseException( _D( "UDP: sendto failed" ) );
    }

    // Receive the response immediately — mirrors the Indy pattern
    recvBuffer_.Length = 2048;
    sockaddr_storage from;
    int fromLen = sizeof( from );
    int received = recvfrom( socket_,
                             reinterpret_cast<char*>( &recvBuffer_[0] ),
                             recvBuffer_.Length, 0,
                             reinterpret_cast<sockaddr*>( &from ), &fromLen );
    if ( received == SOCKET_ERROR ) {
        throw EBaseException( _D( "UDP: recvfrom failed (timeout?)" ) );
    }

    recvBufferPos_  = 0;
    recvBufferSize_ = received;
}
//---------------------------------------------------------------------------

void UDPProtocolWinSock::DoRead( TBytes & InBuffer, size_t Length )
{
    if ( recvBufferSize_ - recvBufferPos_ < static_cast<int>( Length ) ) {
        throw EBaseException( _D( "UDP read timeout" ) );
    }
    InBuffer = recvBuffer_.CopyRange( recvBufferPos_, Length );
    recvBufferPos_ += static_cast<int>( Length );
#if defined( _DEBUG )
    DebugBytesToHex( _D( "UDP RX: " ), InBuffer );
#endif
}

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
