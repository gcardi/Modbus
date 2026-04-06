# Modbus

## Modbus master library for Embarcadero compilers

<a href="https://github.com/gcardi/Modbus#readme">
  <img src="https://raw.githubusercontent.com/gcardi/Modbus/5a3574a0d60edea218482e14501e780f6e2f9aa2/Images/ClassHierarchy.svg" width="100%" height="512">
</a>

## Overview

This repository hosts a Modbus Master library implemented in C++ for Embarcadero C++Builder and RAD Studio. The library supports Modbus RTU, Modbus TCP/UDP and a dummy protocol. It provides a protocol-agnostic API for reading and writing Modbus registers and coils from a master application.

## Architecture

- `Modbus.h`: core types, exceptions, `Context` and `Master::Protocol` interface.
- `ModbusRTU.*`: implementation of Modbus RTU over serial (`CommPort` helper, CRC, frame format).
- `ModbusTCP_IP.*`: Modbus TCP/BMAP frame layer with validation and parsing.
- `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`: Indy-based TCP/UDP transport classes.
- `ModbusTCP_WinSock.*`, `ModbusUDP_WinSock.*`: WinSock2-based TCP/UDP transport classes.
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
- `Modbus::Master::TCPProtocolIndy` / `Modbus::Master::TCPProtocolWinSock` (TCP)
- `Modbus::Master::UDPProtocolIndy` / `Modbus::Master::UDPProtocolWinSock` (UDP)
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
- For RTU, use virtual COM ports or actual serial.<br>
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


