# Modbus

## Modbus master library for Embarcadero compilers

![diagram](Images/ClassHierarchy.svg?v=2)

## Overview

This repository hosts a Modbus Master library implemented in C++ for Embarcadero C++Builder and RAD Studio. The library supports Modbus RTU, Modbus TCP/UDP and a dummy protocol. It provides a protocol-agnostic API for reading and writing Modbus registers and coils from a master application.

For a deeper project-oriented reference, see [Technical Documentation](TECHNICAL_DOCS.md).

## Architecture

 - `Modbus.h`: core types, exceptions, `Context` and abstract `Master::Protocol` interface.
     - Implements the **Non-Virtual Interface (NVI)** pattern: all public methods are non-virtual
         and forward to protected `Do…()` virtual hooks in subclasses.
- `ModbusRTU.*`: implementation of Modbus RTU over serial (`CommPort` helper, CRC, frame format).
- `ModbusTCP_IP.*`: shared Modbus TCP/MBAP framing and validation layer.
- `ModbusTCP.*`, `ModbusUDP.*`: marker base classes for TCP and UDP transports.
- `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`: Indy concrete classes (`TCPProtocolIndy`, `UDPProtocolIndy`).
- `ModbusTCP_WinSock.*`, `ModbusUDP_WinSock.*`: WinSock concrete classes (`TCPProtocolWinSock`, `UDPProtocolWinSock`).
- `ModbusDummy.*`: no-op implementation for testing.
- `CommPort.*`: serial control layer for RTU.
- `SerEnum.*`: serial port enumeration utilities.

## Key Concepts

### Modbus::Context

Encapsulates slave unit ID and transaction identifier for request/response correlation.

- `SlaveAddrType`: uint8_t
- `TransactionIdType`: uint16_t

### Master::Protocol

Abstract base class exposing:

- `Open()` / `Close()` / `IsConnected()`
- `GetProtocolName()` / `GetProtocolParamsStr()`
- `ReadHoldingRegisters()`, `ReadInputRegisters()`
- `PresetSingleRegister()`, `PresetMultipleRegisters()`, `MaskWrite4XRegister()`

`SessionManager` RAII wrapper ensures connection lifecycle.

## Supported Modbus Function Codes

- Read holding/input registers
- Preset single/multiple registers
- Mask write registers
- Standard exceptions: IllegalFunction, IllegalDataAddress, IllegalDataValue, SlaveDeviceFailure, etc.

## Protocol Implementations

### Modbus RTU

- `Modbus::Master::RTUProtocol`
- Serial config: `CommPort`, `CommSpeed`, `CommParity`, `CommBits`, `CommStopBits`.
- Retries via `RetryCount`; timeout via `TimeoutValue`.
- CRC-16 and frame-level logic.

### Modbus TCP/IP

- `Modbus::Master::TCPIPProtocol` MBAP framing layer
- Marker base classes: `Modbus::Master::TCPProtocol` and `Modbus::Master::UDPProtocol`
- Indy concrete classes: `Modbus::Master::TCPProtocolIndy` (TCP), `Modbus::Master::UDPProtocolIndy` (UDP)
- WinSock concrete classes: `Modbus::Master::TCPProtocolWinSock` (TCP), `Modbus::Master::UDPProtocolWinSock` (UDP)
- Defaults: host `localhost`, port `502`.

### Dummy Protocol

- `Modbus::Master::DummyProtocol`
- No actual transport; useful for unit tests and placeholder behavior.

## Demonstration Projects

### 1) Modbus Master Client Demo

Use this to test read/write operations against a Modbus slave.

Example:

```cpp
#include "ModbusTCP_Indy.h"
#include "Modbus.h"

using namespace Modbus;
using namespace Modbus::Master;

int main() {
    TCPProtocolIndy master("192.168.0.10", 502);
    SessionManager session(master);

    Context ctx(1, 1); // slave address 1, transaction 1
    RegDataType regs[10];

    master.ReadHoldingRegisters(ctx, 0, 10, regs);
    for (int i = 0; i < 10; ++i) {
        printf("R[%d]=%u\n", i, regs[i]);
    }

    master.PresetSingleRegister(ctx, 0, 12345);
    return 0;
}
```

### 2) Modbus Server/Slave Demo

For a server-demo, run an emulator or physical slave, then connect from the client.

- Use Modbus slave tools like `modbuspoll`, `mbslave`, `CAS Modbus Scanner`.
- For RTU, use virtual COM ports or actual serial.
- Emulate holding registers and validate client reads/writes.

### 3) Integration Test Flow

1. Start slave emulator/device.
2. Run client demo.
3. Verify register reads match expected values.
4. Validate writes by reading registers back.

## Build & Usage

- Target C++Builder / RAD Studio (Windows).
- Add all sources to project: `Modbus.h/cpp`, `ModbusRTU.*`, `ModbusTCP_IP.*`, `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`, `ModbusTCP_WinSock.*`, `ModbusUDP_WinSock.*`, `ModbusDummy.*`, `CommPort.*`, `SerEnum.*`.
- Required Indy units: `IdTCPClient`, `IdUDPClient`, `IdIOHandler`, `IdIOHandlerSocket`.
- Optional: `boost::crc` for RTU CRC.
- C++17 compatible compiler settings are recommended.

## Testing

- Use Modbus slave simulator to verify opearations.
- Create small harness for protocol validation and exception cases.

## Contribution

- Fork, implement features in protocol abstraction.
- Add tests under the same module.
- Keep existing API stability.

## License

MIT (see `LICENSE`).
