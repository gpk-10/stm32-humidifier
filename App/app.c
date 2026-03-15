#include "app.h"

/* ================= 1. 头文件引用 ================= */
/* 标准库 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h> 

/* 驱动库 */
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* 模块驱动 */
#include "ui.h"       
#include "sys_config.h"
#include "OLED.h"
#include "ESP8266.h"
#include "button.h"
#include "HE_30.h"
#include "water.h"
#include "aht20.h"
#include "rgb.h"
#include "buzzer.h"

/* ================= 调试打印开关 ================= */
#define APP_DEBUG_EN  1    // 【核心开关】设为 1 开启应用层打印，设为 0 关闭

#if APP_DEBUG_EN
    #define APP_LOG(fmt, ...)   UART_printf(&huart1, fmt, ##__VA_ARGS__)
#else
    #define APP_LOG(fmt, ...)   // 宏替换为空，自动剔除
#endif
/* ================================================ */

/* ================= 2. 宏定义与全局变量 ================= */
#define FLASH_SIZE_REG_ADDR  0x1FFFF7E0
#define WIFI_SEND_INTERVAL   1000 // WiFi状态上报间隔(ms)
#define SENSOR_READ_INTERVAL 1000 // 温湿度传感器读取间隔(ms)

/* --- 硬件句柄全局变量 (遵循 g_ 前缀规范) --- */
ESP8266_HandleTypeDef g_hEsp;         // WiFi 模块句柄
HE30_HandleTypeDef    g_he30_device1; // 雾化器 1 句柄
HE30_HandleTypeDef    g_he30_device2; // 雾化器 2 句柄
Water_HandleTypeDef   g_hwater;       // 水位传感器句柄
RGB_HandleTypeDef     g_hrgb;         // RGB 状态灯句柄
Buzzer_HandleTypeDef  g_hbuzz;        // 蜂鸣器句柄

/* --- 页面状态枚举与变量 --- */
typedef enum { PAGE_HOME, PAGE_SETTING, PAGE_WIFI, PAGE_MAX } Page_t;
static Page_t g_cur_page = PAGE_HOME; // 当前 OLED 显示页面

/* --- 核心业务控制全局变量 --- */
volatile static bool    g_is_auto_mode = true;         // 自动/手动模式切换标志
volatile static float   g_humidity     = 0.0f;         // 当前环境湿度
volatile static float   g_temp         = 0.0f;         // 当前环境温度
volatile static uint8_t g_target_hum   = 60;           // 用户设定的目标湿度 
volatile bool    g_flag_start_smartconfig = false; // 按键触发配网的异步标志位

// 【新增】用于随时打断配网的标志位
volatile bool g_is_smartconfig_running = false; 
volatile bool g_cancel_smartconfig = false;

/* Flash 延时写入保护机制变量 */
static uint32_t g_flash_save_tick  = 0;
static bool     g_need_save_config = false;

/* 经过软件防抖后的真实水位状态 */
static Water_Status_t g_real_water_status = WATER_MID; 

/* --- 硬件中断驱动对象 --- */
static Button_t g_btn_mode;   // 模式切换键
static Button_t g_btn_set;    // 设置/开关键
static Button_t g_btn_up;     // 数值加键
static Button_t g_btn_down;   // 数值减键
static uint8_t  g_uart_rx_byte; // 串口2异步接收缓冲字节

/* ================= 3. 私有函数声明 ================= */
#if APP_DEBUG_EN
static uint16_t Get_Flash_Size(void);
#endif
static void App_Process_WiFi_Msg(char *msg_buf);
static void App_Auto_Control_Loop(void);
static void App_Send_Status_To_WiFi(void);
static void App_Read_Sensor_Loop(void);
static void App_Status_Feedback_Loop(void); // 新增：统管声光反馈与水位防抖

// 【新增】公开的本地背景任务集合，供外部耗时循环调用
void App_Background_Yield(void);
/* ================= 4. 辅助工具函数 ================= */

/**
 * @brief 自定义串口打印函数 (格式化输出到 UART1)
 */
