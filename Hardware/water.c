#include "water.h"

void Water_Init(Water_HandleTypeDef* h_water, 
                GPIO_TypeDef* l_port, uint16_t l_pin,
                GPIO_TypeDef* h_port, uint16_t h_pin,
                uint8_t active_level) 
{
    h_water->low_port = l_port;
    h_water->low_pin = l_pin;
    h_water->high_port = h_port;
    h_water->high_pin = h_pin;
    h_water->active_level = active_level;
}

Water_Status_t Water_GetStatus(Water_HandleTypeDef* h_water) {
    // 读取引脚电平
    int low_val = HAL_GPIO_ReadPin(h_water->low_port, h_water->low_pin);
    int high_val = HAL_GPIO_ReadPin(h_water->high_port, h_water->high_pin);
    
    // 判断逻辑 (假设 active_level = 1 代表有水)
    int has_water_low = (low_val == h_water->active_level);
    int has_water_high = (high_val == h_water->active_level);

    if (!has_water_low) {
        return WATER_EMPTY; // 底部都没水了 -> 缺水
    } else if (has_water_high) {
        return WATER_FULL;  // 顶部也有水了 -> 满水
    } else {
        return WATER_MID;   // 底部有水，顶部没水 -> 正常
    }
}
