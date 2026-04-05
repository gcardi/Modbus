/**
 * @file ModbusTCP.h
 * @brief Modbus::Master::TCPProtocol — TCP marker class in the MBAP protocol hierarchy.
 *
 * @details TCPProtocol is a thin intermediate class that identifies TCP-based transports
 *  in the class hierarchy without adding behaviour.  Concrete implementations (TCPProtocolWinSock,
 *  TCPProtocolIndy) derive from this class and provide the actual TCP socket I/O.
 */

//---------------------------------------------------------------------------

#ifndef ModbusTCPH
#define ModbusTCPH

#include "ModbusTCP_IP.h"

//---------------------------------------------------------------------------
namespace Modbus {
//---------------------------------------------------------------------------
namespace Master {
//---------------------------------------------------------------------------

/**
 * @brief Marker base class for TCP-based Modbus master implementations.
 *
 * @details Inherits all MBAP framing logic from TCPIPProtocol.  Concrete TCP transport
 *  classes (TCPProtocolWinSock, TCPProtocolIndy) derive from TCPProtocol and implement
 *  the DoOpen()/DoClose()/DoWrite()/DoRead() methods using their respective I/O libraries.
 *
 *  Use this class as a base-class reference when you need to accept any TCP transport
 *  polymorphically without caring about the underlying socket library.
 */
class TCPProtocol : public TCPIPProtocol {
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