void UART_printf(UART_HandleTypeDef *huart, char *format, ...) {
    char String[256];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    HAL_UART_Transmit(huart, (uint8_t *)String, strlen(String), 100);
}

#if APP_DEBUG_EN
static uint16_t Get_Flash_Size(void) {
    return *(uint16_t*)(FLASH_SIZE_REG_ADDR);
}
#endif

/* ================= 5. 核心业务逻辑函数 ================= */

/**
 * @brief 解析并执行手机 APP/小程序 发来的网络指令
 */
static void App_Process_WiFi_Msg(char *msg_buf) {
    APP_LOG("[WiFi Recv]: %s\r\n", msg_buf);

    if (strstr(msg_buf, "OPEN") != NULL) {
        g_is_auto_mode = false;       // 强制接管，切为手动
        HE30_Start(&g_he30_device1);  
        HE30_Start(&g_he30_device2);  
        Buzzer_Beep(&g_hbuzz, 100);   // 手机控制成功，滴一声反馈
    }
    else if (strstr(msg_buf, "CLOSE") != NULL) {
        g_is_auto_mode = false;       
        HE30_Stop(&g_he30_device1);    
        HE30_Stop(&g_he30_device2);    
        Buzzer_Beep(&g_hbuzz, 100);
    }
    else if (strstr(msg_buf, "AUTO") != NULL) {
        g_is_auto_mode = true;        // 恢复传感器自动控制
        Buzzer_Beep(&g_hbuzz, 100);
    }
    else if (strstr(msg_buf, "MANUAL") != NULL) {
        g_is_auto_mode = false;
        Buzzer_Beep(&g_hbuzz, 100);
    }
    else if (strstr(msg_buf, "SET:") != NULL) {
        char *ptr = strstr(msg_buf, "SET:");
        if (ptr != NULL) {
            int val = atoi(ptr + 4); 
            // 湿度阈值安全检查，并启用延时异步保存保护 Flash
            if (val >= 20 && val <= 90) {
                g_target_hum = (uint8_t)val;
                g_need_save_config = true;
                g_flash_save_tick = HAL_GetTick(); // 刷新计时起点
                Buzzer_Beep(&g_hbuzz, 50);
            }
        }
    }
}

/**
 * @brief 加湿器恒湿算法及缺水防干烧逻辑
 */
static void App_Auto_Control_Loop(void) {
    if (g_is_auto_mode) {
        // 第一优先级：水位安全检查 (使用防抖后的真实水位)
        if (g_real_water_status != WATER_EMPTY) {
            // 加入 2% 的回差 (Hysteresis)，防止湿度在临界值时继电器频繁打火
            if (g_humidity < g_target_hum - 2.0f) {
                if (!HE30_IsRunning(&g_he30_device1)) HE30_Start(&g_he30_device1);
                if (!HE30_IsRunning(&g_he30_device2)) HE30_Start(&g_he30_device2);
            }
            else if (g_humidity >= g_target_hum) {
                if (HE30_IsRunning(&g_he30_device1))  HE30_Stop(&g_he30_device1);
                if (HE30_IsRunning(&g_he30_device2))  HE30_Stop(&g_he30_device2);
            }
        } else {
            // 水位空，生命安全至上，强制切断雾化器
            HE30_Stop(&g_he30_device1);
            HE30_Stop(&g_he30_device2);
        }
    } else {
        // 如果是手动模式且缺水，依然强制关闭防干烧
        if (g_real_water_status == WATER_EMPTY) {
            HE30_Stop(&g_he30_device1);
            HE30_Stop(&g_he30_device2);
        }
    }
}

/**
 * @brief 统管系统声光反馈联动与环境数据防抖
 */
