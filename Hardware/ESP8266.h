#ifndef __ESP8266_H__
#define __ESP8266_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h" 
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ================= 用户配置区 ================= */
#define ESP_SERVER_PORT     8080               // 开启的本地TCP服务器监听端口
#define ESP_RX_BUFFER_SIZE  512                // 串口接收缓冲区大小 (防止WiFi数据包过大溢出)

/* ================= 状态机定义 ================= */
// 使用状态机(State Machine)管理WiFi状态，避免死循环阻塞单片机
typedef enum {
    ESP_STATE_RESET,        // 复位/初始化阶段：准备下发AT指令
    ESP_STATE_CONFIG_STA,   // 配置阶段：连接路由器及建立TCP服务器
    ESP_STATE_RUNNING,      // 正常运行阶段：监听并处理手机发来的数据
    ESP_STATE_ERROR,        // 错误状态：连接失败，等待延时后重试
} ESP_State_t;

/* ================= 句柄定义 ================= */
// 将ESP8266的所有属性打包成一个结构体句柄
typedef struct {
    UART_HandleTypeDef* huart;             // 绑定的串口句柄 (如 &huart2)
    uint8_t rx_buffer[ESP_RX_BUFFER_SIZE]; // 接收缓冲区 (存放串口收到的原始数据)
    volatile uint16_t rx_len;              // 当前接收缓冲区的数据长度 (volatile防止编译器优化)
    ESP_State_t state;                     // 当前工作状态
    uint32_t last_tick;                    // 用于非阻塞延时的计时器 (记录上一次的时间戳)
    bool is_initialized;                   // 初始化完成标志位
    char ssid[32];                         // 目标 WiFi 名称 (从Flash读取或配网获取)
    char pwd[32];                          // 目标 WiFi 密码 (从Flash读取或配网获取)
    char local_ip[16];                     // 保存路由器动态分配的局域网 IP 地址
} ESP8266_HandleTypeDef;

/* ================= API 接口函数 ================= */

// 初始化并绑定串口
void ESP_Init(ESP8266_HandleTypeDef* h_esp, UART_HandleTypeDef* huart, char *ssid, char *pwd);
// WiFi核心主循环 (需放在main的while(1)中不断轮询)
void ESP_Loop(ESP8266_HandleTypeDef* h_esp);
// 串口中断接收回调函数 (每次收到1字节数据时调用)
void ESP_RxCallback(ESP8266_HandleTypeDef* h_esp, uint8_t received_byte);
// 从缓冲区中提取完整的指令数据 (非阻塞)
bool ESP_GetMessage(ESP8266_HandleTypeDef* h_esp, char* msg_buf, uint16_t max_len);
// 智能一键配网函数 (阻塞式，获取手机广播的账号密码)
bool ESP_RunSmartConfig(ESP8266_HandleTypeDef* h_esp);

#ifdef __cplusplus
}
#endif

#endif /* __ESP8266_H__ */
