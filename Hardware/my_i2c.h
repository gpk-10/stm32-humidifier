#ifndef __MY_I2C_H
#define __MY_I2C_H

#include <stdint.h>

/* 定义函数指针类型 */
typedef void (*Ops_Write_Pin)(uint8_t val);
typedef uint8_t (*Ops_Read_Pin)(void);
typedef void (*Ops_Delay_Us)(void);

/* I2C 对象结构体 */
typedef struct {
    Ops_Write_Pin Set_SCL;
    Ops_Write_Pin Set_SDA;
    Ops_Read_Pin  Read_SDA;
    Ops_Delay_Us  Delay;
} SoftI2C_t;

/* 接口函数 */
void I2C_Start(SoftI2C_t* i2c);
void I2C_Stop(SoftI2C_t* i2c);
void I2C_SendByte(SoftI2C_t* i2c, uint8_t byte);
uint8_t I2C_ReadByte(SoftI2C_t* i2c);
uint8_t I2C_WaitAck(SoftI2C_t* i2c);
void I2C_SendAck(SoftI2C_t* i2c, uint8_t ack);

#endif