static void App_Status_Feedback_Loop(void) {
    // 1. 水位防抖滤波处理 (防止水面晃动产生假信号)
    static uint8_t s_water_empty_cnt = 0;
    Water_Status_t raw_water = Water_GetStatus(&g_hwater);
    
    if (raw_water == WATER_EMPTY) {
        if (s_water_empty_cnt < 5) s_water_empty_cnt++; 
    } else {
        s_water_empty_cnt = 0;
    }
    // 连续 5 个循环检测到缺水，才判定为真缺水
    g_real_water_status = (s_water_empty_cnt >= 5) ? WATER_EMPTY : raw_water;

    // 2. 缺水边缘检测，触发一次长鸣警报
    static Water_Status_t s_last_water = WATER_MID;
    if (g_real_water_status == WATER_EMPTY && s_last_water != WATER_EMPTY) {
        Buzzer_Beep(&g_hbuzz, 500); // 缺水瞬间长鸣 0.5 秒
    }
    s_last_water = g_real_water_status;

    // 3. RGB 状态指示灯联动逻辑
    if (g_flag_start_smartconfig) {
        RGB_SetColor(&g_hrgb, RGB_COLOR_BLUE); // 正在配网：蓝灯
    } 
    else if (g_hEsp.state == ESP_STATE_RUNNING) {
        // 连网成功后的状态呈现
        if (g_real_water_status == WATER_EMPTY) {
            RGB_SetColor(&g_hrgb, RGB_COLOR_RED); // 缺水：红灯警示
        } else if (HE30_IsRunning(&g_he30_device1) || HE30_IsRunning(&g_he30_device2)) {
            RGB_SetColor(&g_hrgb, RGB_COLOR_GREEN); // 加湿中：绿灯正常
        } else {
            RGB_SetColor(&g_hrgb, RGB_COLOR_WHITE); // 待机：白灯照明
        }
    } 
    else {
        RGB_SetColor(&g_hrgb, RGB_COLOR_YELLOW); // 未连网/断网重连中：黄灯警告
    }
}

/**
 * @brief 定时将设备状态打包发送给手机
 */
static void App_Send_Status_To_WiFi(void) {
    static uint32_t last_send_time = 0;
    
    if (HAL_GetTick() - last_send_time < WIFI_SEND_INTERVAL) return;
    last_send_time = HAL_GetTick();

    int h2o_status = (g_real_water_status != WATER_EMPTY) ? 1 : 0;
    int run_status = (HE30_IsRunning(&g_he30_device1) || HE30_IsRunning(&g_he30_device2)) ? 1 : 0;
    int mode_status = g_is_auto_mode ? 1 : 0;

    char real_data[128];
    sprintf(real_data, "#HUM:%.1f,TEMP:%.1f,H2O:%d,RUN:%d,MODE:%d,SET:%d$\r\n", 
            g_humidity, g_temp, h2o_status, run_status, mode_status, g_target_hum);

    char at_head[64];
    sprintf(at_head, "AT+CIPSEND=0,%d\r\n", strlen(real_data));
    HAL_UART_Transmit(&huart2, (uint8_t*)at_head, strlen(at_head), 100);
    HAL_Delay(20); 
    HAL_UART_Transmit(&huart2, (uint8_t*)real_data, strlen(real_data), 100);
}

/**
 * @brief 定时读取温湿度传感器数据
 */
static void App_Read_Sensor_Loop(void) {
    static uint32_t last_read_time = 0;
    
    if (HAL_GetTick() - last_read_time < SENSOR_READ_INTERVAL) return;
    last_read_time = HAL_GetTick();

    float cur_temp = 0.0f, cur_hum = 0.0f;

    if (AHT20_Read_Values(&cur_temp, &cur_hum) == 0) {
        g_temp = cur_temp;
        g_humidity = cur_hum;
    } else {
        APP_LOG("[Sensor] Read Error!\r\n");
    }
}

/* ================= 6. 中断与按键回调区 ================= */

/**
 * @brief 模式键 (PA4) 回调：切换UI页面 / 长按配网
 */
