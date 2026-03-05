#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <pico/i2c_slave.h>

static void setup_slave();

#define I2C_PORT i2c1
#define SDA_PIN 6
#define SCL_PIN 7
// For this example, we run both the master and slave from the same board.
// You'll need to wire pin GP4 to GP6 (SDA), and pin GP5 to GP7 (SCL).
static const uint I2C_SLAVE_SDA_PIN = 4;
static const uint I2C_SLAVE_SCL_PIN = 5;
#define DEVICE_SIGNATURE "RP2040_I2C_ADAPTER_V1"

// 增加全局变量记录当前波特率，默认 100kHz
uint32_t current_i2c_baudrate = 100 * 1000;

// 协议定义
#define HEADER_BYTE 0xAA
#define TAIL_BYTE 0x55  // 新增帧尾定义
#define CMD_GET_SIG 0x01
#define CMD_SET_BAUD 0x02
#define CMD_I2C_WRITE 0x03
#define CMD_I2C_READ 0x04
#define CMD_I2C_RESET 0x05
#define CMD_I2C_WRITEREAD 0x06

// 辅助函数：发送响应包 (长度参数升级为 uint16_t)
void send_response(uint8_t cmd, uint8_t *data, uint16_t len) {
    uint8_t len_lsb = len & 0xFF;
    uint8_t len_msb = (len >> 8) & 0xFF;
    uint8_t checksum = cmd + len_lsb + len_msb;
    
    putchar_raw(HEADER_BYTE);
    putchar_raw(cmd);
    putchar_raw(len_lsb); // 发送长度低字节
    putchar_raw(len_msb); // 发送长度高字节
    
    for (int i = 0; i < len; i++) {
        putchar_raw(data[i]);
        checksum += data[i];
    }
    putchar_raw(checksum);
    putchar_raw(TAIL_BYTE); // 发送帧尾 0x55
    stdio_flush(); // 确保数据立刻通过 USB 发送
}

// 处理接收到的完整数据帧 (长度参数升级为 uint16_t)
void process_frame(uint8_t cmd, uint8_t *data, uint16_t len) {
    // 缓冲区扩容，最大支持 512 字节载荷加上可能的控制字节
    uint8_t reply_data[1024]; 
    
    switch (cmd) {
        case CMD_GET_SIG:
            // 响应特征码
            send_response(cmd | 0x80, (uint8_t*)DEVICE_SIGNATURE, strlen(DEVICE_SIGNATURE));
            break;

        case CMD_SET_BAUD:
            if (len == 4) {
                current_i2c_baudrate = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
                i2c_set_baudrate(I2C_PORT, current_i2c_baudrate); // 更新真实波特率
                reply_data[0] = 0x00; // 成功
                send_response(cmd | 0x80, reply_data, 1);
            }
            break;

        case CMD_I2C_RESET:
            i2c_deinit(I2C_PORT);
            i2c_init(I2C_PORT, current_i2c_baudrate);
            gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
            gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
            gpio_pull_up(SDA_PIN);
            gpio_pull_up(SCL_PIN);
            reply_data[0] = 0x00; 
            send_response(cmd | 0x80, reply_data, 1);
            break;

        case CMD_I2C_WRITE:
            // 数据域: [7位设备地址] [写入的数据(最多512字节)...]
            if (len >= 1) {
                uint8_t addr = data[0];
                int written = i2c_write_blocking(I2C_PORT, addr, &data[1], len - 1, false);
                reply_data[0] = (written == (len - 1)) ? 0x00 : 0xFF; // 0x00 成功，0xFF 失败
                send_response(cmd | 0x80, reply_data, 1);
            }
            break;

        case CMD_I2C_READ:
            // 数据域变更为: [7位设备地址] [读取长度低字节 LSB] [读取长度高字节 MSB]
            if (len == 3) {
                uint8_t addr = data[0];
                uint16_t read_len = data[1] | (data[2] << 8); // 解析 16位 长度
                if (read_len <= 512) { // 确保不超过缓冲区限制
                    int read_count = i2c_read_blocking(I2C_PORT, addr, reply_data, read_len, false);
                    if (read_count == read_len) {
                        send_response(cmd | 0x80, reply_data, read_len);
                    } else {
                        // 读取失败，返回长度为 0 的包表示错误
                        send_response(cmd | 0x80, NULL, 0); 
                    }
                }
            }
            break;

        case CMD_I2C_WRITEREAD:
            // 数据域变更为: [7位设备地址] [读取长度 LSB] [读取长度 MSB] [待写入的数据...]
            if (len >= 3) {
                uint8_t addr = data[0];
                uint16_t read_len = data[1] | (data[2] << 8); // 解析 16位 长度
                uint16_t write_len = len - 3; // 剩余的字节就是需要写入的数据
                
                int success = 1;

                if (write_len > 0) {
                    int written = i2c_write_blocking(I2C_PORT, addr, &data[3], write_len, true);
                    if (written != write_len) {
                        success = 0; 
                    }
                }

                if (success && read_len > 0) {
                    if (read_len <= 512) { 
                        int read_count = i2c_read_blocking(I2C_PORT, addr, reply_data, read_len, false);
                        if (read_count == read_len) {
                            send_response(cmd | 0x80, reply_data, read_len);
                        } else {
                            success = 0;
                        }
                    } else {
                        success = 0; // 超出最大读取长度
                    }
                } else if (success && read_len == 0) {
                    reply_data[0] = 0x00;
                    send_response(cmd | 0x80, reply_data, 1);
                }

                if (!success) {
                    send_response(cmd | 0x80, NULL, 0);
                }
            }
            break;
    }
}

