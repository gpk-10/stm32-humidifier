#include "ESP8266.h"
#include "sys_config.h" // 引入外部配置结构体以便配网时写入

/* ================= 引入外部调试工具 ================= */
extern UART_HandleTypeDef huart1;
extern void UART_printf(UART_HandleTypeDef *huart, char *format, ...);

/* ================= 调试打印开关 ================= */
#define WIFI_DEBUG_EN  1   // 【核心开关】设为 1 开启底层WiFi打印，设为 0 关闭，节约Flash空间

#if WIFI_DEBUG_EN
    #define WIFI_LOG(fmt, ...)  UART_printf(&huart1, fmt, ##__VA_ARGS__)
#else
    #define WIFI_LOG(fmt, ...)  // 宏替换为空，编译器自动优化剔除
#endif
/* ================================================ */

/* ================= 私有函数声明 ================= */
static void ESP_ClearBuffer(ESP8266_HandleTypeDef* h_esp);
static bool ESP_SendCmd(ESP8266_HandleTypeDef* h_esp, const char* cmd, const char* expect_ack, uint32_t timeout);
static bool ESP_SetupSTAMode(ESP8266_HandleTypeDef* h_esp);
 
// 【新增】引入外部本地背景任务，用于防止网络阻塞导致系统瘫痪
extern void App_Background_Yield(void);
// 【新增】引入外部打断标志
extern volatile bool g_cancel_smartconfig;
/* ================= 接口函数实现 ================= */

// 初始化并绑定串口
void ESP_Init(ESP8266_HandleTypeDef* h_esp, UART_HandleTypeDef* huart, char *ssid, char *pwd) {
    h_esp->huart = huart;
    h_esp->rx_len = 0;
    h_esp->state = ESP_STATE_RESET; 
    h_esp->is_initialized = false;
    h_esp->last_tick = 0;
    strcpy(h_esp->ssid, ssid);
    strcpy(h_esp->pwd, pwd);
    strcpy(h_esp->local_ip, "0.0.0.0"); // 默认未获取到IP时显示0.0.0.0
    ESP_ClearBuffer(h_esp);
}
 
// 串口中断回调：将收到的字节一个个塞入缓冲区
void ESP_RxCallback(ESP8266_HandleTypeDef* h_esp, uint8_t received_byte) {
    if (h_esp->rx_len < ESP_RX_BUFFER_SIZE - 1) {
        h_esp->rx_buffer[h_esp->rx_len++] = received_byte;
        h_esp->rx_buffer[h_esp->rx_len] = '\0'; // 始终保持字符串结尾，方便使用strstr函数查找
    } else {
        h_esp->rx_len = 0; // 缓冲区满，执行清空防止数组越界引发硬错误 (HardFault)
    }
}

// 状态机主循环
void ESP_Loop(ESP8266_HandleTypeDef* h_esp) {
    switch (h_esp->state) {
        case ESP_STATE_RESET:
            h_esp->state = ESP_STATE_CONFIG_STA;
            break;

        case ESP_STATE_CONFIG_STA:
            if (ESP_SetupSTAMode(h_esp)) {
                h_esp->state = ESP_STATE_RUNNING;
                h_esp->is_initialized = true;
                WIFI_LOG("\r\n[WIFI INIT SUCCESS] Connect to: %s, IP: %s\r\n\r\n", h_esp->ssid, h_esp->local_ip);
            } else {
                h_esp->state = ESP_STATE_ERROR;
                h_esp->last_tick = HAL_GetTick(); // 记录失败时间戳
                WIFI_LOG("\r\n[WIFI INIT FAILED] Retrying in 5s...\r\n\r\n");
            }
            break;

        case ESP_STATE_RUNNING:
            // 正常运行，静静等待接收数据
            break;

        case ESP_STATE_ERROR:
            // 非阻塞延时：等待 5 秒后尝试重新连接路由器
            if (HAL_GetTick() - h_esp->last_tick > 5000) {
                h_esp->state = ESP_STATE_RESET; 
                ESP_ClearBuffer(h_esp);
            }
            break;
    }
}

