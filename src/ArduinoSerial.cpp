#include "ArduinoSerial.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

namespace {
speed_t baudrateToSpeed(int baudrate) {
    switch (baudrate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;
    }
}
} // anonymous namespace

ArduinoSerial::ArduinoSerial(std::string port, int baudrate)
    : port_(std::move(port)),
      baudrate_(baudrate),
      fd_(-1) {}

ArduinoSerial::~ArduinoSerial() {
    close();
}

bool ArduinoSerial::open() {
    if (fd_ >= 0) {
        return true;
    }

    fd_ = ::open(port_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "[ERROR] Failed to open Arduino port " << port_ << ": " << strerror(errno) << std::endl;
        return false;
    }

    struct termios tty;
    std::memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "[ERROR] tcgetattr failed for Arduino port " << port_ << ": " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    speed_t speed = baudrateToSpeed(baudrate_);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    // 8 Data Bits, No Parity, 1 Stop Bit (8N1)
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;

    // Enable receiver, local line
    tty.c_cflag |= (CLOCAL | CREAD);

    // Raw mode: disable flow control, translations, echoing, canonical mode
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tty.c_oflag &= ~(OPOST | ONLCR | OCRNL);
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG | IEXTEN);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "[ERROR] tcsetattr failed for Arduino port " << port_ << ": " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    tcflush(fd_, TCIOFLUSH);
    return true;
}

void ArduinoSerial::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool ArduinoSerial::isOpen() const {
    return fd_ >= 0;
}

void ArduinoSerial::poll(const std::function<void(const std::string& line)>& on_line) {
    if (fd_ < 0) {
        return;
    }

    char buf[256];
    ssize_t n = ::read(fd_, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        rx_buffer_.append(buf, static_cast<size_t>(n));

        size_t newline_pos;
        while ((newline_pos = rx_buffer_.find('\n')) != std::string::npos) {
            std::string line = rx_buffer_.substr(0, newline_pos);
            rx_buffer_.erase(0, newline_pos + 1);

            // Trim carriage return or trailing spaces
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }

            if (!line.empty() && on_line) {
                on_line(line);
            }
        }
    }
}
