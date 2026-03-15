# CLAUDE.md

本文档为 Claude Code (claude.ai/code) 提供与本仓库代码协作时的指导说明。

## 项目概述

这是一个基于 STM32 的智能加湿器嵌入式项目,具备 WiFi 连接和环境检测功能。项目使用 STM32CubeMX 进行配置,MDK-ARM (Keil) 工具链进行构建。

**MCU:** STM32F103C8T6 (Blue Pill)
**时钟:** 72MHz (HSE 8MHz, PLL 9倍频)
**使用外设:** USART1, USART2, TIM2, I2C(软件模拟), GPIO

## 构建系统

本项目使用 **Embedded IDE (EIDE)** VS Code 插件配合 Keil ARM 编译器。

### 构建命令(通过 VS Code 任务)

```bash
# 构建项目
cmd: build

# 重新构建(清理+构建)
cmd: rebuild

# 清理构建产物
cmd: clean

# 下载到设备
cmd: flash

# 构建并下载
cmd: build and flash
```

或者使用 Keil MDK IDE 打开 `MDK-ARM/demo01.uvprojx` 进行构建。

### 输出文件

- 构建目录: `MDK-ARM/demo01/`
- 可执行文件: `demo01/demo01.axf`
- HEX 文件: `demo01/demo01.hex`
- MAP 文件: `demo01/demo01.map`

## 代码架构

### 目录结构

- **Core/由 CubeMX 生成**
  - `Core/Src/` - CubeMX 生成的 HAL 初始化代码
    - `main.c` - 入口,调用 App_Init() 和 App_Task()
    - `tim.c` - TIM2 配置(1ms 中断)
    - `usart.c` - USART1 和 USART2 配置
    - `gpio.c` - GPIO 初始化
  - `Core/Inc/` - 对应头文件

- **App/应用层**
  - `App/app.c` - 主应用逻辑,初始化所有硬件模块
  - `App/app.h` - 应用 API(App_Init 和 App_Task)
  - `App/ui.c` - UI 状态机和 OLED 显示逻辑
  - `App/ui.h` - UI 模块接口

- **Hardware/驱动层**
  - `Hardware/` - 所有自定义硬件驱动
  - 驱动命名: `<设备>.c` 和 `<设备>.h`(如 `ESP8266.c`, `OLED.c`)

- **STM32Cube HAL**
  - `Drivers/STM32F1xx_HAL_Driver/` - STM32 HAL 库
  - `Drivers/CMSIS/` - CMSIS 核心头文件

### 关键设计模式

1. **基于句柄的架构**:每个硬件模块都有句柄结构(如 `ESP8266_HandleTypeDef`, `OLED_HandleTypeDef`)包含配置和状态。

2. **前缀规范**:全局变量使用 `g_` 前缀(如 `g_hEsp`, `g_he30_device1`)。

3. **调试日志**:每个模块可通过类似 `APP_DEBUG_EN` 的宏启用/禁用调试打印。调试输出发送到 USART1。

4. **初始化流程**:
   ```
   main() → HAL_Init() → SystemClock_Config() → MX_*_Init() → App_Init() → while(1) → App_Task()
   ```

5. **基于定时器的轮询**:TIM2 产生 1ms 中断。模块使用 `HAL_GetTick()` 实现非阻塞延时,而不是 `HAL_Delay()`。

## 硬件模块

设备包含以下模块(全部由 `app.c` 中的 `App_Init()` 初始化):

- **ESP8266** - WiFi 连接(USART2, AT 指令)
- **OLED** - 0.96" I2C OLED 显示屏(软件模拟 I2C, PB6/PB7)
- **AHT20** - 温湿度传感器(软件模拟 I2C)
- **HE30** - 超声波雾化器(5 个通道:PA15, PB3, PB4, PB5, PB9)
- **RGB LED** - 状态指示灯(PB12, PB13, PB14)
- **Buzzer** - 音频反馈(PB0)
- **Buttons** - 用户输入(5 个按键:PA4, PA5, PA6, PA7, PB1)
- **Water Sensor** - 水位检测(PA11, PA12)