void Mode_Btn_Callback(void* btn, Button_Event_t event) {
    // 【新增】：静态变量，用来记录上一次触发长按的时间戳
    static uint32_t last_long_press_time = 0;

    if (event == BTN_EVENT_LONG_PRESS) {
        if (g_cur_page == PAGE_WIFI) {
            Buzzer_Beep(&g_hbuzz, 200); 
            g_flag_start_smartconfig = true; 
            last_long_press_time = HAL_GetTick(); // 记录长按触发的瞬间
        }
    }
    else if (event == BTN_EVENT_CLICK) {
        // ==========================================
        // 【核心修复1】：如果距离上次长按还不到 1.5 秒，
        // 说明这绝对是“手指抬起”产生的幽灵误触，直接拦截！
        // ==========================================
        if (HAL_GetTick() - last_long_press_time < 1500) {
            return; 
        }

        Buzzer_Beep(&g_hbuzz, 50); // 正常的按键音反馈
        
        // 配网中的打断逻辑
        if (g_is_smartconfig_running) {
            g_cancel_smartconfig = true; 
            return; 
        }

        // 正常的翻页逻辑
        if (g_cur_page == PAGE_HOME) g_cur_page = PAGE_SETTING;
        else if (g_cur_page == PAGE_SETTING) g_cur_page = PAGE_WIFI;
        else if (g_cur_page == PAGE_WIFI) g_cur_page = PAGE_HOME;
    }
}

/**
 * @brief 设置键 (PA5) 回调：主页开关加湿器 / 切换模式
 */
void Set_Btn_Callback(void* btn, Button_Event_t event) {
    if (g_cur_page == PAGE_HOME) {
        if (event == BTN_EVENT_CLICK) {
            Buzzer_Beep(&g_hbuzz, 50);
            HE30_Toggle(&g_he30_device1); 
            HE30_Toggle(&g_he30_device2); 
            g_is_auto_mode = false; // 手动干预后，自动切换为手动模式
        }
        else if (event == BTN_EVENT_LONG_PRESS) {
            Buzzer_Beep(&g_hbuzz, 100);
            g_is_auto_mode = !g_is_auto_mode; // 长按切换自动/手动模式
        }
    }
    else if (g_cur_page == PAGE_SETTING) {
        if (event == BTN_EVENT_LONG_PRESS) {
            Buzzer_Beep(&g_hbuzz, 100);
            sys_cfg.target_hum = g_target_hum;
            Config_Save();
            g_cur_page = PAGE_HOME; // 保存并返回主页
        }
    }
}

/**
 * @brief 上键 (PA6) 回调：设置页增加湿度
 */
void Up_Btn_Callback(void* btn, Button_Event_t event) {
    if (event == BTN_EVENT_CLICK && g_cur_page == PAGE_SETTING) {
        Buzzer_Beep(&g_hbuzz, 30); // 简短清脆的按键音
        if (g_target_hum <= 85) g_target_hum += 5;
    }
}

/**
 * @brief 下键 (PA7) 回调：设置页减小湿度
 */
void Down_Btn_Callback(void* btn, Button_Event_t event) {
    if (event == BTN_EVENT_CLICK && g_cur_page == PAGE_SETTING) {
        Buzzer_Beep(&g_hbuzz, 30);
        if (g_target_hum >= 25) g_target_hum -= 5;
    }
}

/* 串口 2 异步接收中断回调 (服务于 WiFi) */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        ESP_RxCallback(&g_hEsp, g_uart_rx_byte);
        HAL_UART_Receive_IT(&huart2, &g_uart_rx_byte, 1);
    }
}

/* 定时器 2 溢出中断回调 (服务于按键状态机基准时钟) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM2) {
        Button_Process();
    }
}

/* ================= 7. 初始化与主循环 ================= */

