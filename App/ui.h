#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include <stdbool.h>

/* ================== 数据结构定义 (Data Transfer Objects) ================== */

// 1. 主页数据包
typedef struct {
    float humidity;       // 当前湿度
    float temperature;    // 当前温度
    uint8_t target_hum;   // 设定目标湿度
    bool is_running;      // 加湿器运行状态
    bool is_wifi_connected; // WiFi连接状态
    bool is_auto_mode;    // true=自动模式, false=手动模式
    uint8_t water_level;  // 0:Empty, 1:Mid, 2:Full
} UI_HomeData_t;

// 2. 设置页数据包
typedef struct {
    uint8_t target_hum;   // 当前设定的目标值
    bool is_editing;      // (可选) 是否处于编辑闪烁状态
} UI_SetData_t;

// 3. WiFi页数据包
typedef struct {
    const char* ssid;     // WiFi 名称
    const char* pwd;      // WiFi 密码
    const char* ip;       // IP 地址 (字符串形式)
    uint32_t port;        // 端口
    uint8_t mode;         // 1:STA模式, 2:AP模式

} UI_WiFiData_t;

/* ================== 接口函数 ================== */

/**
 * @brief UI系统初始化 (初始化OLED)
 */
void UI_Init(void);

/**
 * @brief 开机动画
 */
void UI_UpdateBootProgress(uint8_t percent, char* info_text);

/**
 * @brief 绘制主页 (仪表盘)
 * @param data: 主页数据结构体指针
 */
void UI_DrawHome(const UI_HomeData_t *data);

/**
 * @brief 绘制设置页 (目标值调整)
 * @param data: 设置页数据结构体指针
 */
void UI_DrawSetting(const UI_SetData_t *data);

/**
 * @brief 绘制WiFi信息页 (名称/密码/IP)
 * @param data: WiFi数据结构体指针
 */
void UI_DrawWiFi(const UI_WiFiData_t *data);

#endif /* __UI_H__ */
