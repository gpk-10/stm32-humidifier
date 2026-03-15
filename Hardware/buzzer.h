#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* ================= 句柄定义 ================= */
typedef struct {
    /* 硬件引脚配置 */
    GPIO_TypeDef* port;       
    uint16_t pin;             
    uint8_t active_level;     // 1:高电平响, 0:低电平响
    
    /* 非阻塞控制参数 */
    bool is_beeping;          // 当前是否正在鸣叫
    uint32_t stop_tick;       // 预计停止的时间戳
} Buzzer_HandleTypeDef;

/* ================= API 接口 ================= */

void Buzzer_Init(Buzzer_HandleTypeDef* h_buzz, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level);

// 基础控制
void Buzzer_On(Buzzer_HandleTypeDef* h_buzz);
void Buzzer_Off(Buzzer_HandleTypeDef* h_buzz);
void Buzzer_Toggle(Buzzer_HandleTypeDef* h_buzz);

// 异步非阻塞鸣叫 (时长 duration_ms)
void Buzzer_Beep(Buzzer_HandleTypeDef* h_buzz, uint32_t duration_ms);

// 蜂鸣器主循环 (必须放入 main while(1) 中轮询，用于异步关闭)
void Buzzer_Loop(Buzzer_HandleTypeDef* h_buzz);

#endif /* __BUZZER_H__ */