void App_Init(void) {
    APP_LOG("Chip Flash Size: %d KB\n", Get_Flash_Size());
    
    // 1. 加载持久化配置
    Config_Init();
    g_target_hum = sys_cfg.target_hum;

    // 2. 交互外设初始化
    UI_Init();
    UI_UpdateBootProgress(0, "System Start...");
    HAL_Delay(500);

    // 初始化声光外设
    Buzzer_Init(&g_hbuzz, GPIOB, GPIO_PIN_0, 1);
    RGB_Init(&g_hrgb, GPIOB, GPIO_PIN_12, GPIOB, GPIO_PIN_13, GPIOB, GPIO_PIN_14, 1);
    Buzzer_Beep(&g_hbuzz, 100); // 开机滴一声迎宾
    RGB_SetColor(&g_hrgb, RGB_COLOR_BLUE); // 初始化阶段亮蓝灯

    // 初始化驱动硬件
    Water_Init(&g_hwater, GPIOA, GPIO_PIN_11, GPIOA, GPIO_PIN_12, 0);
    HE30_Init(&g_he30_device1, GPIOA, GPIO_PIN_15, 1);
    HE30_Init(&g_he30_device2, GPIOB, GPIO_PIN_3, 1);

    // 注册 4 个按键
    Button_Init(&g_btn_mode, GPIOA, GPIO_PIN_4, 0, Mode_Btn_Callback);
    Button_Init(&g_btn_set,  GPIOA, GPIO_PIN_5, 0, Set_Btn_Callback);
    Button_Init(&g_btn_up,   GPIOA, GPIO_PIN_6, 0, Up_Btn_Callback);
    Button_Init(&g_btn_down, GPIOA, GPIO_PIN_7, 0, Down_Btn_Callback);
    HAL_TIM_Base_Start_IT(&htim2); // 启动按键扫描心跳

    // 检查初始水位
    if (Water_GetStatus(&g_hwater) == WATER_EMPTY) {
         UI_UpdateBootProgress(20, "Water: Empty!");
    } else {
         UI_UpdateBootProgress(20, "Water: OK");
    }
    HAL_Delay(500);

    // 传感器预热
    UI_UpdateBootProgress(40, "Sensor Warmup...");
    AHT20_Init_Device();
    HAL_Delay(500);

    // 3. 网络模块初始化
    HAL_UART_Receive_IT(&huart2, &g_uart_rx_byte, 1);
    ESP_Init(&g_hEsp, &huart2, sys_cfg.wifi_ssid, sys_cfg.wifi_pwd);

    // === 【出厂智能配网分流逻辑】 ===
    if (strlen(sys_cfg.wifi_ssid) == 0) {
        APP_LOG("No WiFi config found! Auto start SmartConfig...\r\n");
        UI_UpdateBootProgress(60, "Need WiFi Config!");
        OLED_Clear();

        g_cur_page = PAGE_WIFI;
        UI_WiFiData_t wifi_dt = {"WAITING...", "WAITING...", "0.0.0.0", ESP_SERVER_PORT, 1};
        UI_DrawWiFi(&wifi_dt);

        RGB_SetColor(&g_hrgb, RGB_COLOR_BLUE); // 强配网蓝灯

        if (ESP_RunSmartConfig(&g_hEsp)) {
            Config_Save(); 
            UI_UpdateBootProgress(100, "Config OK! Reboot");
            Buzzer_Beep(&g_hbuzz, 500); // 成功长鸣
            HAL_Delay(1000);
            HAL_NVIC_SystemReset(); // 软复位应用新网络
        } else {
            g_cur_page = PAGE_HOME; // 超时退回本地模式
        }
    } 
    else {
        UI_UpdateBootProgress(60, "Init WiFi STA...");
        UI_UpdateBootProgress(70, "Connecting...");
        for(int i=0; i<30; i++) {
            if (g_hEsp.state == ESP_STATE_RUNNING) break; 
            uint8_t prog = 70 + (i * 30 / 30);
            UI_UpdateBootProgress(prog, "Waiting WiFi...");
            HAL_Delay(200);
        }
    }

    // 初始化完成
    UI_UpdateBootProgress(100, "Ready!");
    Buzzer_Beep(&g_hbuzz, 100); 
    HAL_Delay(500);
    OLED_Clear();
}

//void App_Task(void) {
//    // 1. 底层驱动异步轮询
//    ESP_Loop(&g_hEsp);
//    Buzzer_Loop(&g_hbuzz); // 释放蜂鸣器阻塞计时

//    // 2. 状态反馈与防抖
//    App_Status_Feedback_Loop();

//    // 3. 传感器数据采集
//    App_Read_Sensor_Loop();

