//---------------------------------------------------------------------------

#ifndef ModbusUDP_IndyH
#define ModbusUDP_IndyH

#include <IdBaseComponent.hpp>
#include <IdComponent.hpp>
#include <IdUDPBase.hpp>
#include <IdUDPClient.hpp>

#include <memory>
#include <cstdint>

#include "ModbusUDP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class UDPProtocolIndy : public UDPProtocol {
public:
    UDPProtocolIndy( String Host = _T( DEFAULT_MODBUS_TCPIP_HOST ),
                     uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );
protected:
    virtual String DoGetProtocolName() const override { return _T( "Modbus UDP (Indy)" ); }

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
    std::unique_ptr<Idudpclient::TIdUDPClient> idUDPClient_;
    TBytes recvBuffer_;
    int recvBufferPos_ { 0 };
    int recvBufferSize_ { 0 };
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#endif
