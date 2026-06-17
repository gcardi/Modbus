# Modbus

## Modbus master/server library for Embarcadero compilers

<img src="Images/ClassHierarchy.svg?v=3" alt="Modbus class hierarchy diagram" style="max-width:100%; height:auto; pointer-events:none;">

## Overview

This repository hosts a Modbus library implemented in C++ for Embarcadero C++Builder and RAD Studio. The library supports Modbus master clients over RTU, TCP, and UDP, plus embedded Modbus slave/server implementations for TCP and RTU test or integration scenarios. It provides protocol-agnostic APIs for reading and writing Modbus registers and coils, and a server-side dispatch layer for handling incoming function-code requests.

For a deeper project-oriented reference, see [Technical Documentation](TECHNICAL_DOCS.md).

## Architecture

- `Modbus.h`: core types, exceptions, `Context` and abstract `Master::Protocol` interface.
    Implements the **Non-Virtual Interface (NVI)** pattern: all public methods are non-virtual
    and forward to protected `Do…()` virtual hooks in subclasses.
- `ModbusPDU.*`: shared helpers for Modbus PDU construction, parsing, and validation.
- `ModbusMBAP.*`: shared helpers for Modbus TCP/IP MBAP header framing.
- `ModbusRTU.*`: implementation of Modbus RTU over serial (`CommPort` helper, CRC, frame format).
- `ModbusTCP_IP.*`: shared Modbus TCP/MBAP framing and validation layer.
- `ModbusTCP.*`, `ModbusUDP.*`: marker base classes for TCP and UDP transports.
- `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`: Indy concrete classes (`TCPProtocolIndy`, `UDPProtocolIndy`).
- `ModbusTCP_WinSock.*`, `ModbusUDP_WinSock.*`: WinSock concrete classes (`TCPProtocolWinSock`, `UDPProtocolWinSock`).
- `ModbusServer.*`: abstract server/slave dispatch framework and TCP/IP server base.
- `ModbusServerTCP_WinSock.*`: WinSock TCP slave/server implementation.
- `ModbusServerRTU.*`: Win32 serial RTU slave/server implementation.
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
- `ReadCoilStatus()`, `ReadInputStatus()`
- `ReadHoldingRegisters()`, `ReadInputRegisters()`
- `ForceSingleCoil()`, `ForceMultipleCoils()`
- `PresetSingleRegister()`, `PresetMultipleRegisters()`, `MaskWrite4XRegister()`
- `ReadGeneralReference()`, `WriteGeneralReference()`
- `ReadWrite4XRegisters()`
- `ReadExceptionStatus()`, `Diagnostics()`, `ReadFIFOQueue()`

`SessionManager` RAII wrapper ensures connection lifecycle.

### Server::Protocol

Server-side classes expose a callback-driven slave implementation:

- `Modbus::Server::RequestHandler`: application callback interface for supported function codes.
- `Modbus::Server::Protocol`: shared dispatch engine that converts requests into handler calls.
- `Modbus::Server::TCPIPProtocol`: MBAP-framed server base for Modbus TCP.
- `Modbus::Server::TCPProtocolWinSock`: concrete WinSock TCP slave/server.
- `Modbus::Server::RTUProtocol`: concrete serial RTU slave/server.

Unsupported handler methods return `ExceptionCode::IllegalFunction` by default, so a server only needs to override the function codes it supports.

## Supported Modbus Function Codes

- FC01 Read Coil Status / FC02 Read Input Status
- FC03 Read Holding Registers / FC04 Read Input Registers
- FC05 Force Single Coil / FC15 Force Multiple Coils
- FC06 Preset Single Register / FC16 Preset Multiple Registers
- FC07 Read Exception Status
- FC08 Diagnostics (Return Query Data and other sub-functions)
- FC20 Read General Reference (File Records)
- FC21 Write General Reference (File Records)
- FC22 Mask Write 4X Register
- FC23 Read/Write 4X Registers
- FC24 Read FIFO Queue
- Standard exceptions: IllegalFunction, IllegalDataAddress, IllegalDataValue, SlaveDeviceFailure, etc.

## Addressing Convention

Important notes for this library:

