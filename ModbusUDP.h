/**
 * @file ModbusUDP.h
 * @brief Modbus::Master::UDPProtocol — UDP marker class in the MBAP protocol hierarchy.
 *
 * @details UDPProtocol is a thin intermediate class that identifies UDP-based transports
 *  in the class hierarchy without adding behaviour.  Concrete implementations (UDPProtocolWinSock,
 *  UDPProtocolIndy) derive from this class and provide the actual UDP socket I/O.
 */

//---------------------------------------------------------------------------

#ifndef ModbusUDPH
#define ModbusUDPH

#include "ModbusTCP_IP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief Marker base class for UDP-based Modbus master implementations.
 *
 * @details Inherits all MBAP framing logic from TCPIPProtocol.  Concrete UDP transport
 *  classes (UDPProtocolWinSock, UDPProtocolIndy) derive from UDPProtocol and implement
 *  DoOpen()/DoClose()/DoWrite()/DoRead() using their respective I/O libraries.
 *
 *  @note UDP is connectionless; DoWrite() sends a datagram and DoRead() retrieves bytes
 *        from a locally cached response datagram.  DoInputBufferClear() resets the cache.
 */
class UDPProtocol : public TCPIPProtocol {
public:
protected:
private:
};

//---------------------------------------------------------------------------
}; // End of namespace Master
//---------------------------------------------------------------------------
}; // End of namespace Modbus
//---------------------------------------------------------------------------
#endif
