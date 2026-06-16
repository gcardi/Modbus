# Modbus Technical Documentation

Doxygen reference: [Detailed API Documentation](https://gcardi.github.io/Modbus/index.html)

## 1. Scope

This document provides technical guidance for the Modbus repository with emphasis on:

- Library architecture and module responsibilities
- Test suite structure
- Build workflow for Embarcadero C++Builder with CMake + Ninja
- Doxygen/API documentation generation
- Recent migration notes (`_T` to `_D`)
- Common troubleshooting steps

## 2. Repository Architecture

### 2.1 Core Protocol Abstractions

- Modbus.h / Modbus.cpp
  - Core types, context, exception hierarchy, base protocol behavior
- ModbusRTU.h / ModbusRTU.cpp
  - RTU frame and serial protocol implementation
- ModbusPDU.h / ModbusPDU.cpp
  - Shared Modbus PDU request/response helpers
- ModbusMBAP.h / ModbusMBAP.cpp
  - Shared MBAP header helpers for TCP/IP framing
- ModbusTCP_IP.h / ModbusTCP_IP.cpp
  - TCP framing and shared IP transport logic

### 2.2 Transport Implementations

- ModbusTCP_Indy.h / ModbusTCP_Indy.cpp
  - TCP transport using Indy
- ModbusUDP_Indy.h / ModbusUDP_Indy.cpp
  - UDP transport using Indy
- ModbusTCP_WinSock.h / ModbusTCP_WinSock.cpp
  - TCP transport using WinSock
- ModbusUDP_WinSock.h / ModbusUDP_WinSock.cpp
  - UDP transport using WinSock
- ModbusServer.h / ModbusServer.cpp
  - Server/slave RequestHandler interface, shared dispatch engine, and TCP/IP server base
- ModbusServerTCP_WinSock.h / ModbusServerTCP_WinSock.cpp
  - TCP slave/server transport using WinSock
- ModbusServerRTU.h / ModbusServerRTU.cpp
  - RTU slave/server transport using CommPort

### 2.3 Support Modules

- CommPort.h / CommPort.cpp
  - Serial communication utilities
- SerEnum.h / SerEnum.cpp
  - Serial port enumeration
- ModbusDummy.h / ModbusDummy.cpp
  - Dummy protocol implementation

## 3. Test Suite

### 3.1 Primary Test Target

- Test/ModbusTest.cpp
  - Main Boost.Test suite and embedded server integration tests
  - Covers FC01/FC02/FC03/FC04/FC05/FC06/FC07/FC08/FC15/FC16/FC20/FC21/FC22/FC23/FC24
  - Includes endpoint coverage for TCP/IP, Dummy, and RTU
  - Uses an embedded TCP server fixture and optional RTU round-trip fixture

### 3.2 CMake Test Project

- Test/CMakeLists.txt
  - Primary test build path using CMake and Ninja
- Test/README-cmake.md
  - Practical commands and usage instructions

## 4. Build Workflows

### 4.1 CMake + Ninja (Release default)

Configure and build:

```cmd
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake -S Test -B Test/build/win64x-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=bcc64x
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake --build Test/build/win64x-release -v
```

Run tests:

```cmd
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && ctest --test-dir Test/build/win64x-release --output-on-failure
```

Implementation details in Test/CMakeLists.txt:

- Uses bcc64x compiler
- Forces include of Test/ModbusTestPCH2.h for VCL/TCHAR-related macro availability
- Links required runtime/system libs and SysInit.o
- Builds the full current test suite, including PDU/MBAP helpers and TCP/RTU server sources

### 4.2 Doxygen/API documentation

Generate API documentation:

```powershell
& "C:\Program Files\doxygen\bin\doxygen.exe" Doxyfile
```

Output is written to `docs/`. The Doxygen main page is `README.md`, and the class hierarchy diagram source is `Images/ClassHierarchy.dot`, rendered as `Images/ClassHierarchy.svg`.

## 5. Macro Migration Notes (`_T` to `_D`)

The codebase was migrated from `_T(...)` to `_D(...)`.

### 5.1 Why `_D`

- `_D` is provided by Embarcadero RTL in include/windows/rtl/sysmac.h (via System.hpp)
- It maps string literals to the native Delphi character width

### 5.2 Important Constraint

`_D` must wrap string literals, not macro identifiers.

Correct examples:

- `_D("Modbus TCP")`
- `String(DEFAULT_MODBUS_TCPIP_HOST)`

Incorrect example:

- `_D(DEFAULT_MODBUS_TCPIP_HOST)`

## 6. Known Pitfalls and Fixes

### 6.1 unit_test_main linker mismatch (Boost.Test)

Symptom:

- Undefined symbol for boost::unit_test::unit_test_main with incompatible callback signature

Fix path used:

- Align test runner startup pattern with current Boost.Test usage in source
- Avoid mixed old/new init APIs

### 6.2 SysInit.o link error in CMake build

Symptom:

- Unable to link files from Delphi without SysInit.o

Fix:

- Explicitly link:
  - C:/Program Files (x86)/Embarcadero/Studio/37.0/lib/win64x/release/SysInit.o

### 6.3 Missing toolchain environment

Symptom:

- bcc64x not recognized

Fix:

- Always run rsvars.bat in the same command session before building

## 6.4 Address notation vs protocol address field

In the actual Modbus PDU, the start address is a 16-bit unsigned value (`0..65535`) and is interpreted as an offset.
This library uses that offset directly (zero-based) for coil/register API calls.
The API method already identifies the target table, so no leading table digit is required in the address value.

Practical mapping:

- First item in a table (`1`) maps to library address `0`
- Item `108` in that table maps to library address `107`

Some external tools display formatted references, but this is only a representation format. The library API expects the numeric zero-based Modbus offset.

## 7. Branching Workflow (Recommended)

To move pending work to develop branch safely:

```powershell
git stash push -u -m "WIP-before-branch-switch"
git switch develop
# or: git switch -c develop

git stash pop
```

## 8. Suggested Maintenance Practices

- Keep the CMake test build path healthy:
  - Test/CMakeLists.txt
- Validate Release build regularly (not only Debug)
- Run CTest after changes in transport/protocol code
- Keep Test/README-cmake.md synchronized with actual commands
- Keep Images/ClassHierarchy.dot synchronized with class hierarchy changes, then regenerate Images/ClassHierarchy.svg with Graphviz
- Regenerate Doxygen output after public API or Doxygen comment changes

## 9. Quick Verification Checklist

- Configure succeeds with rsvars + CMake + Ninja
- Release build succeeds
- CTest returns 100% pass
- No `_T(...)` usages remain in source files
- Default host constructor arguments use `String(DEFAULT_MODBUS_TCPIP_HOST)`
- Doxygen completes without new warnings for documented public headers
