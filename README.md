## RP2040用作I2C接口的关键优势
RP2040 的 I2C IP 核使用的是 Synopsys（新思科技） 的商业 IP --  DW_apb_i2c。

这是一款在半导体业界极其成熟、应用非常广泛的 I2C IP。

可以灵活调节CLK信号的High Time和Low Time，可以满足microchip的时序要求

## 接线说明
I2C主机接口

GP6 -- I2C1 SDA

GP7 -- I2C1 SCL

## 功能

0x01 / 0x81: 获取设备特征码 (Get Signature)

0x02 / 0x82: 配置 I2C 波特率 (Set Baudrate)

0x03 / 0x83: I2C 写操作 (I2C Write)

0x04 / 0x84: I2C 读操作 (I2C Read)

0x05 / 0x85: 软复位I2C外设 (I2C Reset) 注：会保持上次的波特率设置

0x06 / 0x86: I2C 复合写读操作 (Write-Read)

## 附加功能
I2C从机7bit地址：0x60

GP4 -- I2C0 SDA

GP5 -- I2C0 SCL

The slave implements a 512 byte memory. To write a series of bytes, the master first
writes the memory address, followed by the data. The address is automatically incremented
for each byte transferred, looping back to 0 upon reaching the end. Reading is done
sequentially from the current memory address.

## 时序说明
最快220us的间隔连续执行I2C读写

100kHz波特率设置下，T (clk high) = 4.5us，T (clk low) = 6.4us

<img width="1225" height="410" alt="image" src="https://github.com/user-attachments/assets/8bae670a-dba9-457c-89a2-d668523d0138" />

<img width="1920" height="976" alt="image" src="https://github.com/user-attachments/assets/95a51bc6-ce05-4f5c-8aff-1231a3fee4f5" />
