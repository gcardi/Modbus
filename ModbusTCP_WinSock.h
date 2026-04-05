//---------------------------------------------------------------------------

#ifndef ModbusTCP_WinSockH
#define ModbusTCP_WinSockH

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>

#include "ModbusTCP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

class TCPProtocolWinSock : public TCPProtocol {
public:
    TCPProtocolWinSock( String Host = String( DEFAULT_MODBUS_TCPIP_HOST ),
                        uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );
    ~TCPProtocolWinSock();
protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus TCP (WinSock)" ); }
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
    String   host_;
    uint16_t port_;
    SOCKET   socket_;
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
