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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdio.h>

#include "../../App/app.h"
#include "../../BSP/bsp.h"
#include "../../BSP/bsp_oled_smoke.h"
#include "../../Drivers/drv_adc_internal.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* APP_SMOKE_OLED_TEST=1 -> OLED-only smoke, APP_SMOKE_LED_TEST=1 -> LED-only smoke, both 0 -> normal app */
#define APP_SMOKE_OLED_TEST 1
#define APP_SMOKE_LED_TEST 0

#define OLED_SMOKE_FORCE_PROFILE OLED_PROFILE_SSD1315_PAGE
#define OLED_REFRESH_MS 500u
#define ADC_FALLBACK_VDDA_MV 3300u
#if defined(__GNUC__)
#define APP_MAYBE_UNUSED __attribute__((unused))
#else
#define APP_MAYBE_UNUSED
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef hi2c3;
TIM_HandleTypeDef htim16;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
static volatile oled_smoke_diag_t g_smoke_last_diag;
static volatile uint8_t g_oled_addr7_dbg;
static volatile uint16_t g_oled_addr_hal_dbg;
static volatile uint16_t g_adc_raw_dbg;
static volatile uint32_t g_adc_mv_dbg;
static volatile uint32_t g_adc_vdda_dbg;
static volatile app_err_t g_adc_stat_dbg;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_I2C3_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM2_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
/* USER CODE BEGIN PFP */
static void SmokeLed_Init(void);
static void SmokeLed_Run(void);
static void SmokeLed_BlinkError(uint8_t code);
static void SmokeDiagCapture(void);
static void SmokeOled_DrawInitPage(void);
static void SmokeOled_DrawAdcPage(uint16_t raw, bool raw_valid, uint32_t mv, bool mv_valid,
                                  uint32_t vdda_mv, bool vdda_valid, app_err_t stat);
static void SmokeOled_Run(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void SmokeLed_BlinkError(uint8_t code)
{
  uint8_t i;

  for (;;) {
    for (i = 0u; i < code; i++) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
      HAL_Delay(100);
    }
    HAL_Delay(600);
  }
}

static APP_MAYBE_UNUSED void SmokeLed_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

static APP_MAYBE_UNUSED void SmokeLed_Run(void)
{
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
  HAL_Delay(1000);

  while (1)
  {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
    HAL_Delay(100);
  }
}

static void SmokeDiagCapture(void)
{
  oled_smoke_diag_t tmp;
  oled_smoke_diag_get(&tmp);
  g_smoke_last_diag = tmp;
}

static void SmokeOled_DrawInitPage(void)
{
  oled_smoke_fb_clear(0x00u);
  oled_smoke_draw_text_line(0u, "OLED TXT OK");
  oled_smoke_draw_text_line(1u, "ADC INIT...");
  oled_smoke_draw_text_line(2u, "RAW: ----");
  oled_smoke_draw_text_line(3u, "MV : ----");
}

static void SmokeOled_DrawAdcPage(uint16_t raw, bool raw_valid, uint32_t mv, bool mv_valid,
                                  uint32_t vdda_mv, bool vdda_valid, app_err_t stat)
{
  char line[24];

  oled_smoke_fb_clear(0x00u);
  oled_smoke_draw_text_line(0u, "OLED TXT OK");

  if (raw_valid) {
    (void)snprintf(line, sizeof(line), "RAW: %5u", raw);
  } else {
    (void)snprintf(line, sizeof(line), "RAW: ----");
  }
  oled_smoke_draw_text_line(1u, line);

  if (mv_valid) {
    (void)snprintf(line, sizeof(line), "MV : %lu.%03lu",
                   (unsigned long)(mv / 1000u), (unsigned long)(mv % 1000u));
  } else {
    (void)snprintf(line, sizeof(line), "MV : ----");
  }
  oled_smoke_draw_text_line(2u, line);

  if (vdda_valid) {
    (void)snprintf(line, sizeof(line), "VDDA:%4lu", (unsigned long)vdda_mv);
  } else {
    (void)snprintf(line, sizeof(line), "VDDA:----");
  }
  oled_smoke_draw_text_line(3u, line);

  if (stat == ERR_OK) {
    (void)snprintf(line, sizeof(line), "STAT: OK");
  } else {
    (void)snprintf(line, sizeof(line), "STAT: ERR%d", (int)stat);
  }
  oled_smoke_draw_text_line(4u, line);
}

