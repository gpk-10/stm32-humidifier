#include "my_i2c.h"

void I2C_Start(SoftI2C_t* i2c) {
    i2c->Set_SDA(1);
    i2c->Set_SCL(1);
    i2c->Delay();
    i2c->Set_SDA(0);
    i2c->Delay();
    i2c->Set_SCL(0);
    i2c->Delay();
}

void I2C_Stop(SoftI2C_t* i2c) {
    i2c->Set_SDA(0);
    i2c->Set_SCL(1);
    i2c->Delay();
    i2c->Set_SDA(1);
    i2c->Delay();
}

void I2C_SendByte(SoftI2C_t* i2c, uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        i2c->Set_SDA((byte & 0x80) ? 1 : 0);
        byte <<= 1;
        i2c->Delay();
        i2c->Set_SCL(1);
        i2c->Delay();
        i2c->Set_SCL(0);
        i2c->Delay();
    }
}

uint8_t I2C_ReadByte(SoftI2C_t* i2c) {
    uint8_t byte = 0;
    i2c->Set_SDA(1);
    for (uint8_t i = 0; i < 8; i++) {
        i2c->Set_SCL(1);
        i2c->Delay();
        byte <<= 1;
        if (i2c->Read_SDA()) {
            byte |= 0x01;
        }
        i2c->Set_SCL(0);
        i2c->Delay();
    }
    return byte;
}

uint8_t I2C_WaitAck(SoftI2C_t* i2c) {
    uint8_t ack = 0;
    i2c->Set_SDA(1);
    i2c->Delay();
    i2c->Set_SCL(1);
    i2c->Delay();
    if (i2c->Read_SDA()) {
        ack = 1;
    }
    i2c->Set_SCL(0);
    i2c->Delay();
    return ack;
}

void I2C_SendAck(SoftI2C_t* i2c, uint8_t ack) {
    i2c->Set_SDA(ack);
    i2c->Delay();
    i2c->Set_SCL(1);
    i2c->Delay();
    i2c->Set_SCL(0);
    i2c->Delay();
}
