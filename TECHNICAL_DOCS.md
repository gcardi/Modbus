# Modbus Technical Documentation

## 1. Scope

This document provides technical guidance for the Modbus repository with emphasis on:

- Library architecture and module responsibilities
- Test suite structure
- Build workflows for Embarcadero MSBuild and CMake + Ninja
- Recent migration notes (_T to _D)
- Common troubleshooting steps

## 2. Repository Architecture

### 2.1 Core Protocol Abstractions

- Modbus.h / Modbus.cpp
  - Core types, context, exception hierarchy, base protocol behavior
- ModbusRTU.h / ModbusRTU.cpp
  - RTU frame and serial protocol implementation
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

### 3.2 Legacy Project (RAD Studio)

- Test/ModbusTest.cbproj
  - C++Builder/MSBuild test project

### 3.3 CMake Test Project

- Test/CMakeLists.txt
  - Alternative test build path using CMake and Ninja
- Test/README-cmake.md
  - Practical commands and usage instructions

## 4. Build Workflows

### 4.1 Embarcadero MSBuild

Always initialize environment first:

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && msbuild .\Test\ModbusTest.cbproj /t:Build /p:Config=Debug /p:Platform=Win64x /v:minimal'
```

Notes:
- The rsvars.bat call is required so bcc64x, linker, and SDK paths are configured.
- Without rsvars.bat, msbuild and toolchain discovery can fail.

### 4.2 CMake + Ninja (Release default)

Configure and build:

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake -S Test -B Test/build/win64x-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=bcc64x'
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake --build Test/build/win64x-release -v'
```

Run tests:

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && ctest --test-dir Test/build/win64x-release --output-on-failure'
```

Implementation details in Test/CMakeLists.txt:
- Uses bcc64x compiler
- Forces include of Test/ModbusTestPCH2.h for VCL/TCHAR-related macro availability
- Links required runtime/system libs and SysInit.o

## 5. Macro Migration Notes (_T to _D)

The codebase was migrated from _T(...) to _D(...).

### 5.1 Why _D

- _D is provided by Embarcadero RTL in include/windows/rtl/sysmac.h (via System.hpp)
- It maps string literals to the native Delphi character width

### 5.2 Important Constraint

_D must wrap string literals, not macro identifiers.

Correct examples:
- _D("Modbus TCP")
- String(DEFAULT_MODBUS_TCPIP_HOST)

Incorrect example:
- _D(DEFAULT_MODBUS_TCPIP_HOST)

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
- msbuild or bcc64x not recognized

Fix:
- Always run rsvars.bat in the same command session before building

## 7. Branching Workflow (Recommended)

To move pending work to develop branch safely:

```powershell
git stash push -u -m "WIP-before-branch-switch"
git switch develop
# or: git switch -c develop

git stash pop
```

## 8. Suggested Maintenance Practices

- Keep both test build paths healthy:
  - Test/ModbusTest.cbproj
  - Test/CMakeLists.txt
- Validate Release build regularly (not only Debug)
- Run CTest after changes in transport/protocol code
- Keep Test/README-cmake.md synchronized with actual commands

## 9. Quick Verification Checklist

- Configure succeeds with rsvars + CMake + Ninja
- Release build succeeds
- CTest returns 100% pass
- No _T(...) usages remain in source files
- Default host constructor arguments use String(DEFAULT_MODBUS_TCPIP_HOST)
