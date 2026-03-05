#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm> 
#include <cctype>    
#include <iomanip>

#include "serial/serial.h" 
#include "RPI2C.h"

using namespace RPI2C;

// 辅助函数：提取串口名称末尾的数字
int extractPortNumber(const std::string& portName) {
    int num = 0;
    int multiplier = 1;
    for (int i = portName.length() - 1; i >= 0; --i) {
        if (std::isdigit(portName[i])) {
            num += (portName[i] - '0') * multiplier;
            multiplier *= 10;
        } else if (multiplier > 1) {
            break; 
        }
    }
    return num;
}

// 辅助函数：大小写不敏感查找
bool containsIgnoreCase(const std::string& str, const std::string& sub) {
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != str.end());
}

// 辅助函数：打印 Hex 字节流
void printHex(const std::vector<uint8_t>& data) {
    for (uint8_t b : data) {
        std::cout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)b << " ";
    }
    std::cout << std::dec << "\n";
}

// ==========================================
// 通信执行引擎：负责发送指令并等待有效回复
// ==========================================
bool executeCommand(serial::Serial& my_serial, Protocol& parser, const std::vector<uint8_t>& tx_data, Packet& rx_packet, int retries = 5) {
    parser.reset();
    my_serial.write(tx_data);

    for (int i = 0; i < retries; ++i) {
        std::vector<uint8_t> rx_buffer;
        size_t bytes_read = my_serial.read(rx_buffer, 1024); 

        if (bytes_read > 0) {
            for (uint8_t byte : rx_buffer) {
                if (parser.parseByte(byte, rx_packet)) {
                    return true; // 成功解析到一帧数据
                }
            }
        } else {
            parser.reset(); // 读超时(100ms)，重置状态机
        }
    }
    return false; // 多次尝试后依然失败
}


int main() {
    std::cout << "--- RPI2C Full API Hardware Test ---\n\n";

    std::vector<serial::PortInfo> ports = serial::list_ports();
    if (ports.empty()) return -1;

    std::sort(ports.begin(), ports.end(), [](const serial::PortInfo& a, const serial::PortInfo& b) {
        return extractPortNumber(a.port) > extractPortNumber(b.port);
    });

    serial::Serial my_serial;
    Protocol protocol_parser;
    Packet rx_packet;
    std::string target_port = "";
    
    // --- 1. 自动扫描与连接 ---
    std::cout << "[Step 1] Scanning for RP2040 Adapter...\n";
    std::vector<uint8_t> tx_data = Protocol::packGetSignature();

    for (const auto& port_info : ports) {
        if (containsIgnoreCase(port_info.description, "bluetooth") || 
            containsIgnoreCase(port_info.description, "蓝牙") || 
            containsIgnoreCase(port_info.hardware_id, "bthenum")) {
            continue;
        }

        try {
            my_serial.setPort(port_info.port);
            my_serial.setBaudrate(115200);
            serial::Timeout to = serial::Timeout::simpleTimeout(100);
            my_serial.setTimeout(to);
            my_serial.open();

            if (my_serial.isOpen()) {
                my_serial.setDTR(true); 
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
            }
        } catch (...) { continue; }

        if (!my_serial.isOpen()) continue;

        if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet, 3)) {
            if (rx_packet.cmd == (CMD_GET_SIG | 0x80)) {
                target_port = port_info.port;
                std::cout << "  -> Connected to: " << target_port << "\n\n";
                break; 
            }
        }
        my_serial.close();
    }

    if (target_port.empty()) {
        std::cerr << "Device not found!\n";
        return -1;
    }

    // ==========================================
    // --- 2. 开始测试完整的 I2C API ---
    // 假设 RP2040 内部已经运行了地址为 0x60 的 Dummy Slave
    // ==========================================
    uint8_t slave_addr = 0x60;

    // 测试 A：软复位
    std::cout << "[Test A] Resetting I2C Peripheral...\n";
    tx_data = Protocol::packReset();
    if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet)) {
        std::cout << "  -> Success. Status: 0x" << std::hex << (int)rx_packet.payload[0] << std::dec << "\n";
    }

    // 测试 B：设置波特率到 400kHz
    std::cout << "[Test B] Setting Baudrate to 400kHz...\n";
    tx_data = Protocol::packSetBaudrate(400000);
    if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet)) {
        std::cout << "  -> Success. Status: 0x" << std::hex << (int)rx_packet.payload[0] << std::dec << "\n";
    }

    // 测试 C：I2C 写操作 (向虚拟从机的 0x10 内存地址，写入 3 个字节：0xDE, 0xAD, 0xBE)
    std::cout << "[Test C] I2C Write (Addr 0x60, writing to mem 0x10)...\n";
    std::vector<uint8_t> write_payload = {0x10, 0xDE, 0xAD, 0xBE}; // 首字节是内存地址，后面是数据
    tx_data = Protocol::packWrite(slave_addr, write_payload);
    if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet)) {
        if (rx_packet.payload[0] == 0x00) {
            std::cout << "  -> Write Success! (ACK received)\n";
        } else {
            std::cout << "  -> Write Failed! (NACK or Timeout)\n";
        }
    }

    // 测试 D：I2C 复合写读操作 (读取刚才写入的 0x10 地址的 3 个字节)
    std::cout << "[Test D] I2C WriteRead (Addr 0x60, read 3 bytes from mem 0x10)...\n";
    std::vector<uint8_t> target_reg = {0x10}; 
    tx_data = Protocol::packWriteRead(slave_addr, 3, target_reg);
    if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet)) {
        if (rx_packet.len == 3) {
            std::cout << "  -> Read Success! Data: ";
            printHex(rx_packet.payload); // 预期输出: DE AD BE
        } else {
            std::cout << "  -> Read Failed! Length: " << rx_packet.len << "\n";
        }
    }

    // 测试 E：I2C 纯读操作 (连续读取 2 个字节)
    std::cout << "[Test E] I2C Pure Read (Addr 0x60, read 2 bytes)...\n";
    tx_data = Protocol::packRead(slave_addr, 2);
    if (executeCommand(my_serial, protocol_parser, tx_data, rx_packet)) {
        if (rx_packet.len == 2) {
            std::cout << "  -> Read Success! Data: ";
            printHex(rx_packet.payload); 
        } else {
            std::cout << "  -> Read Failed! Length: " << rx_packet.len << "\n";
        }
    }

    std::cout << "\n--- All Hardware Tests Completed! ---\n";

    my_serial.close();
    return 0;
}