//    // 4. 自动控制逻辑执行
//    App_Auto_Control_Loop();

//    // 5. 局域网上报
//    App_Send_Status_To_WiFi();

//    // 6. 网络下行指令处理
//    char msg_buf[128];
//    if (ESP_GetMessage(&g_hEsp, msg_buf, sizeof(msg_buf))) {
//        App_Process_WiFi_Msg(msg_buf);
//    }

//    // 7. Flash 延时异步保存保护 (收到 APP 频繁设值指令时，静默等 5 秒后再写入)
//    if (g_need_save_config && (HAL_GetTick() - g_flash_save_tick > 5000)) {
//        sys_cfg.target_hum = g_target_hum;
//        Config_Save();
//        g_need_save_config = false;
//        APP_LOG("[System]: Target Humidity Saved to Flash.\r\n");
//    }

//    // ==========================================
//    // 8. 【中断接管区】安全处理物理按键发起的配网请求
//    // ==========================================
//    if (g_flag_start_smartconfig) {
//        g_flag_start_smartconfig = false;

//        g_is_auto_mode = false;
//        HE30_Stop(&g_he30_device1); 
//        HE30_Stop(&g_he30_device2);

//        OLED_Clear();
//        UI_UpdateBootProgress(50, "SmartConfig...");
//        UI_UpdateBootProgress(50, "Use Phone App");

//        if (ESP_RunSmartConfig(&g_hEsp)) {
//            Config_Save();
//            UI_UpdateBootProgress(100, "Success! Rebooting");
//            Buzzer_Beep(&g_hbuzz, 500);
//            HAL_Delay(1000);
//            HAL_NVIC_SystemReset();
//        } else {
//            OLED_Clear();
//            g_cur_page = PAGE_HOME;
//        }
//    }

//    // 9. 状态调试监听
//    static int last_state = -1;
//    if(g_hEsp.state != last_state) {
//        APP_LOG("[System]: WiFi State -> %d\r\n", g_hEsp.state);
//        last_state = g_hEsp.state;
//    }

//    // 10. UI 动态页面刷新路由
//    switch (g_cur_page) {
//        case PAGE_HOME: {
//            UI_HomeData_t home_dt;
//            home_dt.humidity      = g_humidity;
//            home_dt.temperature   = g_temp;
//            home_dt.target_hum    = g_target_hum;
//            home_dt.is_running    = HE30_IsRunning(&g_he30_device1) || HE30_IsRunning(&g_he30_device2);
//            home_dt.is_wifi_connected = (g_hEsp.state == ESP_STATE_RUNNING);
//            home_dt.is_auto_mode  = g_is_auto_mode;
//            home_dt.water_level   = g_real_water_status; // 传入防抖后的安全值
//            UI_DrawHome(&home_dt);
//            break;
//        }

//        case PAGE_SETTING: {
//            UI_SetData_t set_dt;
//            set_dt.target_hum = g_target_hum;
//            UI_DrawSetting(&set_dt);
//            break;
//        }

//        case PAGE_WIFI: {
//            UI_WiFiData_t wifi_dt;
//            wifi_dt.mode = 1; // 1代表局域网模式
//            wifi_dt.ssid = g_hEsp.ssid;
//            wifi_dt.pwd  = g_hEsp.pwd;
//            wifi_dt.ip   = g_hEsp.local_ip;
//            wifi_dt.port = ESP_SERVER_PORT;
//            UI_DrawWiFi(&wifi_dt);
//            break;
//        }
//    }
//}

/**
 * @brief 本地硬件背景任务池 (纯本地运算，绝对不能包含任何 WiFi 发送逻辑)
 * 这个函数专门在 WiFi 模块发生长时间阻塞等待时被调用，保持设备本地不死机。
 */
