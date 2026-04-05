# ModbusTest with CMake + Ninja

This folder provides an alternative build path for the test executable using CMake and Ninja.

## Prerequisites

- RAD Studio/C++Builder 12.3 toolchain installed
- `cmake` and `ninja` available in PATH

## Configure and build (PowerShell)

Run from repository root.

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake -S Test -B Test/build/win64x-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=bcc64x'
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && cmake --build Test/build/win64x-release -v'
```

## Run tests

```powershell
Test\build\win64x-release\ModbusTest.exe
```

Or with CTest:

```powershell
cmd /d /c '"C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\rsvars.bat" && ctest --test-dir Test/build/win64x-release --output-on-failure'
```
