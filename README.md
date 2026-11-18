# C++ Motor Control System (DA200A Modbus RTU + Arduino Interlock)

A high-performance, native **C++17** implementation for controlling the **DA200A Vector Control Variable Frequency Drive (VFD)** via Modbus serial RTU, with an **Arduino ultrasonic sensor safety interlock**.

This project provides an exact, zero-dependency C++ counterpart to the Python continuous motor control script.

---

## Features
- **Zero External Library Dependencies**: Native POSIX serial (`termios`) and custom high-speed Modbus RTU frame generator with CRC-16 hardware checksum.
- **Continuous Motor Control**: Continuous heartbeat ensures VFD watchdog timeout never trips.
- **Safety Interlock**: Automatically polls the Arduino ultrasonic sensor. If an obstacle is detected within 30 cm, motors brake to 0 RPM immediately. Once the obstacle is cleared for 2 seconds, motors automatically resume.
- **Mock Mode**: Pass `--mock` to simulate and test control logic safely without physical hardware attached.
- **Clean Signal Handling**: Graceful stop on `Ctrl+C` (SIGINT/SIGTERM): commands `0 RPM`, disables bus control, and closes ports cleanly.
- **Dual Motor Support**: Seamlessly coordinates Left and Right motor drivers.

---

## Directory Structure
```
Motor-Control-Cpp/
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # Documentation
├── include/
│   ├── ArduinoSerial.hpp       # Non-blocking Arduino serial driver
│   ├── IModbusClient.hpp       # Abstract Modbus client interface
│   ├── MockModbusClient.hpp    # Mock Modbus driver for dry-run testing
│   ├── ModbusRtuClient.hpp     # Native POSIX Modbus RTU serial client
│   └── MotorController.hpp     # DA200A register map and speed commands
├── src/
│   ├── ArduinoSerial.cpp
│   ├── ModbusRtuClient.cpp
│   ├── MotorController.cpp
│   └── main.cpp                # CLI entry point and main control loop
└── tests/
    └── test_modbus.cpp         # Unit tests (CRC-16, frame builders, mock)
```

---

## Building

### Prerequisites
- GCC (`g++`) 9+ or Clang 10+ (supporting C++17)
- CMake 3.16+
- Make

### Build Steps
```bash
cd /home/gio/Motor-Control-Cpp
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Running Unit Tests
```bash
./test_modbus
# or with ctest:
ctest --output-on-failure
```

---

## Usage

```bash
./motor_control [options]
```

### Options
| Option | Default | Description |
|---|---|---|
| `--port <path>` | — | Shared port for both motors (alias for `--port-left` and `--port-right`) |
| `--port-left <path>` | `/dev/ttyUSB_LEFT` | Serial port for the left motor driver |
| `--port-right <path>` | `/dev/ttyUSB_RIGHT` | Serial port for the right motor driver |
| `--rpm <int>` | `1000` | Target RPM for the motors |
| `--baudrate <int>` | `19200` | Modbus serial baudrate |
| `--parity <N\|E\|O>` | `N` | Modbus serial parity (`N`=None, `E`=Even, `O`=Odd) |
| `--stopbits <1\|2>` | `1` | Modbus serial stop bits |
| `--device-id <int>` | `1` | Modbus slave ID of the motor drive |
| `--arduino-port <path>`| — | Serial port for Arduino ultrasonic safety interlock |
| `--arduino-baudrate <int>`| `9600` | Baudrate for Arduino serial port |
| `--mock` | `false` | Run in mock mode without physical hardware |
| `-h`, `--help` | — | Display help message and exit |

---

## Examples

### 1. Test in Mock Mode (Safe dry-run)
```bash
./motor_control --mock --rpm 500
```

### 2. Live Hardware Run (Bench test at 200 RPM)
```bash
./motor_control \
  --port-left /dev/ttyUSB_LEFT \
  --port-right /dev/ttyUSB_RIGHT \
  --arduino-port /dev/ttyACM0 \
  --rpm 200
```

### 3. Full Speed Production Run (1000 RPM)
```bash
./motor_control \
  --port-left /dev/ttyUSB_LEFT \
  --port-right /dev/ttyUSB_RIGHT \
  --arduino-port /dev/ttyACM0 \
  --rpm 1000
```

### 4. Single Shared Port
```bash
./motor_control --port /dev/ttyUSB0 --arduino-port /dev/ttyACM0 --rpm 500
```

