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

`Test/ModbusTest.cpp` is the main Boost.Test suite. It uses an embedded TCP slave started
as a global fixture, covers all 16 Modbus function codes implemented by the library, and
includes endpoint coverage for TCP/IP, Dummy, and RTU transports.

### 3.2 CMake Test Project

- `Test/CMakeLists.txt` — primary test build path using CMake and Ninja
- `Test/README-cmake.md` — practical commands and usage instructions

### 3.3 Server Register State

`initRegisters()` seeds all data tables with deterministic patterns. It is called once by
the global `ServerFixture` at test-run start, and again by `ProtoFixture` at the start of
each test suite, so each suite sees a clean slate.

| Table | Initial value at index `i` | Pattern |
| --- | --- | --- |
| `coilRegs[i]` | `i & 1` | Alternating 0, 1, 0, 1, ... |
| `inputBits[i]` | `(i % 3) == 0` | 1, 0, 0, 1, 0, 0, ... |
| `holdingRegs[i]` | `i` | Identity mapping |
| `inputRegs[i]` | `0x1000 + i` | Fixed base offset |
| `fileRecords[f][r]` | `((f+1) << 8) \| r` | 4 files × 32 records |
| `exceptionStatus` | `0x6D` | Fixed known byte (FC07) |
| `fifoQueue[i]` | `0x100 + i` | 5 entries pre-loaded; `fifoCount = 5` (FC24) |

All tables have 256 entries (`REG_COUNT`). The FIFO maximum is 31 entries (`FIFO_MAX`).

### 3.4 Fixture Architecture

Three fixture layers cooperate to set up and tear down the test environment:

**`ServerFixture` (global)** — Starts a `Modbus::Server::TCPProtocolWinSock` slave on
`127.0.0.1:5020` backed by a `TestRequestHandler`. Runs once for the entire test session.

**`RTUServerFixture` (global)** — Optional. Reads `MODBUS_RTU_MASTER` and
`MODBUS_RTU_SLAVE` environment variables at startup; falls back to an interactive console
prompt when the variables are not set. When enabled, starts a
`Modbus::Server::RTUProtocol` slave on the specified port at 9600 baud.

**`ProtoFixture` (per-suite)** — Opens a fresh `TCPProtocolWinSock` connection to
`127.0.0.1:5020` and wraps it in a `SessionManager`. Calls `initRegisters()` to reset
shared register state. Isolation is per-suite, not per-test-case: tests within a suite
share the reset state and can observe each other's writes.

**`RTUMasterFixture` (per-suite)** — Opens `RTUProtocol` on `gRTUMasterPort` at 9600 baud.
Each test case guards with `if (!gRTUEnabled) return;` so the suite is silently skipped
when RTU is not configured.

**`TestRequestHandler`** — The `Modbus::Server::RequestHandler` implementation that backs
both the TCP and RTU slaves. All FC handlers enforce bounds and return
`IllegalDataAddress` on overflow. FC08 (Diagnostics) echoes the data word back for every
sub-function (simplified slave — does not maintain bus counters or statistics). FC21
(WriteGeneralReference) validates all sub-requests before applying any writes.

### 3.5 Test Helper Functions

Four thin wrappers read a single item and unpack it where necessary:

| Function | FC | Returns |
| --- | --- | --- |
| `readH(proto, addr)` | FC03 | One holding register value |
| `readI(proto, addr)` | FC04 | One input register value |
| `readC(proto, addr)` | FC01 | One coil state (0 or 1, unpacked from packed LSB-first byte) |
| `readD(proto, addr)` | FC02 | One input bit state (0 or 1, unpacked from packed LSB-first byte) |

All four accept an optional `slave` argument (default 1).

### 3.6 Test Suite Reference

All TCP suites below run against the embedded WinSock slave via `ProtoFixture`.

#### TCP/IP suites

