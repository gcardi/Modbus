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
 * @brief Marker base class for UDP-based Modbus master implementations (NVI pattern).
 *
 *  **Role in NVI Hierarchy:**
 *  - Inherits all MBAP framing logic and virtual hooks from TCPIPProtocol.
 *  - Concrete UDP transport classes (UDPProtocolWinSock, UDPProtocolIndy) derive from UDPProtocol
 *    and implement the Do…() virtual methods using their respective I/O libraries (WinSock2 or Indy).
 *  - Provides no additional virtual methods; all behavior is inherited from Protocol and TCPIPProtocol.
 *
 *  UDP-Specific Behavior:
 *  - UDP is connectionless: DoWrite() sends a datagram and DoRead() retrieves bytes from a
 *    locally cached response datagram.
 *  - DoInputBufferClear() resets the receive cache between transactions.
 *
 *  Use this class as a base (polymorphic reference) when you need to accept any UDP transport
 *  polymorphically without caring about the underlying socket library.
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
