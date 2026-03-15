#include "ui.h"
#include "OLED.h" // 引用底层的OLED驱动
#include <stdio.h> // 用于 sprintf
#include <string.h>
#include "stm32f1xx_hal.h"

/* 字体大小宏定义，方便统一调整 */
#define FONT_SMALL  OLED_6X8    // 6x8
#define FONT_MID    OLED_8X16   // 8x16 (或者12)

// 定义水箱坐标
#define TANK_X 95
#define TANK_Y 18
#define TANK_W 20
#define TANK_H 26
/* ================== 内部辅助函数 ================== */

void Draw_Water_Tank(uint8_t level) { // level: 0=Empty, 1=Mid, 2=Full
    // 1. 先画一个空心的水箱外壳 (杯子)
    OLED_DrawRectangle(TANK_X, TANK_Y, TANK_W, TANK_H, 0); 
    
    // 2. 稍微加点装饰，画个盖子或者刻度线 (可选)
    // OLED_DrawLine(TANK_X-2, TANK_Y+5, TANK_X, TANK_Y+5); // 刻度
    
    // 3. 根据水位填充
    if (level == 0) {
        // 缺水：画个叉 X 或者闪烁
        OLED_ShowString(TANK_X+7, TANK_Y+6, "!", FONT_MID); 
    }
    else if (level == 1) {
        // 正常：填充下半部分 (高度的一半)
        // 坐标计算：Y起点 = 底部 - 高度的一半
        uint8_t fill_h = TANK_H / 2;
        OLED_DrawRectangle(TANK_X + 2, TANK_Y + TANK_H - fill_h - 2,  TANK_W - 4, fill_h, 1); // 左右缩进2像素，不仅好看，还像玻璃杯壁
    }
    else if (level == 2) {
        // 满水：填充大部分 (留2像素顶部)
        OLED_DrawRectangle(TANK_X + 2, TANK_Y + 2, TANK_W - 4, TANK_H - 4, 1);
    }
}

/* ================== 接口实现 ================== */

void UI_Init(void) {
    OLED_Init();
}

// 这里我们定义一个公开接口给 app.c 调用
void UI_UpdateBootProgress(uint8_t percent, char* info_text) {
    // 1. 静态背景只画一次，避免闪烁 (优化)
    static bool is_first = true;
    if (is_first) {
        OLED_Clear();
        OLED_ShowString(20, 10, "SYSTEM BOOT", FONT_MID); // 标题
        OLED_DrawRectangle(10, 50, 108, 8, 0);      // 进度条空外框
        is_first = false;
    }

    // 2. 清除上一条文字区域 (防止文字重叠)
    // 假设文字在 Y=30, 高8像素, 宽128
    OLED_ClearArea(0, 30, 128, 8); 
    
    // 3. 显示当前正在初始化的硬件名称
    // 居中计算: (128 - 字符数*6)/2
    uint8_t x_pos = (128 - strlen(info_text) * 6) / 2;
    if(x_pos > 120) x_pos = 0; // 防止溢出
    OLED_ShowString(x_pos, 30, info_text, FONT_SMALL);

    // 4. 填充进度条
    if(percent > 100) percent = 100;
    uint8_t w = (104 * percent) / 100; // 算出内部宽度
    
    if(w > 0) {
        // 使用你的库函数画实心矩形
        // 参数: x, y, width, height, is_filled
        OLED_DrawRectangle(12, 52, w, 4, 1); 
    }
    
    // 5. 刷新到屏幕
    OLED_Update();
}

// 2. 绘制主页
void UI_DrawHome(const UI_HomeData_t *data) {
    OLED_Clear();

    // --- 顶栏 ---
    // 左上角 WiFi 状态
    if(data->is_wifi_connected) OLED_ShowString(2, 2, "WIFI:OK", FONT_SMALL);
    else                        OLED_ShowString(2, 2, "WIFI:--", FONT_SMALL);

    // 右上角 运行状态
    if(data->is_running)        OLED_ShowString(60, 2, "HE30:[RUN]", FONT_SMALL);
    else                        OLED_ShowString(60, 2, "HE30:[OFF]", FONT_SMALL);

    // 分割线
    OLED_DrawLine(0, 12, 127, 12);

    Draw_Water_Tank(data->water_level);

    // --- 中栏 (大字显示湿度) ---
    // 这里的坐标需要根据你实际字体大小微调
    char buf[16];
    sprintf(buf, "%.1f %%", data->humidity); 
    OLED_ShowString(30, 22, buf, FONT_MID); 

    // --- 底栏 ---
    OLED_DrawLine(0, 48, 127, 48);
    
    // 左下: 温度
    sprintf(buf, "T:%.1f", data->temperature);
    OLED_ShowString(2, 52, buf, FONT_SMALL);

    // 中下: 设定值
    sprintf(buf, "Set:%d", data->target_hum);
    OLED_ShowString(50, 52, buf, FONT_SMALL);

    // 右下: 模式
    if(data->is_auto_mode) OLED_ShowString(96, 52, "AUTO", FONT_SMALL);
    else                   OLED_ShowString(96, 52, "MAN ", FONT_SMALL);

    OLED_Update();
}

// 3. 绘制设置页
void UI_DrawSetting(const UI_SetData_t *data) {
    OLED_Clear();

    // 反色标题栏
    OLED_ShowString(36, 2, "SETTING", FONT_MID);

    // 中间显示大数字，带箭头装饰
    char buf[16];
    sprintf(buf, "< %d %% >", data->target_hum);
    // 居中显示 (假设16号字，宽8像素，字符串约9个字 -> 72宽)
    // X = (128-72)/2 = 28
    OLED_ShowString(28, 24, buf, FONT_MID);

    // 底部操作提示
    OLED_DrawLine(0, 50, 127, 50);
    OLED_ShowString(10, 54, "DblClick: +5%", FONT_SMALL);

    OLED_Update();
}

// 4. 绘制WiFi信息页
void UI_DrawWiFi(const UI_WiFiData_t *data) {
    OLED_Clear();

    // --- 标题栏 (根据模式显示不同文字) ---
    if (data->mode == 1)      OLED_ShowString(36, 2, "AP Mode", FONT_MID);
    else if (data->mode == 2) OLED_ShowString(32, 2, "STA Mode", FONT_MID);
    else                      OLED_ShowString(36, 2, "No WiFi", FONT_MID);

    // --- 内容区 ---
    // SSID
    OLED_ShowString(2, 22, "ID:", FONT_SMALL);
    OLED_DrawRectangle(20, 20, 106, 12, 0); // 框
    OLED_ShowString(24, 22, (char*)data->ssid, FONT_SMALL);

    // Password
    OLED_ShowString(2, 38, "PW:", FONT_SMALL);
    OLED_DrawRectangle(20, 36, 106, 12, 0); // 框
    OLED_ShowString(24, 38, (char*)data->pwd, FONT_SMALL);

    // --- 底部 IP ---
    OLED_DrawLine(0, 52, 127, 52);
    char buf[32];
    if(data->ip) sprintf(buf, "IP:%s %d", data->ip, data->port);
    else         sprintf(buf, "IP: ......");
    OLED_ShowString(2, 54, buf, FONT_SMALL);

    OLED_Update();
}
