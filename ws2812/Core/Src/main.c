/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ws2812.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define LED_COUNT     4
#define RESET_PULSES  80

static uint8_t  led_color[LED_COUNT * 3];
static uint16_t led_pwm[24 * LED_COUNT + RESET_PULSES];

static WS2812_S ws2812;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ws2812_test_all(void);
static void ws2812_test_chase(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  全部 4 颗 LED 同时切换: 红→绿→蓝→白→灭
 */
static void ws2812_test_all(void)
{
    ws2812.fill(&ws2812, 255, 0, 0);
    ws2812.send(&ws2812);
    ws2812.wait_done(&ws2812);
    HAL_Delay(300);

    ws2812.fill(&ws2812, 0, 255, 0);
    ws2812.send(&ws2812);
    ws2812.wait_done(&ws2812);
    HAL_Delay(300);

    ws2812.fill(&ws2812, 0, 0, 255);
    ws2812.send(&ws2812);
    ws2812.wait_done(&ws2812);
    HAL_Delay(300);

    ws2812.fill(&ws2812, 255, 255, 255);
    ws2812.send(&ws2812);
    ws2812.wait_done(&ws2812);
    HAL_Delay(300);

    ws2812.clear(&ws2812);
    ws2812.send(&ws2812);
    ws2812.wait_done(&ws2812);
    HAL_Delay(300);
}

/**
 * @brief  跑马灯: 每颗 LED 依次闪红→绿→蓝→白
 */
static void ws2812_test_chase(void)
{
    for (uint16_t i = 0; i < LED_COUNT; i++)
    {
        ws2812.clear(&ws2812);
        ws2812.set_led(&ws2812, i, 255, 0, 0);
        ws2812.send(&ws2812);
        ws2812.wait_done(&ws2812);
        HAL_Delay(100);

        ws2812.clear(&ws2812);
        ws2812.set_led(&ws2812, i, 0, 255, 0);
        ws2812.send(&ws2812);
        ws2812.wait_done(&ws2812);
        HAL_Delay(100);

        ws2812.clear(&ws2812);
        ws2812.set_led(&ws2812, i, 0, 0, 255);
        ws2812.send(&ws2812);
        ws2812.wait_done(&ws2812);
        HAL_Delay(100);

        ws2812.clear(&ws2812);
        ws2812.set_led(&ws2812, i, 255, 255, 255);
        ws2812.send(&ws2812);
        ws2812.wait_done(&ws2812);
        HAL_Delay(100);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* ---- 创建 WS2812 驱动对象 ---- */
  WS2812_CFG_S cfg = {
      .htim          = &htim2,
      .timer_channel = TIM_CHANNEL_1,
      .led_count     = LED_COUNT,
      .color_buf     = led_color,
      .pwm_buf       = led_pwm,
      .pwm_buf_size  = sizeof(led_pwm) / sizeof(uint16_t),
  };
  WS2812_Create(&ws2812, &cfg);

  /* ---- 启动验证: 4 颗同时亮绿色 2 秒 ---- */
  ws2812.fill(&ws2812, 0, 255, 0);
  ws2812.send(&ws2812);
  ws2812.wait_done(&ws2812);
  HAL_Delay(2000);

  /* ---- 逐颗不同颜色: 红/绿/蓝/白 2 秒 ---- */
  ws2812.clear(&ws2812);
  ws2812.set_led(&ws2812, 0, 255, 0,   0);   /* LED0 = 红   */
  ws2812.set_led(&ws2812, 1, 0,   255, 0);   /* LED1 = 绿   */
  ws2812.set_led(&ws2812, 2, 0,   0, 255);   /* LED2 = 蓝   */
  ws2812.set_led(&ws2812, 3, 255, 255, 255); /* LED3 = 白   */
  ws2812.send(&ws2812);
  ws2812.wait_done(&ws2812);
  HAL_Delay(2000);

  /* ---- 半亮度验证 2 秒 ---- */
  ws2812.set_brightness(&ws2812, 128);
  ws2812.send(&ws2812);
  ws2812.wait_done(&ws2812);
  HAL_Delay(2000);
  ws2812.set_brightness(&ws2812, 255);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      ws2812_test_chase();
      HAL_Delay(300);
      ws2812_test_all();
      HAL_Delay(300);
      /* USER CODE END WHILE */

      /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
