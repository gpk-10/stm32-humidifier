#ifndef __SYS_CONFIG_H__
#define __SYS_CONFIG_H__

#include "stm32f1xx_hal.h"
#include <string.h>

// 定义 Flash 最后一页的地址 (STM32F103C8T6 64KB Flash, Page 63)
#define CONFIG_FLASH_ADDR  0x0800FC00 
#define CONFIG_MAGIC_NUM   0xAA55AA55 // 用于判断 Flash 里是否有数据

// 系统配置结构体
typedef struct {
    uint32_t magic;      // 用于校验
    char wifi_ssid[32];  // WiFi 名称
    char wifi_pwd[32];   // WiFi 密码
    uint8_t target_hum;  // 目标湿度 (顺便把这个也存进去)
    uint8_t volume;      // 蜂鸣器音量 (预留)
} System_Config_t;

// 全局变量声明
extern System_Config_t sys_cfg;

// 函数接口
void Config_Init(void);      // 初始化（读取或恢复默认）
void Config_Save(void);      // 保存当前参数到 Flash
void Config_Reset(void);     // 恢复出厂设置

#endif