| Suite | Test case | What is verified |
| --- | --- | --- |
| **FC01_ReadCoilStatus** | SingleCoilAlternatingPattern | Reads coils 0–3 individually; expects `0, 1, 0, 1` from the `i & 1` init pattern |
| | PackedBitOrderLSBFirst | Reads 16 coils into a 2-byte buffer; verifies LSB-first packing produces `{0xAA, 0xAA}` |
| | OutOfRangeThrows | Address 255 + count 2 overflows 256-entry table; expects `EBaseException` |
| **FC02_ReadInputStatus** | SingleInputModuloPattern | Reads input bits 0–3 individually; expects `1, 0, 0, 1` from the `(i%3)==0` init pattern |
| | PackedBitOrderLSBFirst | Reads 16 input bits into a 2-byte buffer; verifies packing produces `{0x49, 0x92}` |
| | OutOfRangeThrows | Address 255 + count 2 overflows; expects `EBaseException` |
| **FC03_ReadHoldingRegisters** | SingleRegisterEqualsAddress | Reads registers 0, 1, 10; expects identity values from `holdingRegs[i] = i` |
| | BlockReadSequentialValues | Reads 8 registers starting at 20; verifies sequential values 20–27 |
| | BoundaryAddress | Reads register 255 (last valid); expects 255 |
| | OutOfRangeThrows | Address 255 + count 2 overflows; expects `EBaseException` |
| **FC04_ReadInputRegisters** | SingleRegisterEqualsBase | Reads registers 0, 1, 255; expects `0x1000`, `0x1001`, `0x10FF` |
| | BlockReadSequentialValues | Reads 4 registers starting at 5; verifies values `0x1005`–`0x1008` |
| | OutOfRangeThrows | Address 255 + count 2 overflows; expects `EBaseException` |
| **FC05_ForceSingleCoil** | ForceOnAndReadBack | Forces coil 0 (initially 0) ON; reads back via FC01 and verifies 1 |
| | ForceOffAndReadBack | Forces coil 1 (initially 1) OFF; reads back and verifies 0 |
| | ToggleCoil | Forces coil 10 ON then OFF; verifies 1 then 0 with FC01 reads |
| | OutOfRangeThrows | Address 256 exceeds table; expects `EBaseException` |
| **FC06_PresetSingleRegister** | WriteAndReadBack | Writes `0xABCD` to register 50; reads back via FC03 and verifies |
| | OverwriteWithZero | Writes `0xABCD` then `0x0000` to same address; verifies zero overwrites |
| | BoundaryAddress | Writes `0x1234` to register 255 (last valid); reads back and verifies |
| | OutOfRangeThrows | Address 256 exceeds table; expects `EBaseException` |
| **FC07_ReadExceptionStatus** | ReturnsKnownPattern | Reads exception status; expects fixed init value `0x6D` |
| | ReturnTypeIsUint8 | Verifies `sizeof(ExceptionStatusDataType) == 1` |
| **FC08_Diagnostics** | EchoTestReturnsData | Sends `ReturnQueryData` sub-function with `0xABCD`; verifies echo |
| | EchoTestZero | Echo with `0x0000`; verifies zero is returned |
| | EchoTestMaxValue | Echo with `0xFFFF`; verifies max value is returned |
| | SubFunctionEchoed | Sends `ReturnBusMessageCount` sub-function; verifies no exception (server echoes all sub-functions) |
| **FC15_ForceMultipleCoils** | ForceBlockAndReadBack | Forces coils 0–7 all ON (`0xFF`); reads back 8 coils and verifies `0xFF` |
| | ForcePatternAndReadBack | Forces coils 16–31 to `{0xA5, 0x5A}`; reads back and verifies pattern preserved |
| | ForceAllOff | Forces coils 0–7 all OFF (`0x00`); reads back and verifies `0x00` |
| | OutOfRangeThrows | Address 255 + count 2 overflows; expects `EBaseException` |
| **FC16_PresetMultipleRegisters** | BlockRoundTrip | Writes `{0x0100, 0x0200, 0x0300}` to registers 60–62; reads back via FC03 and verifies all three |
| | SingleRegisterViaFC16 | Writes single value `0xBEEF` via FC16; reads back via FC03 and verifies |
| | OutOfRangeThrows | Address 255 + count 2 overflows; expects `EBaseException` |
| **FC20_ReadGeneralReference** | SingleSubRequest_ReadsCorrectValues | Reads file 1 records 0–2; expects `0x0100, 0x0101, 0x0102` from init pattern |
| | MultipleSubRequests_ConcatenatedOutput | Two sub-requests (file 1 rec 0–1, file 2 rec 4–6); verifies 5 concatenated output words |
| | InvalidFileNumber_Throws | File number 99 does not exist; expects `EBaseException` |
| **FC21_WriteGeneralReference** | WriteThenReadBack | Writes `{0xAAAA, 0xBBBB}` to file 1 records 10–11; reads back via FC20 and verifies |
| | MultipleSubRequests_WriteThenVerify | Writes one word each to file 1 rec 20 and file 2 rec 20; reads each back independently |
| | InvalidFileNumber_Throws | File number 99 does not exist; expects `EBaseException` |
| **FC22_MaskWrite4XRegister** | MaskApplied | Presets `0xAA55`; applies AND=`0xFF00`, OR=`0x00AA`; expects `0xAAAA` from `(val & AND) \| (OR & ~AND)` |
| | ChainedMasks | Presets `0xAAAA`; applies AND=`0x0F0F`, OR=`0xF0F0`; expects `0xFAFA` |
| | IdentityMaskLeavesValueUnchanged | Applies AND=`0xFFFF`, OR=`0x0000`; value must be unchanged |
| | ZeroAndMaskForcesOrMask | Applies AND=`0x0000`, OR=`0x1234`; entire word is replaced by the OR mask value |
| **FC23_ReadWrite4XRegisters** | WriteAndReadSameRegion | Writes `0xAAAA` to address 100 and reads address 100 in the same call; verifies write-before-read ordering |
| | WriteThenReadDifferentRegions | Writes to addresses 50–51; reads addresses 0–2 (initially `0, 1, 2`); verifies both independently |
| | WriteBeforeReadSemantics | Writes `0xBEEF` to address 200 and reads address 200 in same call; verifies Modbus-spec write-before-read |
| | OutOfRangeReadThrows | Read range starting at address 255 with count 2 overflows; expects `EBaseException` |
| | OutOfRangeWriteThrows | Write range starting at address 255 with count 2 overflows; expects `EBaseException` |
| **FC24_ReadFIFOQueue** | ReadsExpectedCount | Reads FIFO at address 0; verifies returned count is 5 (pre-loaded value) |
| | ReadsExpectedValues | Reads FIFO; verifies all 5 entries are `0x100`–`0x104` |
| | EmptyFIFO | Sets `fifoCount = 0` directly; verifies returned count is 0 and output buffer is untouched |
| | InvalidAddressThrows | Address 256 exceeds table; expects `EBaseException` |
| **TransactionId** | LowTidEchoed | Issues FC03 with `TCPIPContext(1, 0x0001)`; verifies no exception (server must echo TID in MBAP header) |
| | MaxTidEchoed | Issues FC03 with `TCPIPContext(1, 0xFFFF)`; verifies no exception at maximum TID value |

