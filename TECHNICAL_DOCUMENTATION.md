# Modbus C++ Master Library (Embarcadero)

## Overview

This repository hosts a Modbus Master library implemented in C++ for Embarcadero C++Builder and RAD Studio. The library supports Modbus RTU, Modbus TCP/UDP and a dummy protocol. It provides a protocol-agnostic API for reading and writing Modbus registers and coils from a master application.


## Architecture

- `Modbus.h`: core types, exceptions, `Context` and `Master::Protocol` interface.
- `ModbusRTU.*`: implementation of Modbus RTU over serial (`CommPort` helper, CRC, frame format).
- `ModbusTCP_IP.*`: modbus TCP+BMAP frame layer with validation and parsing.
- `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`: Indy-based TCP/UDP transport classes.
- `ModbusDummy.*`: a no-op implementation for testing or project scaffolding.
- `CommPort.*`: serial control layer (settings, flow, send/receive) for RTU.
- `SerEnum.*`: serial port enumeration utilities.


## Key Concepts

### Modbus::Context

Encapsulates slave unit ID and transaction identifier for Modbus request/response correlation.

- `SlaveAddrType`: uint8_t
- `TransactionIdType`: uint16_t

### Master::Protocol

Abstract base class exposing:

- `Open()` / `Close()` / `IsConnected()`
- `GetProtocolName()` / `GetProtocolParamsStr()`
- Read operations: `ReadHoldingRegisters`, `ReadInputRegisters`
- Write operations: `PresetSingleRegister`, `PresetMultipleRegisters`, `MaskWrite4XRegister`

A `SessionManager` RAII wrapper ensures connection lifecycle management.


## Supported Modbus Function Codes

- Read holding/input registers
- Preset single/multiple registers
- Mask write registers

Exception classes map standard Modbus exception codes:
- `IllegalFunction`, `IllegalDataAddress`, `IllegalDataValue`, `SlaveDeviceFailure`, etc.


## Protocol Implementations

### Modbus RTU

- Class: `Modbus::Master::RTUProtocol`
- Serial configuration properties: `CommPort`, `CommSpeed`, `CommParity`, `CommBits`, `CommStopBits`.
- Retries and timeout handling with `RetryCount` and `TimeoutValue`.
- Frame creation/parsing and CRC-16.

### Modbus TCP/IP

- Class: `Modbus::Master::TCPIPProtocol` (abstract BMAP layer)
- Class: `Modbus::Master::TCPProtocolIndy` (Indy TCP client)
- Class: `Modbus::Master::UDPProtocolIndy` (Indy UDP client)
- Default host: `localhost`, port: `502`.

### Dummy Protocol

- Class: `Modbus::Master::DummyProtocol`
- No transport; it lets applications compile and run without real comms.


## Demonstration Projects

The following demo projects show how to integrate the library in a master/client scenario and how to test with a server/PLC.

### 1) Modbus Master Client Demo (recommended)

Purpose: exercise common read/write operations against a Modbus slave.

Pseudo-code:

```cpp
#include "ModbusTCP_Indy.h"
#include "Modbus.h"

using namespace Modbus;
using namespace Modbus::Master;

int main() {
    TCPProtocolIndy master( "192.168.0.10", 502 );
    SessionManager session( master );

    Context ctx( 1, 1 ); // slave address 1, transaction id 1

    RegDataType regs[10];
    master.ReadHoldingRegisters( ctx, 0, 10, regs );

    // print values
    for( int i = 0; i < 10; ++i ) {
        printf("R[%d]=%u\n", i, regs[i] );
    }

    master.PresetSingleRegister( ctx, 0, 12345 );

    return 0;
}
```

Key points:
- Configure host/port before open.
- Use `Context` for slave selection and transaction ID.
- Wrap with `SessionManager` or release connection with `Close()`.


### 2) Modbus Polling/Control Server Demo (real slave)

This project is a sample Modbus RTU/TCP slave emulation and can run on the same PC for development.

Suggested approach:
- Using a third-party Modbus slave simulator (e.g., `modbuspoll`, `mbslave`, `CAS Modbus Scanner`) to expose registers on port 502.
- For RTU, use virtual COM ports (`com0com`) and set same port and serial parameters.

If you need a full source server demonstration, create an independent program that:
- Listens on TCP/UDP on port 502
- Unpacks BMAP header + PDU
- Handles functions such as read/write registers
- Sends appropriate Modbus exception codes on invalid requests.


### 3) Integration test flow

1. Start a Modbus slave emulator or physical device.
2. Run the client demo.
3. Validate that read register results are correct.
4. Verify write operations using slave tool or by reading back values.


## Build & Usage

- Target: C++Builder / RAD Studio (Windows).
- Add all sources to a VCL/Console project:
  - `Modbus.h/cpp`, `ModbusRTU.h/cpp`, `ModbusTCP_Indy.h/cpp`, `ModbusUDP_Indy.h/cpp`, `ModbusDummy.h/cpp`, `CommPort.*`, `SerEnum.*`.
- Required libraries: `IdTCPClient`, `IdUDPClient`, `IdIOHandler`, `IdSSLOpenSSL` if using secure transports.
- Optional: `boost_crc` for Modbus RTU CRC, included via `boost/crc.hpp`.

Compile options: Use `C++Builder` updated with C++17 or compatible.


## Testing

- Functional tests against a known slave simulator (e.g., `modbus slave` with known register map).
- Unit tests for parser/exception logic can be coded in a small harness.


## Contribution

- Fork repository.
- Add features with a clear API for new Modbus function codes.
- Follow the existing architecture and `Protocol` abstraction.


## License

- MIT (see `LICENSE`)
