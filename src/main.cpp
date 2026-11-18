#include "IModbusClient.hpp"
#include "ModbusRtuClient.hpp"
#include "MockModbusClient.hpp"
#include "ArduinoSerial.hpp"
#include "MotorController.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <unordered_set>

namespace {
std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    (void)signum;
    g_running = false;
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --port <path>             Shared serial port for both motor drivers (alias for --port-left/--port-right)\n"
              << "  --port-left <path>        Serial port to the left motor driver (default: /dev/ttyUSB_LEFT)\n"
              << "  --port-right <path>       Serial port to the right motor driver (default: /dev/ttyUSB_RIGHT)\n"
              << "  --rpm <int>               Target RPM to spin the motors (default: 1000)\n"
              << "  --baudrate <int>          Baudrate for Modbus communication (default: 19200)\n"
              << "  --parity <N|E|O>          Serial parity configured on the VFD (default: N)\n"
              << "  --stopbits <1|2>          Serial stop bits configured on the VFD (default: 1)\n"
              << "  --device-id <int>         Modbus slave ID of the motor drive (default: 1)\n"
              << "  --arduino-port <path>     Serial port to the Arduino (e.g. /dev/ttyACM0)\n"
              << "  --arduino-baudrate <int>  Baudrate for Arduino (default: 9600)\n"
              << "  --mock                    Run in mock mode without physical motor drives\n"
              << "  -h, --help                Show this help message and exit\n";
}

struct ProgramArgs {
    std::string port_left = "/dev/ttyUSB_LEFT";
    std::string port_right = "/dev/ttyUSB_RIGHT";
    int rpm = 1000;
    int baudrate = 19200;
    ModbusRtuClient::Parity parity = ModbusRtuClient::Parity::None;
    int stopbits = 1;
    uint8_t device_id = DA200A::DEFAULT_DEVICE_ID;
    std::string arduino_port = "";
    int arduino_baudrate = 9600;
    bool mock = false;
};

bool parseArgs(int argc, char* argv[], ProgramArgs& args) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else if (arg == "--mock") {
            args.mock = true;
        } else if (arg == "--port" && i + 1 < argc) {
            args.port_left = argv[++i];
            args.port_right = args.port_left;
        } else if (arg == "--port-left" && i + 1 < argc) {
            args.port_left = argv[++i];
        } else if (arg == "--port-right" && i + 1 < argc) {
            args.port_right = argv[++i];
        } else if (arg == "--rpm" && i + 1 < argc) {
            args.rpm = std::stoi(argv[++i]);
        } else if (arg == "--baudrate" && i + 1 < argc) {
            args.baudrate = std::stoi(argv[++i]);
        } else if (arg == "--parity" && i + 1 < argc) {
            std::string p = argv[++i];
            if (p == "N" || p == "n") args.parity = ModbusRtuClient::Parity::None;
            else if (p == "E" || p == "e") args.parity = ModbusRtuClient::Parity::Even;
            else if (p == "O" || p == "o") args.parity = ModbusRtuClient::Parity::Odd;
            else {
                std::cerr << "Invalid parity: " << p << " (expected N, E, or O)\n";
                return false;
            }
        } else if (arg == "--stopbits" && i + 1 < argc) {
            args.stopbits = std::stoi(argv[++i]);
            if (args.stopbits != 1 && args.stopbits != 2) {
                std::cerr << "Invalid stopbits: " << args.stopbits << " (expected 1 or 2)\n";
                return false;
            }
        } else if (arg == "--device-id" && i + 1 < argc) {
            args.device_id = static_cast<uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "--arduino-port" && i + 1 < argc) {
            args.arduino_port = argv[++i];
        } else if (arg == "--arduino-baudrate" && i + 1 < argc) {
            args.arduino_baudrate = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}
} // anonymous namespace

