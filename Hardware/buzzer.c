#include "buzzer.h"

static void Buzzer_WritePin(Buzzer_HandleTypeDef* h_buzz, bool state) {
    GPIO_PinState pin_state;
    if (h_buzz->active_level == 1) {
        pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        pin_state = state ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(h_buzz->port, h_buzz->pin, pin_state);
}

void Buzzer_Init(Buzzer_HandleTypeDef* h_buzz, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level) {
    h_buzz->port = port;
    h_buzz->pin = pin;
    h_buzz->active_level = active_level;
    h_buzz->is_beeping = false;
    h_buzz->stop_tick = 0;
    
    Buzzer_Off(h_buzz);
}

void Buzzer_On(Buzzer_HandleTypeDef* h_buzz) {
    Buzzer_WritePin(h_buzz, true);
    h_buzz->is_beeping = true;
    h_buzz->stop_tick = 0; // 取消定时关闭
}

void Buzzer_Off(Buzzer_HandleTypeDef* h_buzz) {
    Buzzer_WritePin(h_buzz, false);
    h_buzz->is_beeping = false;
}

void Buzzer_Toggle(Buzzer_HandleTypeDef* h_buzz) {
    if (h_buzz->is_beeping) {
        Buzzer_Off(h_buzz);
    } else {
        Buzzer_On(h_buzz);
    }
}

// 异步鸣叫
void Buzzer_Beep(Buzzer_HandleTypeDef* h_buzz, uint32_t duration_ms) {
    Buzzer_WritePin(h_buzz, true);
    h_buzz->is_beeping = true;
    h_buzz->stop_tick = HAL_GetTick() + duration_ms; // 记录应该关闭的时间点
}

// 非阻塞轮询任务
void Buzzer_Loop(Buzzer_HandleTypeDef* h_buzz) {
    if (h_buzz->is_beeping && h_buzz->stop_tick != 0) {
        if (HAL_GetTick() >= h_buzz->stop_tick) {
            Buzzer_Off(h_buzz); // 时间到了，自动关闭
        }
    }
}
