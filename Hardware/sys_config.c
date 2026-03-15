#include "sys_config.h"
#include <string.h>

// 全局配置实例
System_Config_t sys_cfg;

/**
 * @brief 恢复默认出厂参数
 * @note 出厂时必须清空 WiFi 账号密码，以便开机自动触发配网流程
 */
static void Config_SetDefault(void) {
    sys_cfg.magic = CONFIG_MAGIC_NUM; // 写入魔数，标记Flash已被初始化
    
    // 清空 WiFi 信息，变为空白出厂状态
    memset(sys_cfg.wifi_ssid, 0, sizeof(sys_cfg.wifi_ssid));
    memset(sys_cfg.wifi_pwd,  0, sizeof(sys_cfg.wifi_pwd));
    
    sys_cfg.target_hum = 60; // 默认目标湿度设为 60%
}

/**
 * @brief 从 Flash 中读取系统配置
 */
void Config_Init(void) {
    // 将 Flash 特定地址强制转换为结构体指针直接读取
    System_Config_t *p_flash = (System_Config_t *)CONFIG_FLASH_ADDR;
    
    // 检查魔数：如果数据损坏或是第一次全新烧录的芯片，魔数对不上
    if (p_flash->magic != CONFIG_MAGIC_NUM) {
        Config_SetDefault(); // 加载出厂默认值
        Config_Save();       // 格式化并写入 Flash
    } else {
        // 数据有效，将 Flash 中的数据拷贝到 RAM 中的全局变量
        memcpy(&sys_cfg, p_flash, sizeof(System_Config_t));
    }
}

/**
 * @brief 将当前配置保存到 STM32 内部 Flash
 */
void Config_Save(void) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    // 1. 解锁 Flash 控制寄存器
    HAL_FLASH_Unlock();

    // 2. 擦除目标页 (Flash 特性：写入前必须先擦除为 0xFF)
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = CONFIG_FLASH_ADDR;
    EraseInitStruct.NbPages     = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    // 3. 逐字 (32-bit Word) 写入数据
    uint32_t *p_source = (uint32_t *)&sys_cfg;
    uint32_t len = sizeof(System_Config_t);
    
    if (len % 4 != 0) len += (4 - (len % 4)); // 保证按 4 字节对齐

    for (uint32_t i = 0; i < len; i += 4) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                          CONFIG_FLASH_ADDR + i, 
                          *p_source);
        p_source++;
    }

    // 4. 重新上锁，防止程序跑飞误修改 Flash
    HAL_FLASH_Lock();
}

/**
 * @brief 强行重置为出厂状态并保存
 */
void Config_Reset(void) {
    Config_SetDefault();
    Config_Save();
}
