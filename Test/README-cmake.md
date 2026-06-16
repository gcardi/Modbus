# ModbusTest with CMake + Ninja

This folder provides the primary build path for the test executable using CMake and Ninja.

## Prerequisites

- RAD Studio/C++Builder 12.3 toolchain installed
- `cmake` and `ninja` available in PATH
- Boost headers supplied by the RAD Studio installation

## Configure and build (Command Prompt)

Run from repository root.

```cmd
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake -S Test -B Test/build/win64x-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=bcc64x
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake --build Test/build/win64x-release -v
```

## Run tests

```cmd
Test\build\win64x-release\ModbusTest.exe
```

Or with CTest:

```cmd
call "C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && ctest --test-dir Test/build/win64x-release --output-on-failure
```

## RTU round-trip tests

RTU tests are optional because they require paired COM ports. To run them non-interactively, set both environment variables before starting the test executable or CTest:

```cmd
set MODBUS_RTU_MASTER=COM10
set MODBUS_RTU_SLAVE=COM11
```

If the variables are not set, the test executable asks whether to enable the RTU COM-port tests at startup.
