#ifndef __BOARD_I2C_PORT_H
#define __BOARD_I2C_PORT_H

#include "my_i2c.h"

// 对外提供一个已配置好的 I2C 总线对象
extern SoftI2C_t bus_aht20;

void Board_I2C_Init(void);

#endif
