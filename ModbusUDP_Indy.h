/**
 * @file ModbusUDP_Indy.h
 * @brief Modbus::Master::UDPProtocolIndy — Embarcadero Indy TIdUDPClient transport for Modbus UDP.
 *
 * @details Provides a Modbus UDP master implementation using the Embarcadero Indy
 *  TIdUDPClient component.  Response datagrams are cached in an internal buffer;
 *  DoRead() slices bytes from the cache so that TCPIPProtocol's multi-step read logic works
 *  transparently with datagram sockets.
 */

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

/**
 * @brief Embarcadero Indy TIdUDPClient-based UDP implementation of the Modbus master protocol.
 *
 * @details Uses a TIdUDPClient instance (managed via std::unique_ptr) for UDP datagram I/O.
 *  DoWrite() sends the MBAP request datagram and receives the server response into an internal
 *  buffer (recvBuffer_).  DoRead() then slices the requested bytes from that buffer.
 *  DoInputBufferClear() resets the buffer position so stale data is discarded before each
 *  new transaction.
 *
 *  @note Requires the Embarcadero Indy library (IndyCore, IndyProtocols, IndySystem).
 *        This transport is only available in C++Builder / RAD Studio environments.
 */
class UDPProtocolIndy : public UDPProtocol {
public:
    /**
     * @brief Constructs the protocol object with the specified server address.
     * @param Host Server hostname or IP address (default: "localhost").
     * @param Port Server UDP port (default: 502).
     */
    UDPProtocolIndy( String Host = String( DEFAULT_MODBUS_TCPIP_HOST ),
                     uint16_t Port = DEFAULT_MODBUS_TCPIP_PORT );
protected:
    virtual String DoGetProtocolName() const override { return _D( "Modbus UDP (Indy)" ); }

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
