//---------------------------------------------------------------------------

#ifndef ModbusTCP_IndyH
#define ModbusTCP_IndyH

#include <IdIOHandlerStack.hpp>
#include <IdBaseComponent.hpp>
#include <IdComponent.hpp>
#include <IdIOHandler.hpp>
#include <IdIOHandlerSocket.hpp>
#include <IdTCPClient.hpp>
#include <IdTCPConnection.hpp>

#include <memory>
#include <cstdint>

#include "ModbusTCP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class TCPProtocolIndy : public TCPProtocol {
public:
    TCPProtocolIndy( String Host = _T( DEFAULT_MODBUS_TCPIP_HOST ),
                     uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );
protected:
    virtual String DoGetProtocolName() const override { return _T( "Modbus TCP (Indy)" ); }
    virtual String DoGetHost() const override;
    virtual void DoSetHost( String Val ) override;
    virtual uint16_t DoGetPort() const override;
    virtual void DoSetPort( uint16_t Val ) override;
    virtual void DoOpen() override;
    virtual void DoClose() override;
    virtual bool DoIsConnected() const override;
    virtual void DoInputBufferClear() override;
    virtual void DoWrite( TBytes const OutBuffer ) override;
    virtual void DoRead( TBytes & InBuffer, size_t Length ) override;
private:
    std::unique_ptr<Idtcpclient::TIdTCPClient> idTCPClient_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