int main() {
    stdio_init_all();

    setup_slave();

    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

// 接收状态机变量升级
    int state = 0;
    uint8_t rx_cmd = 0, rx_calc_checksum = 0;
    uint8_t len_lsb = 0;
    uint16_t rx_len = 0;
    uint8_t rx_buf[1024]; 
    int rx_idx = 0;
    
    // 新增：超时计数器
    int timeout_counter = 0; 

    while (true) {
        int c = getchar_timeout_us(100); 
        
        if (c != PICO_ERROR_TIMEOUT) {
            timeout_counter = 0; // 只要收到数据，立刻清零超时计数器
            uint8_t byte = (uint8_t)c;

            switch (state) {
                case 0: // 等待包头
                    if (byte == HEADER_BYTE) state = 1;
                    break;
                case 1: // 接收命令
                    rx_cmd = byte;
                    rx_calc_checksum = byte;
                    state = 2;
                    break;
                case 2: // 接收长度低字节 (LSB)
                    len_lsb = byte;
                    rx_calc_checksum += byte;
                    state = 3;
                    break;
                case 3: // 接收长度高字节 (MSB)
                    rx_len = len_lsb | (byte << 8);
                    rx_calc_checksum += byte;
                    rx_idx = 0;
                    
                    // 【防线 1】：长度越界拦截
                    if (rx_len > sizeof(rx_buf)) {
                        state = 0; // 长度荒谬（比如几千字节），直接丢弃，重新等包头
                    } else {
                        state = (rx_len > 0) ? 4 : 5;
                    }
                    break;
                case 4: // 接收数据
                    if (rx_idx < sizeof(rx_buf)) {
                        rx_buf[rx_idx++] = byte;
                    }
                    rx_calc_checksum += byte;
                    if (rx_idx >= rx_len) state = 5;
                    break;
                case 5: // 接收并验证校验和
                    if (byte == rx_calc_checksum) {
                        state = 6; 
                    } else {
                        state = 0; 
                    }
                    break;
                case 6: // 接收并验证帧尾
                    if (byte == TAIL_BYTE) {
                        if (rx_len <= sizeof(rx_buf)) {
                            process_frame(rx_cmd, rx_buf, rx_len);
                        }
                    }
                    state = 0; // 一帧处理完毕（或尾部错误），重置
                    break;
            }
        } else {
            // 【防线 2】：空闲超时重置
            // c == PICO_ERROR_TIMEOUT 说明 100us 内没有收到数据
            if (state != 0) {
                timeout_counter++;
                // 连续 1000 次超时 (1000 * 100us = 100ms) 说明上位机发了一半卡住了
                if (timeout_counter > 1000) { 
                    state = 0; // 强制复位状态机
                    timeout_counter = 0;
                }
            }
        }
    }
    return 0;
}

// === 测试用的从机 (Slave) 升级 ===
// 将容量升级至 512 字节，内存地址指针升级为 uint16_t，支持连续跨页读写。
static struct
{
    uint8_t mem[512];
    uint16_t mem_address; // 升级为 16位，防止递增时溢出
    bool mem_address_written;
} context;

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event)
{
    switch (event)
    {
    case I2C_SLAVE_RECEIVE: 
        if (!context.mem_address_written)
        {
            // 为了保持测试简单，依然使用 1字节 设置起始地址 (0-255)
            // 但因为 mem_address 是 16 位的，后续的数据可以连续写入到 256 之后
            context.mem_address = i2c_read_byte_raw(i2c);
            context.mem_address_written = true;
        }
        else
        {
            // 写入并自增，对 512 取模防止越界
            context.mem[context.mem_address % 512] = i2c_read_byte_raw(i2c);
            context.mem_address++;
        }
        break;
    case I2C_SLAVE_REQUEST: 
        // 读出并自增，对 512 取模
        i2c_write_byte_raw(i2c, context.mem[context.mem_address % 512]);
        context.mem_address++;
        break;
    case I2C_SLAVE_FINISH: 
        context.mem_address_written = false;
        break;
    default:
        break;
    }
}

static void setup_slave()
{
    gpio_init(I2C_SLAVE_SDA_PIN);
    gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SDA_PIN);

    gpio_init(I2C_SLAVE_SCL_PIN);
    gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SLAVE_SCL_PIN);

    i2c_init(i2c0, 100*1000);
    // 配置 I2C0 为从机模式，地址 0x60
    i2c_slave_init(i2c0, 0x60, &i2c_slave_handler);
}