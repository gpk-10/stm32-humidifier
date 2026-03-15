#include "rgb.h"

// 内部辅助函数：设置单个引脚的亮灭
static void RGB_WritePin(GPIO_TypeDef* port, uint16_t pin, uint8_t active_level, bool is_on) {
    GPIO_PinState state;
    if (active_level == 1) {
        state = is_on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        state = is_on ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(port, pin, state);
}

void RGB_Init(RGB_HandleTypeDef* h_rgb, 
              GPIO_TypeDef* r_port, uint16_t r_pin,
              GPIO_TypeDef* g_port, uint16_t g_pin,
              GPIO_TypeDef* b_port, uint16_t b_pin,
              uint8_t active_level) 
{
    h_rgb->r_port = r_port; h_rgb->r_pin = r_pin;
    h_rgb->g_port = g_port; h_rgb->g_pin = g_pin;
    h_rgb->b_port = b_port; h_rgb->b_pin = b_pin;
    h_rgb->active_level = active_level;
    
    RGB_Off(h_rgb); // 默认关闭
}

void RGB_SetColor(RGB_HandleTypeDef* h_rgb, RGB_Color_t color) {
    h_rgb->current_color = color;
    
    bool r_on = false, g_on = false, b_on = false;

    switch (color) {
        case RGB_COLOR_RED:    r_on = true; break;
        case RGB_COLOR_GREEN:  g_on = true; break;
        case RGB_COLOR_BLUE:   b_on = true; break;
        case RGB_COLOR_YELLOW: r_on = true; g_on = true; break;
        case RGB_COLOR_CYAN:   g_on = true; b_on = true; break;
        case RGB_COLOR_PURPLE: r_on = true; b_on = true; break;
        case RGB_COLOR_WHITE:  r_on = true; g_on = true; b_on = true; break;
        case RGB_COLOR_OFF:
        default:               break; // 全部为 false
    }

    RGB_WritePin(h_rgb->r_port, h_rgb->r_pin, h_rgb->active_level, r_on);
    RGB_WritePin(h_rgb->g_port, h_rgb->g_pin, h_rgb->active_level, g_on);
    RGB_WritePin(h_rgb->b_port, h_rgb->b_pin, h_rgb->active_level, b_on);
}

void RGB_Off(RGB_HandleTypeDef* h_rgb) {
    RGB_SetColor(h_rgb, RGB_COLOR_OFF);
}
