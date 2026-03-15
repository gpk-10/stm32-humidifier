#include "board_i2c_port.h"
#include "main.h" // 包含 STM32 HAL 库

/* === 引脚配置区 (根据 CubeMX 修改) === */
#define SCL_PORT GPIOB
#define SCL_PIN  GPIO_PIN_10
#define SDA_PORT GPIOB
#define SDA_PIN  GPIO_PIN_11

/* 私有操作函数 */
static void AHT20_SCL_Set(uint8_t val) {
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, (GPIO_PinState)val);
}

static void AHT20_SDA_Set(uint8_t val) {
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, (GPIO_PinState)val);
}

static uint8_t AHT20_SDA_Read(void) {
    return (uint8_t)HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN);
}

static void Sim_Delay(void) {
    volatile int i = 30; // 延时调节
    while(i--);
}

/* 实例化对象 */
SoftI2C_t bus_aht20;

void Board_I2C_Init(void) {
    // 绑定函数指针
    bus_aht20.Set_SCL  = AHT20_SCL_Set;
    bus_aht20.Set_SDA  = AHT20_SDA_Set;
    bus_aht20.Read_SDA = AHT20_SDA_Read;
    bus_aht20.Delay    = Sim_Delay;
    
    // 总线空闲状态
    bus_aht20.Set_SCL(1);
    bus_aht20.Set_SDA(1);
}
