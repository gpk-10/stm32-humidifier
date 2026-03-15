#ifndef __HE_30_H__
#define __HE_30_H__

#include "stm32f1xx_hal.h" // 根据具体芯片调整
#include <stdbool.h>

/* ================= 句柄定义 ================= */
typedef struct {
    /* 硬件引脚配置 */
    GPIO_TypeDef* port;       // GPIO端口 (如 GPIOA)
    uint16_t pin;             // GPIO引脚 (如 GPIO_PIN_1)
    
    /* 配置参数 */
    uint8_t active_level;     // 有效电平: 1=高电平开启, 0=低电平开启
    
    /* 运行状态 (只读) */
    bool is_running;          // 当前是否正在喷雾
} HE30_HandleTypeDef;

/* ================= API 接口 ================= */

/**
 * @brief 初始化雾化器句柄
 * @param h_he30: 句柄指针
 * @param port: GPIO端口
 * @param pin: GPIO引脚
 * @param active_level: 开启时的电平 (1:高电平有效, 0:低电平有效)
 */
void HE30_Init(HE30_HandleTypeDef* h_he30, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level);

/**
 * @brief 开启雾化
 */
void HE30_Start(HE30_HandleTypeDef* h_he30);

/**
 * @brief 停止雾化
 */
void HE30_Stop(HE30_HandleTypeDef* h_he30);

/**
 * @brief 切换状态 (开<->关)
 */
void HE30_Toggle(HE30_HandleTypeDef* h_he30);

/**
 * @brief 获取当前状态
 * @return true:正在运行, false:已停止
 */
bool HE30_IsRunning(HE30_HandleTypeDef* h_he30);

#endif /* __HE30_H__ */
