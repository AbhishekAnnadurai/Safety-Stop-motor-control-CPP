#include "ModbusRtuClient.hpp"
#include "MockModbusClient.hpp"
#include "MotorController.hpp"

#include <iostream>
#include <cassert>
#include <vector>

void testCrc16KnownVectors() {
    // Test 1: Write Single Register (addr 1820, value 1, slave 1)
    std::vector<uint8_t> data1 = {0x01, 0x06, 0x07, 0x1C, 0x00, 0x01};
    uint16_t crc1 = ModbusRtuClient::calculateCrc16(data1.data(), data1.size());
    assert((crc1 & 0xFF) == 0x88);
    assert(((crc1 >> 8) & 0xFF) == 0xB8);

    // Test 2: Write Multiple Registers (addr 1826, count 2, values [5000, 0], slave 1)
    std::vector<uint8_t> data2 = {0x01, 0x10, 0x07, 0x22, 0x00, 0x02, 0x04, 0x13, 0x88, 0x00, 0x00};
    uint16_t crc2 = ModbusRtuClient::calculateCrc16(data2.data(), data2.size());
    assert((crc2 & 0xFF) == 0xD2);
    assert(((crc2 >> 8) & 0xFF) == 0xF0);

    std::cout << "[PASS] testCrc16KnownVectors" << std::endl;
}

void testWriteSingleRegisterFrameBuilder() {
    auto frame = ModbusRtuClient::buildWriteSingleRegisterFrame(1, 1820, 1);
    std::vector<uint8_t> expected = {0x01, 0x06, 0x07, 0x1C, 0x00, 0x01, 0x88, 0xB8};
    assert(frame.size() == expected.size());
    assert(frame == expected);

    std::cout << "[PASS] testWriteSingleRegisterFrameBuilder" << std::endl;
}

void testWriteMultipleRegistersFrameBuilder() {
    // 500 RPM * 10 scale = 5000 (0x1388)
    auto frame = ModbusRtuClient::buildWriteMultipleRegistersFrame(1, 1826, {5000, 0});
    std::vector<uint8_t> expected = {0x01, 0x10, 0x07, 0x22, 0x00, 0x02, 0x04, 0x13, 0x88, 0x00, 0x00, 0xD2, 0xF0};
    assert(frame.size() == expected.size());
    assert(frame == expected);

    std::cout << "[PASS] testWriteMultipleRegistersFrameBuilder" << std::endl;
}

void testMockModbusClient() {
    MockModbusClient mock("/dev/ttyMOCK");
    assert(!mock.isConnected());
    assert(mock.connect());
    assert(mock.isConnected());

    assert(mock.writeRegister(1820, 1, 1));
    assert(mock.writeRegisters(1826, {10000, 0}, 1));

    mock.close();
    assert(!mock.isConnected());

    std::cout << "[PASS] testMockModbusClient" << std::endl;
}

int main() {
    std::cout << "Running C++ Modbus & Motor Controller Unit Tests...\n";
    testCrc16KnownVectors();
    testWriteSingleRegisterFrameBuilder();
    testWriteMultipleRegistersFrameBuilder();
    testMockModbusClient();
    std::cout << "\nALL TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}

