#include "MotorController.hpp"

#include <iostream>

bool writeU16(IModbusClient& client, uint16_t addr, uint16_t value, const std::string& label, uint8_t device_id) {
    bool ok = client.writeRegister(addr, value & 0xFFFF, device_id);
    if (!ok) {
        std::cerr << "[ERROR] Failed to write register " << addr << " (" << label << ") on " << client.getPort() << std::endl;
    }
    return ok;
}

bool writeSpeed(IModbusClient& client, int rpm, uint8_t device_id) {
    int32_t reg_value = rpm * DA200A::VFD_RPM_SCALE;
    uint32_t u = static_cast<uint32_t>(reg_value);
    uint16_t lo = static_cast<uint16_t>(u & 0xFFFF);
    uint16_t hi = static_cast<uint16_t>((u >> 16) & 0xFFFF);

    bool ok = client.writeRegisters(DA200A::REG_SPEED_CMD, {lo, hi}, device_id);
    if (!ok) {
        std::cerr << "[ERROR] Failed to write speed command: " << rpm << " RPM on " << client.getPort() << std::endl;
    }
    return ok;
}