#### Endpoint variation suites

These suites instantiate the transport directly and do not use `ProtoFixture`.

| Suite | Test case | What is verified |
| --- | --- | --- |
| **FC01_FC02_DummyEndpoint** | DummyReadCoilsLeavesBufferUnchanged | Calls FC01 on `DummyProtocol`; output buffer preset to `{0x5A, 0xA5}` is unchanged after the call |
| | DummyReadInputsLeavesBufferUnchanged | Calls FC02 on `DummyProtocol`; output buffer preset to `{0x33, 0xCC}` is unchanged |
| **FC01_FC02_RTUEndpoint** | ReadCoilsWithoutOpenPortThrows | Calls FC01 on `RTUProtocol` with no COM port open; expects `Exception` |
| | ReadInputsWithoutOpenPortThrows | Calls FC02 on `RTUProtocol` with no COM port open; expects `Exception` |

#### RTU round-trip suite

Uses `RTUMasterFixture`; each test case silently returns if `gRTUEnabled` is false (set by
`RTUServerFixture` at startup based on env vars or interactive prompt). Expected values
mirror the TCP suites exactly since both share the same `TestRequestHandler` and
`initRegisters()` state.

| Suite | Test case | What is verified |
| --- | --- | --- |
| **RTU_RoundTrip** | ReadCoilStatus | Reads 16 coils; expects packed bytes `{0xAA, 0xAA}` |
| | ReadInputStatus | Reads 16 input bits; expects `{0x49, 0x92}` |
| | ReadHoldingRegisters | Reads 4 registers starting at address 10; expects `10, 11, 12, 13` |
| | ReadInputRegisters | Reads 3 registers starting at address 5; expects `0x1005, 0x1006, 0x1007` |
| | ForceSingleCoil | Forces coil 4 ON then OFF; reads back each state via FC01 and verifies |
| | PresetSingleRegister | Writes `0xABCD` to register 20; reads back via FC03 and verifies |
| | ReadExceptionStatus | Reads exception status; expects `0x6D` |
| | Diagnostics | Sends `ReturnQueryData` with `0x1234`; verifies echo |
| | ForceMultipleCoils | Forces coils 0–15 to `{0xF0, 0x0F}`; reads back and verifies pattern |
| | PresetMultipleRegisters | Writes `{100, 200, 300}` to registers 30–32; reads back and verifies all three |
| | MaskWrite4XRegister | Register 10 starts at `0x000A`; applies AND=`0xFF00`, OR=`0x00F0`; expects `0x00F0` |
| | ReadWrite4XRegisters | Writes `{0xABCD, 0xEF01}` to registers 50–51 and reads same region in one call; verifies write-before-read |

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