## 引脚分配表

| 引脚 | 标签 | 功能 |
|-----|------|------|
| PA9 | - | USART1_TX(调试串口) |
| PA10 | - | USART1_RX(调试串口) |
| PA2 | - | USART2_TX(ESP8266) |
| PA3 | - | USART2_RX(ESP8266) |
| PA4 | KEY_1 | 按键 1 |
| PA5 | KEY_2 | 按键 2 |
| PA6 | KEY_3 | 按键 3 |
| PA7 | KEY_4 | 按键 4 |
| PA11 | water_low | 低水位传感器 |
| PA12 | water_high | 高水位传感器 |
| PA15 | HE30_1 | 雾化器通道 1 |
| PB0 | BEEP | 蜂鸣器 |
| PB1 | Button2 | 按键 5 |
| PB3 | HE30_2 | 雾化器通道 2 |
| PB4 | HE30_3 | 雾化器通道 3 |
| PB5 | HE30_4 | 雾化器通道 4 |
| PB6 | OLED_SCL | OLED SCL(软件模拟 I2C) |
| PB7 | OLED_SDA | OLED SDA(软件模拟 I2C) |
| PB9 | HE_30 | 雾化器通道 5 |
| PB10 | AHT20_SCL | 温湿度传感器 SCL(软件模拟 I2C) |
| PB11 | AHT20_SDA | 温湿度传感器 SDA(软件模拟 I2C) |
| PB12 | LED_R | RGB 红色 |
| PB13 | LED_G | RGB 绿色 |
| PB14 | LED_B | RGB 蓝色 |

## 开发指南

### 添加新硬件模块

1. 在 `Hardware/` 目录创建 `<ModuleName>.c` 和 `<ModuleName>.h`
2. 定义句柄结构 `<ModuleName>_HandleTypeDef`
3. 实现 `ModuleName_Init()` 函数
4. 在 `app.c` 的 `App_Init()` 中调用初始化
5. 通过 `#define MODULE_DEBUG_EN` 添加调试打印控制

### I2C 使用说明

本项目使用**软件模拟 I2C**(非硬件 I2C):
- OLED: PB6(SCL), PB7(SDA) - 使用 `my_i2c.c`
- AHT20: PB10(SCL), PB11(SDA) - 使用 `board_i2c_port.c`

直接使用 HAL GPIO 函数操作 SDA/SCL 线。

### USART 使用说明

- USART1: 调试控制台(连接 USB-TTL 转换器)
- USART2: ESP8266 WiFi 模块
- 使用自定义函数 `UART_printf()` 进行格式化输出

### 内存限制

Flash: 64KB (0x8000000-0x8010000)
RAM: 20KB (0x20000000-0x20005000)

项目使用最小堆(0x200)和栈(0x400)。尽量避免动态内存分配。

### GPIO 配置

所有 GPIO 初始化由 CubeMX 在 `Core/Src/gpio.c` 中自动生成。修改引脚:
1. 在 STM32CubeMX 中编辑 `demo01.ioc`
2. 重新生成代码(可能覆盖 `/* USER CODE */` 块外的手动修改!)
3. 根据需要更新 `App_Init()`

## 重要注意事项

- **切勿提交自动生成的文件**: `MDK-ARM/demo01/`(构建输出)、`Drivers/` 和大部分 `Core/` 都是自动生成的
- **重新生成前做好备份** - 使用 CubeMX 重新生成前
- **通过 USART1 调试**: 连接 USB-TTL 转换器到 PA9(TX)/PA10(RX),波特率 115200
- **TIM2 中断优先级**: 10(高于系统节拍 15)
- **主循环时序**: `App_Task()` 基于 `uwTick` 变量每 1ms 运行一次
- **EIDE 配置**: `MDK-ARM/.eide/eide.yml` 包含构建设置