// 提取 TCP 接收到的数据
bool ESP_GetMessage(ESP8266_HandleTypeDef* h_esp, char* msg_buf, uint16_t max_len) {
    if (h_esp->state != ESP_STATE_RUNNING) return false;
    if (h_esp->rx_len == 0) return false;

    // 1. 查找 ESP8266 特有的网络数据包头 "+IPD"
    char* p_start = strstr((char*)h_esp->rx_buffer, "+IPD");
    if (p_start == NULL) return false;

    // 2. 查找数据分隔符冒号 ":" (格式如 +IPD,0,5:HELLO)
    char* p_colon = strchr(p_start, ':');
    if (p_colon == NULL) return false; // 没找到冒号，说明数据包还没收完
    
    // 3. 提取数据长度 (冒号前面的数字)
    char* p_len_str = NULL;
    char* p_temp = p_start;
    
    while (p_temp < p_colon) {
        char* p_comma = strchr(p_temp, ',');
        if (p_comma != NULL && p_comma < p_colon) {
            p_len_str = p_comma + 1; 
            p_temp = p_comma + 1;    
        } else {
            break; 
        }
    }

    if (p_len_str == NULL) return false; 

    int data_len = atoi(p_len_str); // 将字符串转换为整数长度
    if (data_len <= 0) {
        ESP_ClearBuffer(h_esp); // 长度异常，清空缓存防止死循环
        return false;
    }

    // 4. 防“半包”机制：检查实际收到的数据长度是否达到了标称长度
    char* p_data = p_colon + 1;
    char* p_buffer_end = (char*)h_esp->rx_buffer + h_esp->rx_len;
    
    if ((p_data + data_len) > p_buffer_end) {
        return false; // 数据包不完整，退出函数，等待下一次中断继续接收
    }

    // 5. 数据包完整，安全拷贝到用户的 buffer 中
    uint16_t copy_len = (data_len >= max_len) ? (max_len - 1) : data_len;
    memcpy(msg_buf, p_data, copy_len);
    msg_buf[copy_len] = '\0'; // 补齐字符串结束符
    ESP_ClearBuffer(h_esp);

    return true;
}

/* ================= 私有/底层函数实现 ================= */

// 发送 AT 指令并阻塞等待预期回应
static bool ESP_SendCmd(ESP8266_HandleTypeDef* h_esp, const char* cmd, const char* expect_ack, uint32_t timeout) {
    ESP_ClearBuffer(h_esp); 
    WIFI_LOG("\r\n>>> [SEND]: %s", cmd); 

    HAL_UART_Transmit(h_esp->huart, (uint8_t*)cmd, strlen(cmd), 100);
    
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout) {
        // ==========================================
        // 【核心修复3】：只要检测到长按，立刻“撕裂”死等，1毫秒都不多等！
        // ==========================================
        extern volatile bool g_flag_start_smartconfig;
        if (g_flag_start_smartconfig) {
            WIFI_LOG("\r\n[Interrupt] Abort AT Command!\r\n");
            return false; 
        }
        
        if (strstr((char*)h_esp->rx_buffer, expect_ack) != NULL) {
            App_Background_Yield(); // 退出前刷新一下状态
            HAL_Delay(10); // 稍微延时，等待缓冲区接收完剩余的 \r\n 字符
            WIFI_LOG("<<< [RECV]:\r\n%s", h_esp->rx_buffer); 
            return true; 
        }
        App_Background_Yield(); // 退出前刷新一下状态
        HAL_Delay(10); // 喂狗，让出 CPU 执行权
    }
    WIFI_LOG("!!! [TIMEOUT] Wait for '%s'. RX Buf:\r\n%s\r\n", expect_ack, h_esp->rx_buffer);
    return false; 
}

// 初始化 STA 模式及动态 IP 提取
static bool ESP_SetupSTAMode(ESP8266_HandleTypeDef* h_esp) {
    char temp_cmd[128];
    WIFI_LOG("--- Start WiFi STA Config ---\r\n");

    // 1. 发送探针指令，确认模块串口通信正常
    bool alive = false;
    for(int i=0; i<5; i++) {
        if(ESP_SendCmd(h_esp, "AT\r\n", "OK", 500)) {
            alive = true; break;
        }
        HAL_Delay(100);
    }
    if(!alive) return false; 

    // 2. 设为客户端模式 (Station)
    if(!ESP_SendCmd(h_esp, "AT+CWMODE=1\r\n", "OK", 1000)) return false;

    // 3. 发送账号密码连接路由器
    sprintf(temp_cmd, "AT+CWJAP=\"%s\",\"%s\"\r\n", h_esp->ssid, h_esp->pwd);
    if(!ESP_SendCmd(h_esp, temp_cmd, "OK", 15000)) return false;

    // 4. 获取动态分配的 IP 地址 (用于OLED显示和手机端连接)
    if(ESP_SendCmd(h_esp, "AT+CIFSR\r\n", "OK", 2000)) {
        char* ip_start = strstr((char*)h_esp->rx_buffer, "+CIFSR:STAIP,\"");
        if(ip_start) {
            ip_start += 14; 
            char* ip_end = strchr(ip_start, '\"'); 
            if(ip_end && (ip_end - ip_start < 16)) {
                strncpy(h_esp->local_ip, ip_start, ip_end - ip_start);
                h_esp->local_ip[ip_end - ip_start] = '\0';
            }
        }
    }

    // 5. 开启多连接及本地 TCP 服务器
    if(!ESP_SendCmd(h_esp, "AT+CIPMUX=1\r\n", "OK", 1000)) return false;
    sprintf(temp_cmd, "AT+CIPSERVER=1,%d\r\n", ESP_SERVER_PORT);
    if(!ESP_SendCmd(h_esp, temp_cmd, "OK", 2000)) return false;

    return true;
}

