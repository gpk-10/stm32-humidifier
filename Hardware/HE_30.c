#include "HE_30.h"

// 内部辅助函数：根据逻辑电平写入引脚
static void HE30_WritePin(HE30_HandleTypeDef* h_he30, bool state) {
    GPIO_PinState pin_state;
    
    if (h_he30->active_level == 1) {
        // 高电平有效：state=true -> Set, state=false -> Reset
        pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        // 低电平有效：state=true -> Reset, state=false -> Set
        pin_state = state ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    
    HAL_GPIO_WritePin(h_he30->port, h_he30->pin, pin_state);
}

/* ================= 接口实现 ================= */

void HE30_Init(HE30_HandleTypeDef* h_he30, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level) {
    h_he30->port = port;
    h_he30->pin = pin;
    h_he30->active_level = active_level;
    
    // 初始化默认关闭
    h_he30->is_running = false;
    HE30_Stop(h_he30);
}

void HE30_Start(HE30_HandleTypeDef* h_he30) {
    if (!h_he30->is_running) {
        HE30_WritePin(h_he30, true);
        h_he30->is_running = true;
    }
}

void HE30_Stop(HE30_HandleTypeDef* h_he30) {
    if (h_he30->is_running) {
        HE30_WritePin(h_he30, false);
        h_he30->is_running = false;
    }
}

void HE30_Toggle(HE30_HandleTypeDef* h_he30) {
    if (h_he30->is_running) {
        HE30_Stop(h_he30);
    } else {
        HE30_Start(h_he30);
    }
}

bool HE30_IsRunning(HE30_HandleTypeDef* h_he30) {
    return h_he30->is_running;
}
