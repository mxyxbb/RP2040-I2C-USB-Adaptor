#pragma once

#include <vector>
#include <cstdint>

namespace RPI2C {

    // ==========================================
    // 1. 命令字与常量定义
    // ==========================================
    enum Command : uint8_t {
        CMD_GET_SIG   = 0x01,
        CMD_SET_BAUD  = 0x02,
        CMD_WRITE     = 0x03,
        CMD_READ      = 0x04,
        CMD_RESET     = 0x05,
        CMD_WRITEREAD = 0x06
    };

    constexpr uint8_t HEADER_BYTE = 0xAA;
    constexpr uint8_t TAIL_BYTE   = 0x55;

    // ==========================================
    // 2. 数据包结构体
    // ==========================================
    struct Packet {
        uint8_t cmd;
        uint16_t len;
        std::vector<uint8_t> payload;
    };

    // ==========================================
    // 3. 协议封包与解包类
    // ==========================================
    class Protocol {
    private:
        // 接收状态机内部变量
        int state = 0;
        uint8_t rx_cmd = 0;
        uint8_t len_lsb = 0;
        uint16_t rx_len = 0;
        uint8_t rx_calc_checksum = 0;
        std::vector<uint8_t> rx_payload;

        // 计算校验和
        static uint8_t calculateChecksum(uint8_t cmd, uint16_t len, const std::vector<uint8_t>& payload) {
            uint8_t checksum = cmd + (len & 0xFF) + ((len >> 8) & 0xFF);
            for (uint8_t byte : payload) {
                checksum += byte;
            }
            return checksum;
        }

        // 通用封包核心逻辑
        static std::vector<uint8_t> packFrame(uint8_t cmd, const std::vector<uint8_t>& payload) {
            std::vector<uint8_t> frame;
            uint16_t len = static_cast<uint16_t>(payload.size());
            
            frame.reserve(len + 6); // 预分配内存，提升性能
            frame.push_back(HEADER_BYTE);
            frame.push_back(cmd);
            frame.push_back(len & 0xFF);         // Len LSB
            frame.push_back((len >> 8) & 0xFF);  // Len MSB
            
            frame.insert(frame.end(), payload.begin(), payload.end());
            
            frame.push_back(calculateChecksum(cmd, len, payload));
            frame.push_back(TAIL_BYTE);
            
            return frame;
        }

    public:
        Protocol() { 
            rx_payload.reserve(1024); 
        }

        // ---------------------------------------------------
        //  封包接口 (Pack) - 生成发往硬件的字节流
        // ---------------------------------------------------

        // 1. 获取特征码 (0x01)
        static std::vector<uint8_t> packGetSignature() {
            return packFrame(CMD_GET_SIG, {});
        }

        // 2. 配置波特率 (0x02) - 4字节小端模式
        static std::vector<uint8_t> packSetBaudrate(uint32_t baudrate) {
            std::vector<uint8_t> payload(4);
            payload[0] = baudrate & 0xFF;
            payload[1] = (baudrate >> 8) & 0xFF;
            payload[2] = (baudrate >> 16) & 0xFF;
            payload[3] = (baudrate >> 24) & 0xFF;
            return packFrame(CMD_SET_BAUD, payload);
        }

        // 3. I2C 写操作 (0x03) - [目标地址] + [数据流]
        static std::vector<uint8_t> packWrite(uint8_t addr, const std::vector<uint8_t>& write_data) {
            std::vector<uint8_t> payload;
            payload.reserve(write_data.size() + 1);
            payload.push_back(addr);
            payload.insert(payload.end(), write_data.begin(), write_data.end());
            return packFrame(CMD_WRITE, payload);
        }

        // 4. I2C 读操作 (0x04) - [目标地址] + [读取长度 (2字节小端)]
        static std::vector<uint8_t> packRead(uint8_t addr, uint16_t read_len) {
            std::vector<uint8_t> payload;
            payload.push_back(addr);
            payload.push_back(read_len & 0xFF);        // Read Len LSB
            payload.push_back((read_len >> 8) & 0xFF); // Read Len MSB
            return packFrame(CMD_READ, payload);
        }

        // 5. 软复位外设 (0x05)
        static std::vector<uint8_t> packReset() {
            return packFrame(CMD_RESET, {});
        }

        // 6. I2C 复合写读操作 (0x06) - [目标地址] + [读取长度 (2字节小端)] + [待写数据]
        static std::vector<uint8_t> packWriteRead(uint8_t addr, uint16_t read_len, const std::vector<uint8_t>& write_data) {
            std::vector<uint8_t> payload;
            payload.reserve(write_data.size() + 3);
            payload.push_back(addr);
            payload.push_back(read_len & 0xFF);        // Read Len LSB
            payload.push_back((read_len >> 8) & 0xFF); // Read Len MSB
            payload.insert(payload.end(), write_data.begin(), write_data.end());
            return packFrame(CMD_WRITEREAD, payload);
        }

        // ---------------------------------------------------
        //  解包接口 (Unpack) - 解析来自硬件的字节流
        // ---------------------------------------------------
        
        // 喂入单个字节。如果恰好解析出一个完整的包，返回 true 并将数据存入 out_packet
        bool parseByte(uint8_t byte, Packet& out_packet) {
            switch (state) {
                case 0: // 等待包头
                    if (byte == HEADER_BYTE) state = 1;
                    break;
                case 1: // 接收命令
                    rx_cmd = byte;
                    rx_calc_checksum = byte;
                    state = 2;
                    break;
                case 2: // 接收长度低字节
                    len_lsb = byte;
                    rx_calc_checksum += byte;
                    state = 3;
                    break;
                case 3: // 接收长度高字节
                    rx_len = len_lsb | (byte << 8);
                    rx_calc_checksum += byte;
                    rx_payload.clear();
                    
                    if (rx_len > 1024) { // 防线1：长度越界保护
                        state = 0;
                    } else {
                        state = (rx_len > 0) ? 4 : 5;
                    }
                    break;
                case 4: // 接收数据
                    rx_payload.push_back(byte);
                    rx_calc_checksum += byte;
                    if (rx_payload.size() >= rx_len) state = 5;
                    break;
                case 5: // 校验和
                    if (byte == rx_calc_checksum) {
                        state = 6;
                    } else {
                        state = 0; // 校验失败，直接丢弃
                    }
                    break;
                case 6: // 验证帧尾
                    state = 0; // 无论帧尾对错，状态机归零
                    if (byte == TAIL_BYTE) {
                        out_packet.cmd = rx_cmd;
                        out_packet.len = rx_len;
                        out_packet.payload = rx_payload;
                        return true; 
                    }
                    break;
            }
            return false;
        }

        // 强行重置状态机 (用于串口接收超时检测)
        void reset() {
            state = 0;
            rx_payload.clear();
        }
    };
}