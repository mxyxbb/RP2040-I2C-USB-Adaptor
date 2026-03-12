## RP2040用作I2C接口的关键优势
RP2040 的 I2C IP 核使用的是 Synopsys（新思科技） 的商业 IP --  DW_apb_i2c。

这是一款在半导体业界极其成熟、应用非常广泛的 I2C IP。

可以灵活调节CLK信号的High Time和Low Time，可以满足microchip的时序要求

## 接线说明
I2C主机接口

GP6 -- I2C1 SDA

GP7 -- I2C1 SCL

## 功能

<img width="1062" height="677" alt="image" src="https://github.com/user-attachments/assets/2f25e20e-fe6b-4e94-859a-9699a222b670" />

0x01 / 0x81: 获取设备特征码 (Get Signature)

0x02 / 0x82: 配置 I2C 波特率 (Set Baudrate)

0x03 / 0x83: I2C 写操作 (I2C Write)

0x04 / 0x84: I2C 读操作 (I2C Read)

0x05 / 0x85: 软复位I2C外设 (I2C Reset) 注：会保持上次的波特率设置

0x06 / 0x86: I2C 复合写读操作 (Write-Read)

## 附加功能
I2C从机，用于基础通信测试  
I2C从机7bit地址：0x60  
GP4 -- I2C0 SDA  
GP5 -- I2C0 SCL  
The slave implements a 512 byte memory. To write a series of bytes, the master first
writes the memory address, followed by the data. The address is automatically incremented
for each byte transferred, looping back to 0 upon reaching the end. Reading is done
sequentially from the current memory address.
<img width="1070" height="678" alt="image" src="https://github.com/user-attachments/assets/250591e5-baef-48c6-be2b-77f6d61e753d" />

## 时序说明
通过打包通信，将多帧指令放到一个数据包的方式，最快220us的间隔连续执行I2C读写  
普通的通信方式单个usb数据包只发送一帧指令，比较浪费usb速率，这种方式的连续执行间隔约为15ms  
100kHz波特率设置下，T (clk high) = 4.5us，T (clk low) = 6.4us  

<img width="1225" height="410" alt="image" src="https://github.com/user-attachments/assets/8bae670a-dba9-457c-89a2-d668523d0138" />
打包通信间隔示意图如下  
<img width="1920" height="976" alt="image" src="https://github.com/user-attachments/assets/95a51bc6-ce05-4f5c-8aff-1231a3fee4f5" />
单帧通信间隔示意图如下  
<img width="935" height="581" alt="image" src="https://github.com/user-attachments/assets/e95993ac-19a2-4385-95db-f77ce672b064" />