int main(int argc, char* argv[]) {
    ProgramArgs args;
    if (!parseArgs(argc, argv, args)) {
        return 0;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize Arduino connection if requested
    std::unique_ptr<ArduinoSerial> arduino;
    if (!args.arduino_port.empty()) {
        arduino = std::make_unique<ArduinoSerial>(args.arduino_port, args.arduino_baudrate);
        if (arduino->open()) {
            std::cout << "[SUCCESS] Connected to Arduino at " << args.arduino_port << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to connect to Arduino at " << args.arduino_port << std::endl;
            return 1;
        }
    }

    // Resolve motor targets preserving left-before-right order and deduplicating
    std::vector<std::pair<std::string, std::string>> targets_info;
    std::unordered_set<std::string> seen_ports;

    for (const auto& item : std::vector<std::pair<std::string, std::string>>{
             {args.port_left, "Left"}, {args.port_right, "Right"}}) {
        if (!item.first.empty() && seen_ports.find(item.first) == seen_ports.end()) {
            seen_ports.insert(item.first);
            targets_info.push_back(item);
        }
    }

    std::vector<MotorClientTarget> targets;
    for (const auto& [port, label] : targets_info) {
        std::shared_ptr<IModbusClient> client;
        if (args.mock) {
            client = std::make_shared<MockModbusClient>(port);
        } else {
            client = std::make_shared<ModbusRtuClient>(
                port, args.baudrate, args.parity, args.stopbits, 300);
        }

        if (client->connect()) {
            std::cout << "[SUCCESS] Connected to " << label << " Motor at " << port << std::endl;
            targets.push_back({client, label});
        } else {
            std::cerr << "[ERROR] Failed to connect to " << label << " Motor at " << port << std::endl;
        }
    }

    if (targets.empty()) {
        std::cerr << "[ERROR] Could not connect to any motors. Exiting." << std::endl;
        return 1;
    }

    std::cout << "Initializing Modbus communication settings..." << std::endl;
    bool initialization_failed = false;

    for (const auto& target : targets) {
        auto& client = *target.client;
        const auto& label = target.label;

        // P4.10=1: Bus command source
        if (!writeU16(client, DA200A::REG_BUS_SOURCE, 1, "Bus Source (" + label + ")", args.device_id)) {
            initialization_failed = true;
        }
        // P0.54=10, P0.55=10: Set accel/decel time to 1.0s
        if (!writeU16(client, DA200A::REG_ACCEL_TIME, 10, "Accel Time (" + label + ")", args.device_id)) {
            initialization_failed = true;
        }
        if (!writeU16(client, DA200A::REG_DECEL_TIME, 10, "Decel Time (" + label + ")", args.device_id)) {
            initialization_failed = true;
        }
        // Start at 0 RPM
        if (!writeSpeed(client, 0, args.device_id)) {
            initialization_failed = true;
        }
        // P4.11=1: Enable bus control
        if (!writeU16(client, DA200A::REG_BUS_ENABLE, 1, "Bus Enable (" + label + ")", args.device_id)) {
            initialization_failed = true;
        }
    }

    if (initialization_failed) {
        std::cerr << "[ERROR] Motor initialization failed. No run command will be sent." << std::endl;
        for (auto& target : targets) {
            target.client->close();
        }
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "--------------------------------------------------\n"
              << "🏎️ Motors are starting up at " << args.rpm << " RPM.\n"
              << "🛑 Press Ctrl+C directly in this terminal to STOP safely.\n"
              << "--------------------------------------------------" << std::endl;

    int current_rpm = args.rpm;
    for (const auto& target : targets) {
        writeSpeed(*target.client, current_rpm, args.device_id);
    }

    auto last_heartbeat = std::chrono::steady_clock::now();
    auto clear_path_start_time = std::chrono::steady_clock::time_point{};
    bool is_braking = false;
    bool has_clear_path_timer = false;

    while (g_running) {
        bool state_changed = false;

        if (arduino && arduino->isOpen()) {
            arduino->poll([&](const std::string& line) {
                if (line.find("Object Detected") != std::string::npos) {
                    has_clear_path_timer = false;
                    if (!is_braking) {
                        std::cout << "[WARNING] Object detected! Stopping motors." << std::endl;
                        current_rpm = 0;
                        is_braking = true;
                        state_changed = true;
                    }
                } else if (line.find("No Problem") != std::string::npos) {
                    if (is_braking && !has_clear_path_timer) {
                        clear_path_start_time = std::chrono::steady_clock::now();
                        has_clear_path_timer = true;
                    }
                }
            });
        }

        // Check if clear path threshold (2.0s) has elapsed
        if (is_braking && has_clear_path_timer) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_sec = std::chrono::duration<double>(now - clear_path_start_time).count();
            if (elapsed_sec >= 2.0) {
                std::cout << "[INFO] Path clear for 2 seconds. Restarting motors to " << args.rpm << " RPM." << std::endl;
                current_rpm = args.rpm;
                is_braking = false;
                has_clear_path_timer = false;
                state_changed = true;
            }
        }

        // Send continuous Modbus heartbeat every 0.2s to prevent VFD timeout
        auto now = std::chrono::steady_clock::now();
        auto heartbeat_elapsed = std::chrono::duration<double>(now - last_heartbeat).count();
        if (state_changed || heartbeat_elapsed >= 0.2) {
            for (const auto& target : targets) {
                writeSpeed(*target.client, current_rpm, args.device_id);
            }
            last_heartbeat = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Safe shutdown sequence
    std::cout << "\n\n[WARNING] Halting command received (Ctrl+C). Stopping motors SAFELY..." << std::endl;
    for (const auto& target : targets) {
        writeSpeed(*target.client, 0, args.device_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Disable bus control
        writeU16(*target.client, DA200A::REG_BUS_ENABLE, 0, "Bus Disable (" + target.label + ")", args.device_id);
        target.client->close();
        std::cout << "[SUCCESS] " << target.label << " Motor stopped successfully." << std::endl;
    }

    if (arduino) {
        arduino->close();
    }

    return 0;
}