- The Modbus protocol start-address field is a 16-bit unsigned value (`0..65535`).
- This library expects **zero-based** start addresses for all coil/register functions.
- The function you call already defines the table (coils, discrete inputs, input registers, or holding registers), so no table-leading digit is needed.
- If your device manual numbers items starting from 1, convert using: `library_address = manual_item_number - 1`.
- Some HMIs/tag editors may show formatted references (for example six digits), but this is display-only. The API input remains the numeric zero-based offset.

Examples (manual item number -> library address argument):

- `1` -> `0`
- `108` -> `107`

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

### Server/Slave Protocols

- `Modbus::Server::RequestHandler` maps incoming requests to application state.
- `Modbus::Server::TCPProtocolWinSock` listens for MBAP-framed TCP requests and responds through the shared dispatch engine.
- `Modbus::Server::RTUProtocol` listens on a COM port, validates RTU CRC frames, dispatches requests, and suppresses replies for broadcast frames.
- `Modbus::PDU` and `Modbus::MBAP` keep request/response framing shared between master and server code.

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

### 2) Embedded Modbus Server/Slave

Use the server classes when tests or applications need an in-process slave:

- Derive a class from `Modbus::Server::RequestHandler`.
- Override only the function-code callbacks your simulated device supports.
- Start `Modbus::Server::TCPProtocolWinSock` for TCP or `Modbus::Server::RTUProtocol` for serial RTU.
- For RTU tests, use paired virtual COM ports or physical serial ports.

Minimal TCP server example:

```cpp
#include "ModbusServerTCP_WinSock.h"

#include <array>
#include <iostream>
#include <optional>

class HoldingRegisterServer : public Modbus::Server::RequestHandler {
public:
    std::optional<Modbus::ExceptionCode> OnReadHoldingRegisters(
        Modbus::RegAddrType start,
        Modbus::RegCountType count,
        Modbus::RegDataType* data ) override
    {
        if ( start + count > registers.size() ) {
            return Modbus::ExceptionCode::IllegalDataAddress;
        }

        for ( Modbus::RegCountType i = 0; i < count; ++i ) {
            data[i] = registers[start + i];
        }

        return std::nullopt;
    }

private:
    std::array<Modbus::RegDataType, 4> registers { 10, 20, 30, 40 };
};

int main()
{
    HoldingRegisterServer handler;
    Modbus::Server::TCPProtocolWinSock server( handler );

    server.Start( 5020 );
    std::cout << "Modbus TCP server listening on port 5020. Press Enter to stop.\n";
    std::cin.get();
    server.Stop();
}
```

### 3) Integration Test Flow

1. Start slave emulator/device.
2. Run client demo.
3. Verify register reads match expected values.
4. Validate writes by reading registers back.

## Build & Usage

- Target C++Builder / RAD Studio (Windows).
- Add all sources to project: `Modbus.h/cpp`, `ModbusPDU.*`, `ModbusMBAP.*`, `ModbusRTU.*`, `ModbusTCP_IP.*`, `ModbusTCP.*`, `ModbusUDP.*`, `ModbusTCP_Indy.*`, `ModbusUDP_Indy.*`, `ModbusTCP_WinSock.*`, `ModbusUDP_WinSock.*`, `ModbusServer.*`, `ModbusServerTCP_WinSock.*`, `ModbusServerRTU.*`, `ModbusDummy.*`, `CommPort.*`, `SerEnum.*`.
- Required Indy units: `IdTCPClient`, `IdUDPClient`, `IdIOHandler`, `IdIOHandlerSocket`.
- Optional: `boost::crc` for RTU CRC.
- C++17 compatible compiler settings are recommended.

## Testing

- The primary test build is CMake + Ninja under `Test/CMakeLists.txt`.
- Build and run instructions live in `Test/README-cmake.md`.
- The test suite uses Boost.Test and includes an embedded TCP slave fixture.
- RTU round-trip tests are optional and require paired COM ports. Set `MODBUS_RTU_MASTER` and `MODBUS_RTU_SLAVE`, or answer the interactive prompt at startup.

## Contribution

- Fork, implement features in protocol abstraction.
- Add tests under the same module.
- Keep existing API stability.

## License

MIT (see `LICENSE`).
