//---------------------------------------------------------------------------

#pragma hdrstop

#include "ModbusTCP_Indy.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

TCPProtocolIndy::TCPProtocolIndy( String Host, uint16_t Port )
    : idTCPClient_( new TIdTCPClient( 0 ) )
{
    idTCPClient_->ConnectTimeout = 5000;
    idTCPClient_->ReadTimeout = 2000;

    DoSetHost( Host );
    DoSetPort( Port );
}
//---------------------------------------------------------------------------

String TCPProtocolIndy::DoGetHost() const
{
    return idTCPClient_->Host;
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoSetHost( String Val )
{
    idTCPClient_->Host = Val;
}
//---------------------------------------------------------------------------

uint16_t TCPProtocolIndy::DoGetPort() const
{
    return idTCPClient_->Port;
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoSetPort( uint16_t Val )
{
    idTCPClient_->Port = Val;
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoOpen()
{
    idTCPClient_->Connect();
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoClose()
{
    idTCPClient_->Disconnect( true );
}
//---------------------------------------------------------------------------

bool TCPProtocolIndy::DoIsConnected() const
{
    return idTCPClient_.get() && idTCPClient_->IOHandler &&
           idTCPClient_->IOHandler->Connected();
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoInputBufferClear()
{
    idTCPClient_->IOHandler->InputBuffer->Clear();
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoWrite( TBytes const OutBuffer )
{
//DebugBytesToHex( _T( "TCP TX: " ), OutBuffer );
    idTCPClient_->IOHandler->Write( OutBuffer, OutBuffer.Length, OutBuffer.Low );
}
//---------------------------------------------------------------------------

void TCPProtocolIndy::DoRead( TBytes & InBuffer, size_t Length )
{
    idTCPClient_->IOHandler->ReadBytes( InBuffer, Length, false );
//DebugBytesToHex( _T( "TCP RX: " ), InBuffer );
}

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

