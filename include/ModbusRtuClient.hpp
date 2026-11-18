#pragma once

#include "IModbusClient.hpp"
#include <string>
#include <vector>
#include <cstdint>

class ModbusRtuClient : public IModbusClient {
public:
    enum class Parity {
        None,
        Even,
        Odd
    };

    ModbusRtuClient(std::string port,
                    int baudrate = 19200,
                    Parity parity = Parity::None,
                    int stopbits = 1,
                    int timeout_ms = 300);

    ~ModbusRtuClient() override;

    bool connect() override;
    void close() override;
    bool isConnected() const override;

    bool writeRegister(uint16_t address, uint16_t value, uint8_t device_id = 1) override;
    bool writeRegisters(uint16_t address, const std::vector<uint16_t>& values, uint8_t device_id = 1) override;

    const std::string& getPort() const override { return port_; }

    static uint16_t calculateCrc16(const uint8_t* data, size_t length);
    static std::vector<uint8_t> buildWriteSingleRegisterFrame(uint8_t device_id, uint16_t address, uint16_t value);
    static std::vector<uint8_t> buildWriteMultipleRegistersFrame(uint8_t device_id, uint16_t address, const std::vector<uint16_t>& values);

private:
    bool sendAndReceive(const std::vector<uint8_t>& request, size_t expected_response_len, std::vector<uint8_t>& response);

    std::string port_;
    int baudrate_;
    Parity parity_;
    int stopbits_;
    int timeout_ms_;
    int fd_;
    bool connected_;
};

