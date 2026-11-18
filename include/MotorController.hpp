#pragma once

#include "IModbusClient.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace DA200A {
    constexpr uint16_t REG_BUS_SOURCE = 1820;       // P4.10: Bus command source
    constexpr uint16_t REG_BUS_ENABLE = 1822;       // P4.11: Bus enable control
    constexpr uint16_t REG_SPEED_CMD  = 1826;       // P4.13: Target speed command (32-bit)
    constexpr uint16_t REG_ACCEL_TIME = 1108;       // P0.54: Acceleration time (0.1s)
    constexpr uint16_t REG_DECEL_TIME = 1110;       // P0.55: Deceleration time (0.1s)
    constexpr uint8_t  DEFAULT_DEVICE_ID = 1;       // Default Modbus slave ID
    constexpr int      VFD_RPM_SCALE  = 10;          // Speed commands are written in 0.1 RPM units
}

struct MotorClientTarget {
    std::shared_ptr<IModbusClient> client;
    std::string label;
};

bool writeU16(IModbusClient& client, uint16_t addr, uint16_t value, const std::string& label, uint8_t device_id = DA200A::DEFAULT_DEVICE_ID);
bool writeSpeed(IModbusClient& client, int rpm, uint8_t device_id = DA200A::DEFAULT_DEVICE_ID);

