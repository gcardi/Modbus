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
 * @brief Marker base class for TCP-based Modbus master implementations (NVI pattern).
 *
 *  **Role in NVI Hierarchy:**
 *  - Inherits all MBAP framing logic and virtual hooks from TCPIPProtocol.
 *  - Concrete TCP transport classes (TCPProtocolWinSock, TCPProtocolIndy) derive from TCPProtocol
 *    and implement the Do…() virtual methods using their respective I/O libraries (WinSock2 or Indy).
 *  - Provides no additional virtual methods; all behavior is inherited from Protocol and TCPIPProtocol.
 *
 *  Use this class:
 *  - As a base (polymorphic reference) when you need to accept any TCP transport without
 *    knowing whether it's WinSock2 or Indy.
 *  - As a template parameter in generic TCP-specific code.
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