static void ESP_ClearBuffer(ESP8266_HandleTypeDef* h_esp) {
    memset(h_esp->rx_buffer, 0, ESP_RX_BUFFER_SIZE);
    h_esp->rx_len = 0;
}

/* ================= 智能一键配网 (SmartConfig) ================= */

/**
 * @brief 运行 SmartConfig 智能配网 (包含手机端成功回执确认)
 * @return true: 配网成功并连接上路由器; false: 超时或失败
 */
bool ESP_RunSmartConfig(ESP8266_HandleTypeDef* h_esp) {
    WIFI_LOG("\r\n--- Enter SmartConfig Mode ---\r\n");

    // 设为 STA 模式并开启混合配网协议监听
    if(!ESP_SendCmd(h_esp, "AT\r\n", "OK", 1000)) return false;
    if(!ESP_SendCmd(h_esp, "AT+CWMODE=1\r\n", "OK", 1000)) return false;
    if(!ESP_SendCmd(h_esp, "AT+CWSTARTSMART=3\r\n", "OK", 2000)) return false;

    WIFI_LOG("Waiting for Phone to send WiFi info (60s timeout)...\r\n");

    uint32_t start_time = HAL_GetTick();
    ESP_ClearBuffer(h_esp);
    
    // ==========================================
    // 【核心修复2】：在开始 60 秒监听前，强制清除打断标志
    // ==========================================
    extern volatile bool g_cancel_smartconfig;
    g_cancel_smartconfig = false;

    // 60秒超时循环监听
    while ((HAL_GetTick() - start_time) < 60000) {
        App_Background_Yield(); // 维持本地硬件任务运行
        // 【新增】检测到用户按键打断，立刻退出
        if (g_cancel_smartconfig) {
            WIFI_LOG("\r\n[SmartConfig] Canceled by User!\r\n");
            ESP_SendCmd(h_esp, "AT+CWSTOPSMART\r\n", "OK", 1000);
            return false; // 返回 false，app.c 会自动让它退回主页
        }
        
        // 捕获到解析成功的关键字
        if (strstr((char*)h_esp->rx_buffer, "Smart get wifi info") != NULL) {
            HAL_Delay(500); // 延时等待模块吐出具体的账号密码字符

            char* p_ssid = strstr((char*)h_esp->rx_buffer, "ssid:");
            char* p_pwd = strstr((char*)h_esp->rx_buffer, "password:");

            if (p_ssid && p_pwd) {
                p_ssid += 5; 
                p_pwd += 9;  

                // 寻找 \r\n 定位字符串结束位置
                char* p_ssid_end = strpbrk(p_ssid, "\r\n");
                char* p_pwd_end = strpbrk(p_pwd, "\r\n");

                if (p_ssid_end && p_pwd_end) {
                    // 调用外部全局配置变量
                    extern System_Config_t sys_cfg; 
                    memset(sys_cfg.wifi_ssid, 0, sizeof(sys_cfg.wifi_ssid));
                    memset(sys_cfg.wifi_pwd, 0, sizeof(sys_cfg.wifi_pwd));

                    // 提取密码到内存
                    strncpy(sys_cfg.wifi_ssid, p_ssid, p_ssid_end - p_ssid);
                    strncpy(sys_cfg.wifi_pwd, p_pwd, p_pwd_end - p_pwd);

                    WIFI_LOG("\r\n[SmartConfig] Got Info! SSID: %s. Waiting to ACK phone...\r\n", sys_cfg.wifi_ssid);

                    // ==========================================
                    // 等待 UDP 回执发送完毕，确保手机 APP 显示配网成功
                    // ==========================================
                    uint32_t ack_start = HAL_GetTick();
                    bool ack_success = false;
                    
                    // 给模块15秒时间连接路由器并通知手机
                    while ((HAL_GetTick() - ack_start) < 15000) {
                        if (strstr((char*)h_esp->rx_buffer, "smartconfig connected wifi") != NULL) {
                            WIFI_LOG("\r\n[SmartConfig] Phone ACKed successfully!\r\n");
                            ack_success = true;
                            break; 
                        }
                        HAL_Delay(10); 
                    }
                    
                    if (!ack_success) {
                        WIFI_LOG("\r\n[SmartConfig] Warning: Phone ACK timeout, but credentials saved.\r\n");
                    }

                    // 停止配网监听任务，释放 ESP8266 内存
                    ESP_SendCmd(h_esp, "AT+CWSTOPSMART\r\n", "OK", 1000);
                    return true;
                }
            }
        }
        HAL_Delay(10); 
    }

    WIFI_LOG("\r\n[SmartConfig Timeout!]\r\n");
    ESP_SendCmd(h_esp, "AT+CWSTOPSMART\r\n", "OK", 1000);
    return false;
}
