//---------------------------------------------------------------------------

#pragma hdrstop

#include <algorithm>

#include "ModbusUDP_Indy.h"

using std::min;

//---------------------------------------------------------------------------
#pragma package(smart_init)

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

UDPProtocolIndy::UDPProtocolIndy( String Host, uint16_t Port )
    : idUDPClient_( new TIdUDPClient( 0 ) )
{
    idUDPClient_->ReceiveTimeout = 2000;

    DoSetHost( Host );
    DoSetPort( Port );
}
//---------------------------------------------------------------------------

String UDPProtocolIndy::DoGetHost() const
{
    return idUDPClient_->Host;
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoSetHost( String Val )
{
    idUDPClient_->Host = Val;
}
//---------------------------------------------------------------------------

uint16_t UDPProtocolIndy::DoGetPort() const
{
    return idUDPClient_->Port;
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoSetPort( uint16_t Val )
{
    idUDPClient_->Port = Val;
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoOpen()
{
    idUDPClient_->Active = true;
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoClose()
{
    idUDPClient_->Active = false;
}
//---------------------------------------------------------------------------

bool UDPProtocolIndy::DoIsConnected() const
{
    return idUDPClient_->Active;
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoInputBufferClear()
{
    recvBufferPos_ = 0;
    recvBufferSize_ = 0;
    recvBuffer_.Length = 2048;
    idUDPClient_->ReceiveBuffer( recvBuffer_, 0 );
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoWrite( TBytes const OutBuffer )
{
#if defined( _DEBUG )
DebugBytesToHex( _T( "UDP TX: " ), OutBuffer );
#endif
    idUDPClient_->SendBuffer( OutBuffer );
    recvBufferSize_ = idUDPClient_->ReceiveBuffer( recvBuffer_ );
}
//---------------------------------------------------------------------------

void UDPProtocolIndy::DoRead( TBytes & InBuffer, size_t Length )
{
    if ( recvBufferSize_ - recvBufferPos_ < Length ) {
        throw EBaseException( _T( "UDP read timeout" ) );
    }
    InBuffer = recvBuffer_.CopyRange( recvBufferPos_, Length );
    recvBufferPos_ += Length;
#if defined( _DEBUG )
DebugBytesToHex( _T( "UDP RX: " ), InBuffer );
#endif
}

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
