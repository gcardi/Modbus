# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### MSBuild (Primary)

Must source `rsvars.bat` first to set up the Embarcadero compiler environment. The installed toolchain can be discovered from the Registry — the `RootDir` value under the numerically highest BDS subkey contains the path:

```text
HKLM\SOFTWARE\WOW6432Node\Embarcadero\BDS\<ver>\RootDir
```

PowerShell one-liner to resolve the latest `rsvars.bat` dynamically:

```powershell
$rsvars = (Get-ItemProperty (Get-ChildItem 'HKLM:\SOFTWARE\WOW6432Node\Embarcadero\BDS' |
    Sort-Object { [double]$_.PSChildName } | Select-Object -Last 1).PSPath).RootDir + 'bin\rsvars.bat'
```

Currently installed versions (all verified on disk):

| BDS key | RAD Studio release | RootDir |
| ------- | ------------------ | ------- |
| 21.0 | 10.4 Sydney | `C:\Program Files (x86)\Embarcadero\Studio\21.0\` |
| 22.0 | 11 Alexandria | `C:\Program Files (x86)\Embarcadero\Studio\22.0\` |
| 23.0 | 12 Athens | `C:\Program Files (x86)\Embarcadero\Studio\23.0\` |
| 37.0 | 13 Florence | `C:\Program Files (x86)\Embarcadero\Studio\37.0\` |

Build using the latest (37.0 / Florence):

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && msbuild .\Test\ModbusTest.cbproj /t:Build /p:Config=Debug /p:Platform=Win64x /v:minimal'
```

### CMake + Ninja (Alternative)

```powershell
cmake -S Test -B Test/build/win64x-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=bcc64x
cmake --build Test/build/win64x-release -v
```

### Run Tests

```powershell
ctest --test-dir Test/build/win64x-release --output-on-failure
```

### Generate API Docs

```powershell
& "C:\Program Files\doxygen\bin\doxygen.exe" Doxyfile
```

Output goes to `docs/`.

## Architecture

This is a **Modbus Master library** for Embarcadero C++Builder/RAD Studio targeting Windows. It implements a layered, protocol-agnostic design:

```text
Application
    │
Protocol (abstract base, Modbus.h)
    ├── RTUProtocol       — Serial RTU over COM port (ModbusRTU.h)
    │       └── TCommPort — Win32 serial wrapper (CommPort.h)
    ├── TCPIPProtocol     — MBAP framing base (ModbusTCP_IP.h)
    │       ├── TCPProtocolIndy     — Indy TIdTCPClient (ModbusTCP_Indy.h)
    │       ├── TCPProtocolWinSock  — WinSock2 TCP (ModbusTCP_WinSock.h)
    │       ├── UDPProtocolIndy     — Indy TIdUDPClient (ModbusUDP_Indy.h)
    │       └── UDPProtocolWinSock  — WinSock2 UDP (ModbusUDP_WinSock.h)
    └── DummyProtocol     — No-op stub for tests (ModbusDummy.h)
```

**`Modbus::Master::Protocol`** is the abstract base. Subclasses implement protected `Do*()` virtual hooks for transport I/O and connection management. Public methods (FC03, FC04, FC06, FC16, FC22) are implemented in the base class on top of those hooks.

**`Modbus::Master::SessionManager`** is an RAII guard: calls `Open()` on construction, `Close()` on destruction.

**`Modbus::Context`** carries the slave address (unit ID). `TCPIPContext` extends it with an explicit MBAP transaction ID.

**Exception hierarchy** (all derive from VCL `Exception` → `EBaseException`):

- `EContextException` — carries a `Context`
- `EProtocolException` — adds an `ExceptionCode`
- `EProtocolStdException<ExceptionCode>` — typed aliases: `EIllegalFunction`, `EIllegalDataAddress`, `EIllegalDataValue`, `ESlaveDeviceFailure`, `EAcknowledge`, `ESlaveDeviceBusy`, `ENegativeAcknowledge`, `EMemoryParityError`

**`SerEnum.h`** provides `Modbus::Utils::EnumSerialPort<OutputIterator, Functor>()` — queries Windows SetupAPI (`GUID_DEVINTERFACE_COMPORT`) to enumerate available COM ports.

## Test Suite

`Test/ModbusTest.cpp` uses **Boost.Test 1.89** (`boost/test/included/unit_test.hpp`). A `ServerFixture` global fixture starts an embedded Modbus slave on `127.0.0.1:5020` in a `std::thread` before tests run and stops it after. Tests cover FC03, FC04, FC06, FC16, FC22, and exception handling.

CMake test build details and known pitfalls (SysInit.o linking, PCH2 force-include) are documented in [Test/README-cmake.md](Test/README-cmake.md) and [TECHNICAL_DOCS.md](TECHNICAL_DOCS.md).

## Code Conventions

**String literals:** Use `_D("...")` macro (Embarcadero RTL), not `_T("...")`. `_D()` wraps only string literals — never macro identifiers:

- Correct: `_D("Modbus TCP")`, `String(DEFAULT_MODBUS_TCPIP_HOST)`
- Incorrect: `_D(DEFAULT_MODBUS_TCPIP_HOST)`

**Namespaces:**

- `Modbus::` — types, exceptions, `Context`
- `Modbus::Master::` — `Protocol`, `SessionManager`, all transport classes
- `Modbus::Utils::` — serial enumeration (`SerEnum.h`)

**Compiler compatibility:** `[[noreturn]]` attribute placement differs between BCC32 and BCC32C/BCC64 — see existing exception class declarations for the guarded pattern.

**C++ standard:** C++23 (CMake build), C++17 minimum.
