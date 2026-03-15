#ifndef __RGB_H__
#define __RGB_H__

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* ================= 颜色枚举 ================= */
typedef enum {
    RGB_COLOR_OFF = 0,
    RGB_COLOR_RED,
    RGB_COLOR_GREEN,
    RGB_COLOR_BLUE,
    RGB_COLOR_YELLOW,  // 红 + 绿
    RGB_COLOR_CYAN,    // 绿 + 蓝
    RGB_COLOR_PURPLE,  // 红 + 蓝
    RGB_COLOR_WHITE    // 红 + 绿 + 蓝
} RGB_Color_t;

/* ================= 句柄定义 ================= */
typedef struct {
    /* 硬件引脚配置 */
    GPIO_TypeDef* r_port; uint16_t r_pin;
    GPIO_TypeDef* g_port; uint16_t g_pin;
    GPIO_TypeDef* b_port; uint16_t b_pin;
    
    /* 配置参数 */
    uint8_t active_level; // 有效电平: 1=高电平亮(共阴), 0=低电平亮(共阳)
    
    /* 当前状态 */
    RGB_Color_t current_color;
} RGB_HandleTypeDef;

/* ================= API 接口 ================= */

// 初始化 RGB 灯
void RGB_Init(RGB_HandleTypeDef* h_rgb, 
              GPIO_TypeDef* r_port, uint16_t r_pin,
              GPIO_TypeDef* g_port, uint16_t g_pin,
              GPIO_TypeDef* b_port, uint16_t b_pin,
              uint8_t active_level);

// 设置 RGB 颜色
void RGB_SetColor(RGB_HandleTypeDef* h_rgb, RGB_Color_t color);

// 关闭 RGB 灯
void RGB_Off(RGB_HandleTypeDef* h_rgb);

#endif /* __RGB_H__ */
