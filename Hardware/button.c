#include "button.h"

/* 链表头指针 */
static Button_t* head_handle = NULL;

/* 内部函数：读取物理引脚电平 */
static uint8_t Button_ReadPin(Button_t* handle) {
    return HAL_GPIO_ReadPin(handle->port, handle->pin) == handle->active_level ? 1 : 0;
}

/* 1. 初始化并注册按键 */
void Button_Init(Button_t* handle, GPIO_TypeDef* port, uint16_t pin, uint8_t active_level, void (*cb)(void*, Button_Event_t)) {
    // 填充基本信息
    handle->port = port;
    handle->pin = pin;
    handle->active_level = active_level;
    handle->state = BTN_STATE_IDLE;
    handle->event = BTN_EVENT_NONE;
    handle->ticks = 0;
    handle->repeat_cnt = 0;
    handle->Callback = cb;

    // 插入链表 (头插法，简单高效)
    handle->next = head_handle;
    head_handle = handle;
}

/* 2. 单个按键的状态机处理 */
static void Button_Handler(Button_t* handle) {
    uint8_t is_pressed = Button_ReadPin(handle);
    handle->event = BTN_EVENT_NONE; // 每一轮先清除事件

    switch (handle->state) {
        case BTN_STATE_IDLE:
            if (is_pressed) {
                handle->ticks = 0;
                handle->state = BTN_STATE_DEBOUNCE;
            }
            break;

        case BTN_STATE_DEBOUNCE:
            if (is_pressed) {
                // 等待消抖时间
                if (++handle->ticks >= (BUTTON_DEBOUNCE_TIME / BUTTON_SCAN_INTERVAL)) {
                    handle->state = BTN_STATE_DOWN;
                    handle->event = BTN_EVENT_DOWN; // 触发按下事件
                    handle->ticks = 0;

                    // 如果是从空闲进来的，说明是第1次按下
                    if(handle->repeat_cnt == 0) {
                        handle->repeat_cnt = 1; 
                    }
                }
            } else {
                handle->state = BTN_STATE_IDLE; // 只是抖动，误触
                handle->repeat_cnt = 0; // 抖动复位
            }
            break;

        case BTN_STATE_DOWN:
            if (is_pressed) {
                // 持续按下，检测长按
                if (++handle->ticks >= (BUTTON_LONG_PRESS_TIME / BUTTON_SCAN_INTERVAL)) {
                    handle->state = BTN_STATE_LONG;
                    handle->event = BTN_EVENT_LONG_PRESS;
                    handle->ticks = 0;
                }
            } else {
                // 松手逻辑：区分是第一次松手还是第二次松手
                handle->event = BTN_EVENT_UP;
                
                if (handle->repeat_cnt == 1) {
                    // 第1次松手 -> 进入等待双击状态
                    handle->state = BTN_STATE_WAIT_DOUBLE;
                    handle->ticks = 0;
                } 
                else if (handle->repeat_cnt >= 2) {
                    // 第2次松手 -> 双击结束，直接回 IDLE，防止再次触发单击
                    handle->state = BTN_STATE_IDLE;
                    handle->repeat_cnt = 0; // 清零计数
                }
            }
            break;

        case BTN_STATE_LONG:
            if (!is_pressed) {
                handle->event = BTN_EVENT_UP;
                handle->state = BTN_STATE_IDLE; // 长按抬起后，直接回空闲，不检测双击
            } else {
                // 如果需要 "长按保持" 事件 (Long Hold)，可以在这里周期触发
            }
            break;

        case BTN_STATE_WAIT_DOUBLE:
            // 在等待期间
            if (is_pressed) {
                // 再次按下 -> 双击
                handle->repeat_cnt++; // 计数变成 2
                // 如果在间隔时间内再次按下 -> 双击
                handle->event = BTN_EVENT_DOUBLE_CLICK;
                handle->state = BTN_STATE_DOWN; // 如果想支持三连击，这里要调整；否则视为新一轮按下但跳过Down事件
                // 为了简化，双击后通常等待松手回IDLE，或者视作下一次按下的开始
                handle->ticks = 0;
            } else {
                // 持续松手，检测超时
                if (++handle->ticks >= (BUTTON_DOUBLE_CLICK_GAP / BUTTON_SCAN_INTERVAL)) {
                    // 超时未按下 -> 确认为单击
                    if (handle->repeat_cnt == 1) {
                        handle->event = BTN_EVENT_CLICK;
                    }
                    handle->state = BTN_STATE_IDLE;
                    handle->repeat_cnt = 0; // 复位
                }
            }
            break;
    }

    // 如果产生了事件且有回调，执行回调
    if (handle->event != BTN_EVENT_NONE && handle->Callback) {
        handle->Callback(handle, handle->event);
    }
}

/* 3. 全局扫描函数 (放在定时器中) */
void Button_Process(void) {
    Button_t* target = head_handle;
    while (target != NULL) {
        Button_Handler(target);
        target = target->next;
    }
}

/* 4. 获取事件 (轮询用) */
Button_Event_t Button_GetEvent(Button_t* handle) {
    return handle->event;
}
