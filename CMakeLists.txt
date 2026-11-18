cmake_minimum_required(VERSION 3.16)
project(motor_control_cpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O2)
endif()

# Include directories
include_directories(include)

# Common library sources
set(COMMON_SOURCES
    src/ModbusRtuClient.cpp
    src/ArduinoSerial.cpp
    src/MotorController.cpp
)

# Main executable
add_executable(motor_control
    src/main.cpp
    ${COMMON_SOURCES}
)

# Test executable
enable_testing()
add_executable(test_modbus
    tests/test_modbus.cpp
    src/ModbusRtuClient.cpp
    src/MotorController.cpp
)

add_test(NAME modbus_unit_tests COMMAND test_modbus)
