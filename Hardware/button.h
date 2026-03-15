#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "stm32f1xx_hal.h" // 根据你的芯片型号调整
#include <stdint.h>
#include <stdbool.h>

/* ================== 用户配置区 ================== */
#define BUTTON_DEBOUNCE_TIME    20   // 消抖时间 (ms) -> 2 ticks
#define BUTTON_LONG_PRESS_TIME  1500 // 长按判定时间 (ms)
#define BUTTON_DOUBLE_CLICK_GAP 300  // 双击间隔时间 (ms)
#define BUTTON_SCAN_INTERVAL    10   // 扫描周期 (ms)，需与定时器调用频率一致

/* ================== 事件定义 ================== */
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_DOWN,         // 按下瞬间
    BTN_EVENT_UP,           // 抬起瞬间
    BTN_EVENT_CLICK,        // 单击 (按下->抬起->超时未再按)
    BTN_EVENT_DOUBLE_CLICK, // 双击
    BTN_EVENT_LONG_PRESS,   // 长按 (按下超过设定时间)
    BTN_EVENT_LONG_HOLD,    // 长按保持 (每隔一段时间触发一次，可选)
} Button_Event_t;

/* ================== 状态机状态 ================== */
typedef enum {
    BTN_STATE_IDLE = 0,     // 空闲
    BTN_STATE_DEBOUNCE,     // 消抖
    BTN_STATE_DOWN,         // 确认按下
    BTN_STATE_LONG,         // 长按中
    BTN_STATE_WAIT_DOUBLE,  // 等待双击
} Button_State_t;

/* ================== 按键对象结构体 ================== */
typedef struct Button {
    /* 硬件配置 */
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t active_level;   // 有效电平 (0:低电平有效/按下接地, 1:高电平有效)
    
    /* 内部状态 (用户无需关心) */
    uint16_t ticks;         // 计时器
    uint8_t  repeat_cnt;    // 连击次数
    Button_State_t state;   // 当前状态机状态
    Button_Event_t event;   // 当前触发的事件
    
    /* 链表节点 */
    struct Button* next;    // 指向下一个按键
    
    /* 回调函数 (可选，也可以用轮询方式获取事件) */
    void (*Callback)(void* btn, Button_Event_t event);
    
} Button_t;

/* ================== 对外接口 ================== */

/**
 * @brief 初始化并注册一个按键
 * @param handle: 按键对象指针
 * @param port: GPIO端口
 * @param pin: GPIO引脚
 * @param active_level: 按下时的电平 (0或1)
 * @param cb: 事件回调函数 (可为NULL)
 */
void Button_Init(Button_t* handle, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level, void (*cb)(void*, Button_Event_t));

/**
 * @brief 按键核心处理函数
 * @note  需要周期性调用 (例如在 SysTick 中每 10ms 调用一次)
 */
void Button_Process(void);

/**
 * @brief 读取按键事件 (轮询模式使用)
 * @return 当前事件
 */
Button_Event_t Button_GetEvent(Button_t* handle);

#endif /* __BUTTON_H__ */
