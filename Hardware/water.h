#ifndef __WATER_H__
#define __WATER_H__

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* 水位状态枚举 */
typedef enum {
    WATER_EMPTY = 0, // 缺水 (危险)
    WATER_MID,       // 正常 (有水但未满)
    WATER_FULL       // 满水
} Water_Status_t;

/* 句柄定义 */
typedef struct {
    /* 缺水检测传感器 (装在底部) */
    GPIO_TypeDef* low_port;
    uint16_t low_pin;
    
    /* 满水检测传感器 (装在顶部) */
    GPIO_TypeDef* high_port;
    uint16_t high_pin;

    /* 传感器有效电平 (假设有水输出高电平=1，无水=0) */
    /* 非接触传感器通常有灵敏度旋钮，有的输出高有的输出低，这里做个配置 */
    uint8_t active_level; 
} Water_HandleTypeDef;

/* 初始化 */
void Water_Init(Water_HandleTypeDef* h_water, 
                GPIO_TypeDef* l_port, uint16_t l_pin,
                GPIO_TypeDef* h_port, uint16_t h_pin,
                uint8_t active_level);

/* 获取当前水位状态 */
Water_Status_t Water_GetStatus(Water_HandleTypeDef* h_water);

#endif