void App_Background_Yield(void) {
    // 1. 释放蜂鸣器阻塞计时
    Buzzer_Loop(&g_hbuzz); 

    // 2. 状态反馈与防抖
    App_Status_Feedback_Loop();

    // 3. 传感器数据采集
    App_Read_Sensor_Loop();

    // 4. 自动控制逻辑执行 (防干烧生命线)
    App_Auto_Control_Loop();

    // 5. 保持 OLED 动态页面刷新
    // 【核心修复】：如果正在进行 SmartConfig 配网，则挂起常规页面刷新，保护配网提示画面不被覆盖
    if (!g_is_smartconfig_running) {
        switch (g_cur_page) {
            case PAGE_HOME: {
                UI_HomeData_t home_dt;
                home_dt.humidity      = g_humidity;
                home_dt.temperature   = g_temp;
                home_dt.target_hum    = g_target_hum;
                home_dt.is_running    = HE30_IsRunning(&g_he30_device1) || HE30_IsRunning(&g_he30_device2);
                home_dt.is_wifi_connected = (g_hEsp.state == ESP_STATE_RUNNING);
                home_dt.is_auto_mode  = g_is_auto_mode;
                home_dt.water_level   = g_real_water_status; 
                UI_DrawHome(&home_dt);
                break;
            }
            case PAGE_SETTING: {
                UI_SetData_t set_dt;
                set_dt.target_hum = g_target_hum;
                UI_DrawSetting(&set_dt);
                break;
            }
            case PAGE_WIFI: {
                UI_WiFiData_t wifi_dt;
                wifi_dt.mode = 1; 
                wifi_dt.ssid = g_hEsp.ssid;
                wifi_dt.pwd  = g_hEsp.pwd;
                wifi_dt.ip   = g_hEsp.local_ip;
                wifi_dt.port = ESP_SERVER_PORT;
                UI_DrawWiFi(&wifi_dt);
                break;
            }
        }
    }
}

/**
 * @brief 系统总管主循环
 */
void App_Task(void) {
    // 1. 执行本地背景任务
    App_Background_Yield();

    // 2. 底层驱动异步轮询 (如果有阻塞，它内部会自动调用 App_Background_Yield)
    ESP_Loop(&g_hEsp);

    // 3. 局域网上报 (一定要放在后面，防止在等待 AT 响应时冲突)
    if (g_hEsp.state == ESP_STATE_RUNNING) {
        App_Send_Status_To_WiFi();
    }

    // 4. 网络下行指令处理
    char msg_buf[128];
    if (ESP_GetMessage(&g_hEsp, msg_buf, sizeof(msg_buf))) {
        App_Process_WiFi_Msg(msg_buf);
    }

    // 5. Flash 延时异步保存保护
    if (g_need_save_config && (HAL_GetTick() - g_flash_save_tick > 5000)) {
        sys_cfg.target_hum = g_target_hum;
        Config_Save();
        g_need_save_config = false;
        APP_LOG("[System]: Target Humidity Saved to Flash.\r\n");
    }

    // 6. 处理按键发起的配网请求
    if (g_flag_start_smartconfig) {
        g_flag_start_smartconfig = false;

        g_is_auto_mode = false;
        HE30_Stop(&g_he30_device1); 
        HE30_Stop(&g_he30_device2);

        OLED_Clear();
        UI_UpdateBootProgress(50, "SmartConfig...");
        UI_UpdateBootProgress(50, "Use Phone App");

        // 【新增】标记配网开始
        g_is_smartconfig_running = true;
        g_cancel_smartconfig = false;
        
        if (ESP_RunSmartConfig(&g_hEsp)) {
            Config_Save();
            UI_UpdateBootProgress(100, "Success! Rebooting");
            Buzzer_Beep(&g_hbuzz, 500);
            HAL_Delay(1000);
            HAL_NVIC_SystemReset();
        } else {
            OLED_Clear();
            g_cur_page = PAGE_HOME;
        }
        
        // 【新增】配网结束（无论成功、超时还是被打断），清除标志
        g_is_smartconfig_running = false;
    }

    // 7. 状态调试监听
    static int last_state = -1;
    if(g_hEsp.state != last_state) {
        APP_LOG("[System]: WiFi State -> %d\r\n", g_hEsp.state);
        last_state = g_hEsp.state;
    }
}

