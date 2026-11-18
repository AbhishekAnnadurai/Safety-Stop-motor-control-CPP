#pragma once

#include "IModbusClient.hpp"
#include <iostream>
#include <string>
#include <vector>

class MockModbusClient : public IModbusClient {
public:
    explicit MockModbusClient(std::string port)
        : port_(std::move(port)), connected_(false) {
        std::cout << "[MOCK] Initialized mock Modbus client for " << port_ << std::endl;
    }

    ~MockModbusClient() override {
        close();
    }

    bool connect() override {
        connected_ = true;
        std::cout << "[MOCK] Connected to " << port_ << std::endl;
        return true;
    }

    void close() override {
        if (connected_) {
            connected_ = false;
            std::cout << "[MOCK] Closed connection for " << port_ << std::endl;
        }
    }

    bool isConnected() const override {
        return connected_;
    }

    bool writeRegister(uint16_t address, uint16_t value, uint8_t device_id = 1) override {
        (void)device_id;
        std::cout << "[MOCK] " << port_ << ": Write register " << address << " = " << value << std::endl;
        return true;
    }

    bool writeRegisters(uint16_t address, const std::vector<uint16_t>& values, uint8_t device_id = 1) override {
        (void)device_id;
        std::cout << "[MOCK] " << port_ << ": Write registers " << address << " = [";
        for (size_t i = 0; i < values.size(); ++i) {
            std::cout << values[i] << (i + 1 < values.size() ? ", " : "");
        }
        std::cout << "]" << std::endl;
        return true;
    }

    const std::string& getPort() const override {
        return port_;
    }

private:
    std::string port_;
    bool connected_;
};

