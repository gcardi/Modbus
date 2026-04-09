cmake_minimum_required(VERSION 3.27)
project(ModbusTest LANGUAGES CXX)

# Prefer Embarcadero bcc64x when the compiler is not explicitly set.
if(NOT CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER bcc64x CACHE STRING "C++ compiler" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Resolve RAD Studio root from environment (rsvars.bat) or fallback path.
set(BDS_ROOT "$ENV{BDS}")
if(BDS_ROOT STREQUAL "")
  set(BDS_ROOT "C:/Program Files (x86)/Embarcadero/Studio/37.0")
endif()

set(BOOST_INCLUDE_DIR "${BDS_ROOT}/include/boost_1_89")

set(MODBUS_TEST_SOURCES
  ../CommPort.cpp
  ../Modbus.cpp
  ../ModbusDummy.cpp
  ../ModbusRTU.cpp
  ../ModbusTCP_IP.cpp
  ../ModbusTCP_WinSock.cpp
  ModbusTest.cpp
)

add_executable(ModbusTest ${MODBUS_TEST_SOURCES})

target_compile_definitions(ModbusTest PRIVATE
  UNICODE
  _UNICODE
  FRAMEWORK_VCL
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

target_include_directories(ModbusTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${BOOST_INCLUDE_DIR}
)

# This codebase relies on VCL/TCHAR macros (_T, String helpers) coming from
# the existing project PCH header.
target_compile_options(ModbusTest PRIVATE
  -include${CMAKE_CURRENT_SOURCE_DIR}/ModbusTestPCH2.h
)

# Windows sockets and RTL/VCL runtime libraries used by these sources.
if(WIN32)
  target_link_libraries(ModbusTest PRIVATE ws2_32 rtl vcl)
  set(SYSINIT_OBJ "${BDS_ROOT}/lib/win64x/release/SysInit.o")
  if(EXISTS "${SYSINIT_OBJ}")
    target_link_libraries(ModbusTest PRIVATE "${SYSINIT_OBJ}")
  endif()
endif()

enable_testing()
add_test(NAME modbus_tests COMMAND ModbusTest)
