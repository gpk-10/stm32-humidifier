#include "aht20.h"
#include "my_i2c.h"
#include "board_i2c_port.h"
#include "main.h" // for HAL_Delay

// 7位地址 0x38 (手册第10页)
#define AHT20_ADDR_7BIT  0x38  
#define AHT20_WRITE_ADDR (AHT20_ADDR_7BIT << 1)       // 0x70
#define AHT20_READ_ADDR  ((AHT20_ADDR_7BIT << 1) | 1) // 0x71

// CRC8 校验算法 (参考手册第12页)
// 多项式: X8 + X5 + X4 + 1 (0x31)
// 初始值: 0xFF
static uint8_t Calc_CRC8(uint8_t *message, uint8_t Num) {
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF; // 初始值

    for (byte = 0; byte < Num; byte++) {
        crc ^= (message[byte]);
        for (i = 8; i > 0; --i) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

void AHT20_Init_Device(void) {
    Board_I2C_Init();
    HAL_Delay(40); // 上电后等待40ms (手册规定)
    
    // 实际工程中，建议在这里读取一次状态字
    // 如果 Status Bit[3] != 1 (未校准)，则需要发送 0xBE 初始化命令
    // 这里为了保持简洁，且 AHT20 通常出厂已校准，暂省略
}

uint8_t AHT20_Read_Values(float *temp, float *hum) {
    SoftI2C_t *bus = &bus_aht20;

    /* 1. 发送触发测量命令 (手册 5.2 节) */
    I2C_Start(bus);
    I2C_SendByte(bus, AHT20_WRITE_ADDR); // 0x70
    if (I2C_WaitAck(bus)) { I2C_Stop(bus); return 1; } // 无应答
    
    I2C_SendByte(bus, 0xAC); // 触发测量
    I2C_WaitAck(bus);
    I2C_SendByte(bus, 0x33); // 参数1
    I2C_WaitAck(bus);
    I2C_SendByte(bus, 0x00); // 参数2
    I2C_WaitAck(bus);
    I2C_Stop(bus);

    /* 2. 等待测量完成 (手册要求 >80ms) */
    HAL_Delay(80);

    /* 3. 读取 7 个字节数据 */
    I2C_Start(bus);
    I2C_SendByte(bus, AHT20_READ_ADDR); // 0x71
    if (I2C_WaitAck(bus)) { I2C_Stop(bus); return 1; }

    uint8_t data[7]; // 【修正】这里改为7个字节
    for (int i = 0; i < 7; i++) {
        data[i] = I2C_ReadByte(bus);
        // 最后一个字节(CRC)发送 NACK，其他发送 ACK
        I2C_SendAck(bus, (i == 6) ? 1 : 0);
    }
    I2C_Stop(bus);

    /* 4. 数据解析与校验 */
    
    // 第一步：检查状态字 Bit7 (忙标志)
    // 如果 Bit7=1，说明还在测量，数据无效
    if ((data[0] & 0x80) != 0) {
        return 2; // 错误码2：设备忙
    }

    // 第二步：CRC 校验 (计算前6个字节的CRC，与第7个字节比较)
    uint8_t crc_result = Calc_CRC8(data, 6);
    if (crc_result != data[6]) {
        return 3; // 错误码3：CRC校验失败
    }

    // 第三步：数据拼接 (手册图18)
    // 湿度: data[1]<<12 | data[2]<<4 | data[3]>>4
    uint32_t rawHum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    
    // 温度: (data[3]&0x0F)<<16 | data[4]<<8 | data[5]
    uint32_t rawTemp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // 第四步：物理量转换 (手册第12页公式)
    // RH% = (S_rh / 2^20) * 100
    *hum = (float)rawHum * 100.0f / 1048576.0f;
    
    // T°C = (S_t / 2^20) * 200 - 50
    *temp = ((float)rawTemp * 200.0f / 1048576.0f) - 50.0f;

    return 0; // 读取成功
}
