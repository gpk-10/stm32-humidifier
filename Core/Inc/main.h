/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_1_Pin GPIO_PIN_4
#define KEY_1_GPIO_Port GPIOA
#define KEY_2_Pin GPIO_PIN_5
#define KEY_2_GPIO_Port GPIOA
#define KEY_3_Pin GPIO_PIN_6
#define KEY_3_GPIO_Port GPIOA
#define KEY_4_Pin GPIO_PIN_7
#define KEY_4_GPIO_Port GPIOA
#define BEEP_Pin GPIO_PIN_0
#define BEEP_GPIO_Port GPIOB
#define Button2_Pin GPIO_PIN_1
#define Button2_GPIO_Port GPIOB
#define AHT20_SCL_Pin GPIO_PIN_10
#define AHT20_SCL_GPIO_Port GPIOB
#define AHT20_SDA_Pin GPIO_PIN_11
#define AHT20_SDA_GPIO_Port GPIOB
#define LED_R_Pin GPIO_PIN_12
#define LED_R_GPIO_Port GPIOB
#define LED_G_Pin GPIO_PIN_13
#define LED_G_GPIO_Port GPIOB
#define LED_B_Pin GPIO_PIN_14
#define LED_B_GPIO_Port GPIOB
#define water_low_Pin GPIO_PIN_11
#define water_low_GPIO_Port GPIOA
#define water_high_Pin GPIO_PIN_12
#define water_high_GPIO_Port GPIOA
#define HE30_1_Pin GPIO_PIN_15
#define HE30_1_GPIO_Port GPIOA
#define HE30_2_Pin GPIO_PIN_3
#define HE30_2_GPIO_Port GPIOB
#define HE30_3_Pin GPIO_PIN_4
#define HE30_3_GPIO_Port GPIOB
#define HE30_4_Pin GPIO_PIN_5
#define HE30_4_GPIO_Port GPIOB
#define OLED_SCL_Pin GPIO_PIN_6
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_7
#define OLED_SDA_GPIO_Port GPIOB
#define HE_30_Pin GPIO_PIN_9
#define HE_30_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
