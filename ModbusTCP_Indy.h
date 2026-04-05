/**
 * @file ModbusTCP_Indy.h
 * @brief Modbus::Master::TCPProtocolIndy — Embarcadero Indy TIdTCPClient transport for Modbus TCP.
 *
 * @details Provides a Modbus TCP master implementation using the Embarcadero Indy
 *  (Internet Direct) TIdTCPClient component.  This is the preferred transport when
 *  building C++Builder / RAD Studio VCL applications that already use the Indy library.
 */

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

/**
 * @brief Embarcadero Indy TIdTCPClient-based TCP implementation of the Modbus master protocol.
 *
 * @details Uses a TIdTCPClient instance (managed via std::unique_ptr) for the TCP connection.
 *  Indy handles hostname resolution, connection management, and blocking I/O internally,
 *  making this implementation straightforward and VCL-friendly.
 *
 *  @note Requires the Embarcadero Indy library (IndyCore, IndyProtocols, IndySystem).
 *        This transport is only available in C++Builder / RAD Studio environments.
 */
class TCPProtocolIndy : public TCPProtocol {
public:
    /**
     * @brief Constructs the protocol object with the specified server address.
     * @param Host Server hostname or IP address (default: "localhost").
     * @param Port Server TCP port (default: 502).
     */
    TCPProtocolIndy( String Host = String( DEFAULT_MODBUS_TCPIP_HOST ),
                     uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );
protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus TCP (Indy)" ); }
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
