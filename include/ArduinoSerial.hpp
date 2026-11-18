#pragma once

#include <string>
#include <functional>

class ArduinoSerial {
public:
    explicit ArduinoSerial(std::string port, int baudrate = 9600);
    ~ArduinoSerial();

    bool open();
    void close();
    bool isOpen() const;

    // Non-blocking poll for incoming lines from Arduino
    void poll(const std::function<void(const std::string& line)>& on_line);

    const std::string& getPort() const { return port_; }

private:
    std::string port_;
    int baudrate_;
    int fd_;
    std::string rx_buffer_;
};

