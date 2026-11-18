#include "ModbusRtuClient.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>

namespace {
speed_t baudrateToSpeed(int baudrate) {
    switch (baudrate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B19200;
    }
}
} // anonymous namespace

ModbusRtuClient::ModbusRtuClient(std::string port,
                                 int baudrate,
                                 Parity parity,
                                 int stopbits,
                                 int timeout_ms)
    : port_(std::move(port)),
      baudrate_(baudrate),
      parity_(parity),
      stopbits_(stopbits),
      timeout_ms_(timeout_ms),
      fd_(-1),
      connected_(false) {}

ModbusRtuClient::~ModbusRtuClient() {
    close();
}

uint16_t ModbusRtuClient::calculateCrc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

std::vector<uint8_t> ModbusRtuClient::buildWriteSingleRegisterFrame(uint8_t device_id, uint16_t address, uint16_t value) {
    std::vector<uint8_t> frame(8);
    frame[0] = device_id;
    frame[1] = 0x06; // Function: Write Single Register
    frame[2] = static_cast<uint8_t>((address >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(address & 0xFF);
    frame[4] = static_cast<uint8_t>((value >> 8) & 0xFF);
    frame[5] = static_cast<uint8_t>(value & 0xFF);

    uint16_t crc = calculateCrc16(frame.data(), 6);
    frame[6] = static_cast<uint8_t>(crc & 0xFF);        // CRC low byte
    frame[7] = static_cast<uint8_t>((crc >> 8) & 0xFF); // CRC high byte
    return frame;
}

std::vector<uint8_t> ModbusRtuClient::buildWriteMultipleRegistersFrame(uint8_t device_id, uint16_t address, const std::vector<uint16_t>& values) {
    uint16_t count = static_cast<uint16_t>(values.size());
    uint8_t byte_count = static_cast<uint8_t>(count * 2);
    size_t payload_len = 7 + byte_count;
    std::vector<uint8_t> frame(payload_len + 2);

    frame[0] = device_id;
    frame[1] = 0x10; // Function: Write Multiple Registers
    frame[2] = static_cast<uint8_t>((address >> 8) & 0xFF);
    frame[3] = static_cast<uint8_t>(address & 0xFF);
    frame[4] = static_cast<uint8_t>((count >> 8) & 0xFF);
    frame[5] = static_cast<uint8_t>(count & 0xFF);
    frame[6] = byte_count;

    for (size_t i = 0; i < values.size(); ++i) {
        frame[7 + i * 2] = static_cast<uint8_t>((values[i] >> 8) & 0xFF);
        frame[7 + i * 2 + 1] = static_cast<uint8_t>(values[i] & 0xFF);
    }

    uint16_t crc = calculateCrc16(frame.data(), payload_len);
    frame[payload_len] = static_cast<uint8_t>(crc & 0xFF);
    frame[payload_len + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return frame;
}

bool ModbusRtuClient::connect() {
    if (connected_) {
        return true;
    }

    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "[ERROR] Cannot open serial port " << port_ << ": " << strerror(errno) << std::endl;
        return false;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "[ERROR] tcgetattr failed for " << port_ << ": " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    speed_t speed = baudrateToSpeed(baudrate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8 Data Bits
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;

    // Parity
    switch (parity_) {
        case Parity::None:
            tty.c_cflag &= ~(PARENB | PARODD);
            tty.c_iflag &= ~INPCK;
            break;
        case Parity::Even:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            tty.c_iflag |= INPCK;
            break;
        case Parity::Odd:
            tty.c_cflag |= (PARENB | PARODD);
            tty.c_iflag |= INPCK;
            break;
    }

    // Stop Bits
    if (stopbits_ == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    // Disable hardware flow control
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif

    // Enable receiver, local line
    tty.c_cflag |= (CLOCAL | CREAD);

    // Raw input: disable software flow control, carriage-return translation
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);

    // Raw output
    tty.c_oflag &= ~(OPOST | ONLCR | OCRNL);

    // Raw mode: non-canonical, no echo, no signal chars
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN);

    // Non-blocking read configurations
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "[ERROR] tcsetattr failed for " << port_ << ": " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Flush any pending data in queues
    tcflush(fd_, TCIOFLUSH);

    connected_ = true;
    return true;
}

void ModbusRtuClient::close() {
    if (connected_) {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        connected_ = false;
    }
}

bool ModbusRtuClient::isConnected() const {
    return connected_;
}

bool ModbusRtuClient::sendAndReceive(const std::vector<uint8_t>& request, size_t expected_response_len, std::vector<uint8_t>& response) {
    if (!connected_ || fd_ < 0) {
        return false;
    }

    // Flush input buffer before transmission
    tcflush(fd_, TCIFLUSH);

    // Send request frame
    ssize_t bytes_written = ::write(fd_, request.data(), request.size());
    if (bytes_written != static_cast<ssize_t>(request.size())) {
        std::cerr << "[ERROR] Failed to write complete frame to " << port_ << std::endl;
        return false;
    }

    // Await response using poll() with timeout
    response.clear();
    response.reserve(expected_response_len);

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int remaining_ms = timeout_ms_;
    auto start_time = std::chrono::steady_clock::now();

    while (response.size() < expected_response_len && remaining_ms > 0) {
        int ret = poll(&pfd, 1, remaining_ms);
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[ERROR] poll error on " << port_ << ": " << strerror(errno) << std::endl;
            return false;
        }
        if (ret == 0) {
            // Timeout reached
            break;
        }

        if (pfd.revents & POLLIN) {
            uint8_t buf[256];
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n > 0) {
                response.insert(response.end(), buf, buf + n);
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
        remaining_ms = timeout_ms_ - static_cast<int>(elapsed);
    }

    // Check response length
    if (response.size() < expected_response_len) {
        // Check if an exception response was returned (5 bytes: slave, func | 0x80, code, crc_lo, crc_hi)
        if (response.size() >= 5 && (response[1] & 0x80)) {
            std::cerr << "[ERROR] Modbus Exception response from " << port_
                      << " (code: 0x" << std::hex << static_cast<int>(response[2]) << std::dec << ")" << std::endl;
        }
        return false;
    }

    // Validate CRC of response
    uint16_t expected_crc = calculateCrc16(response.data(), response.size() - 2);
    uint16_t actual_crc = static_cast<uint16_t>(response[response.size() - 2]) |
                          (static_cast<uint16_t>(response[response.size() - 1]) << 8);

    if (expected_crc != actual_crc) {
        std::cerr << "[ERROR] CRC mismatch in response from " << port_ << std::endl;
        return false;
    }

    return true;
}

bool ModbusRtuClient::writeRegister(uint16_t address, uint16_t value, uint8_t device_id) {
    auto frame = buildWriteSingleRegisterFrame(device_id, address, value);
    std::vector<uint8_t> response;
    // Expected response is 8 bytes (echo of slave, func 0x06, address, value, crc)
    return sendAndReceive(frame, 8, response);
}

bool ModbusRtuClient::writeRegisters(uint16_t address, const std::vector<uint16_t>& values, uint8_t device_id) {
    auto frame = buildWriteMultipleRegistersFrame(device_id, address, values);
    std::vector<uint8_t> response;
    // Expected response is 8 bytes (slave, func 0x10, address, count, crc)
    return sendAndReceive(frame, 8, response);
}