static APP_MAYBE_UNUSED void SmokeOled_Run(void)
{
  app_err_t adc_init_err;
  app_err_t adc_err;
  uint16_t raw = 0u;
  uint32_t mv = 0u;
  uint32_t vdda_mv = ADC_FALLBACK_VDDA_MV;
  bool raw_valid;
  bool mv_valid;
  bool vdda_valid;

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_Delay(250);
  g_oled_addr7_dbg = OLED_ADDR_7BIT;
  g_oled_addr_hal_dbg = OLED_ADDR_HAL;

  if (!oled_smoke_init((oled_smoke_profile_t)OLED_SMOKE_FORCE_PROFILE, OLED_ADDR_7BIT)) {
    SmokeDiagCapture();
    if (g_smoke_last_diag.stage == OLED_SMOKE_FAIL_INIT) {
      SmokeLed_BlinkError(2u);
    }
    SmokeLed_BlinkError(1u);
  }

  g_oled_addr7_dbg = oled_smoke_get_addr7();
  g_oled_addr_hal_dbg = oled_smoke_get_addr_hal();

  SmokeOled_DrawInitPage();
  if (!oled_smoke_flush_full()) {
    SmokeDiagCapture();
    SmokeLed_BlinkError(3u);
  }
  HAL_Delay(OLED_REFRESH_MS);

  adc_init_err = adc1_init();
  g_adc_stat_dbg = adc_init_err;

  for (;;) {
    raw_valid = false;
    mv_valid = false;
    vdda_valid = false;
    raw = 0u;
    mv = 0u;
    vdda_mv = ADC_FALLBACK_VDDA_MV;

    if (adc_init_err == ERR_OK) {
      adc_err = adc1_read_raw_u16(&raw);
      if (adc_err == ERR_OK) {
        raw_valid = true;
      }

      if (adc_err == ERR_OK) {
        adc_err = adc1_read_mv(&mv);
        if (adc_err == ERR_OK) {
          mv_valid = true;
        }
      }

      if (adc_err == ERR_OK) {
        adc_err = adc1_read_vdda_mv(&vdda_mv);
        if (adc_err == ERR_OK) {
          vdda_valid = true;
        }
      }
    } else {
      adc_err = adc_init_err;
    }

    g_adc_raw_dbg = raw;
    g_adc_mv_dbg = mv;
    g_adc_vdda_dbg = vdda_mv;
    g_adc_stat_dbg = adc_err;

    SmokeOled_DrawAdcPage(raw, raw_valid, mv, mv_valid, vdda_mv, vdda_valid, adc_err);
    if (!oled_smoke_flush_full()) {
      SmokeDiagCapture();
      SmokeLed_BlinkError(3u);
    }

    HAL_Delay(OLED_REFRESH_MS);
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
#if APP_SMOKE_OLED_TEST
  (void)SmokeLed_Run;
  (void)MX_I2C3_Init;
  (void)MX_TIM2_Init;
  (void)MX_TIM16_Init;
  (void)bsp_init;
  (void)app_init;
  MX_I2C2_Init();
  SmokeLed_Init();
  SmokeOled_Run();
#elif APP_SMOKE_LED_TEST
  (void)MX_I2C2_Init;
  (void)MX_I2C3_Init;
  (void)MX_TIM2_Init;
  (void)MX_TIM16_Init;
  SmokeLed_Init();
  SmokeLed_Run();
#else
  MX_I2C2_Init();
  MX_I2C3_Init();
  MX_TIM2_Init();
  MX_TIM16_Init();

  /* USER CODE BEGIN 2 */
  bsp_init();
  app_init();
  /* USER CODE END 2 */
#endif

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if !APP_SMOKE_OLED_TEST && !APP_SMOKE_LED_TEST
    app_poll_button();
    app_measure_tick();
    app_ui_tick();
    app_beep_tick();
#endif
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x20303E5D;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x20303E5D;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 15;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 369;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 185;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim16);
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;
  sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;
  sSlaveConfig.TriggerPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sSlaveConfig.TriggerFilter = 0;
  if (HAL_TIM_SlaveConfigSynchro(&htim2, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_INDIRECTTI;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, VOLTAGE_MODE_A_Pin|VOLTAGE_MODE_B_Pin|CHANNLE_SELEC_A_Pin|CHANNLE_SELEC_B_Pin
                          |CHANNLE_SELEC_C_Pin, GPIO_PIN_RESET);

  /* Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, RES_MODE_A_Pin|RES_MODE_B_Pin|RES_MODE_C_Pin, GPIO_PIN_RESET);

  /* Configure GPIO pin : KEY_Pin */
  GPIO_InitStruct.Pin = KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_GPIO_Port, &GPIO_InitStruct);

  /* Configure GPIO pins : VOLTAGE_MODE_A_Pin VOLTAGE_MODE_B_Pin CHANNLE_SELEC_A_Pin CHANNLE_SELEC_B_Pin
                            CHANNLE_SELEC_C_Pin */
  GPIO_InitStruct.Pin = VOLTAGE_MODE_A_Pin|VOLTAGE_MODE_B_Pin|CHANNLE_SELEC_A_Pin|CHANNLE_SELEC_B_Pin
                          |CHANNLE_SELEC_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Configure GPIO pins : RES_MODE_A_Pin RES_MODE_B_Pin RES_MODE_C_Pin */
  GPIO_InitStruct.Pin = RES_MODE_A_Pin|RES_MODE_B_Pin|RES_MODE_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  volatile uint32_t spin = 0u;
  while (1)
  {
    spin++;
    __NOP();
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
