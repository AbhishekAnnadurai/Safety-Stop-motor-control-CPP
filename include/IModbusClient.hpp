#pragma once

#include <string>
#include <vector>
#include <cstdint>

class IModbusClient {
public:
    virtual ~IModbusClient() = default;

    virtual bool connect() = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;

    virtual bool writeRegister(uint16_t address, uint16_t value, uint8_t device_id = 1) = 0;
    virtual bool writeRegisters(uint16_t address, const std::vector<uint16_t>& values, uint8_t device_id = 1) = 0;

    virtual const std::string& getPort() const = 0;
};

