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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "SX1278.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "ultrasonic.h"
#include "i2c_lcd.h"
#include "event_groups.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ALERT_DONE_BIT      (1 << 0)
#define LCD_DONE_BIT        (1 << 1)
#define LORA_DONE_BIT       (1 << 2)
#define ALL_DONE_BITS       (ALERT_DONE_BIT | LCD_DONE_BIT | LORA_DONE_BIT)

#define WORK_DURATION_MS        (30UL * 1000UL)
#define SLEEP_DURATION_MS       (10UL * 1000UL)   // 10 minutes STOP
#define SENSOR_CYCLE_MS         150UL                     // période lecture capteurs
#define TASK_SYNC_TIMEOUT_MS    3000UL
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
I2C_LCD_HandleTypeDef lcd1;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c3;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* Definitions for alertTask */
osThreadId_t alertTaskHandle;
const osThreadAttr_t alertTask_attributes = {
  .name = "alertTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for LoraTask */
osThreadId_t LoraTaskHandle;
const osThreadAttr_t LoraTask_attributes = {
  .name = "LoraTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for readingSensor */
osThreadId_t readingSensorHandle;
const osThreadAttr_t readingSensor_attributes = {
  .name = "readingSensor",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LcdTask */
osThreadId_t LcdTaskHandle;
const osThreadAttr_t LcdTask_attributes = {
  .name = "LcdTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for QueueData */
osMessageQueueId_t QueueDataHandle;
const osMessageQueueAttr_t QueueData_attributes = {
  .name = "QueueData"
};
/* Definitions for QueueLCD */
osMessageQueueId_t QueueLCDHandle;
const osMessageQueueAttr_t QueueLCD_attributes = {
  .name = "QueueLCD"
};
/* Definitions for QueueAlert */
osMessageQueueId_t QueueAlertHandle;
const osMessageQueueAttr_t QueueAlert_attributes = {
  .name = "QueueAlert"
};
/* USER CODE BEGIN PV */

volatile uint32_t rtc_wakeup_flag = 1;
volatile uint8_t sleep_permitted =0;
volatile uint8_t just_woke = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C3_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_RTC_Init(void);
void alertTaskFunction(void *argument);
void LoraTaskFunction(void *argument);
void readingSensorFunction(void *argument);
void LcdTaskFunction(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
SX1278_hw_t SX1278_hw;
SX1278_t SX1278;
int master ;
int ret ;
EventGroupHandle_t xTasksDoneGroup;

typedef struct {
	float distancecm;
	uint16_t rainIn;
		uint8_t rainexistence;
		uint8_t rainCategory;
		uint8_t alertLevel;
	}SensorData;
uint8_t alert=1;
volatile uint32_t IC_Value1 = 0;
volatile uint32_t IC_Value2 = 0;
volatile uint8_t  edgeState = 0;
volatile uint32_t pulseWidth_us = 0;

	void delay_us(uint32_t us)
{
    uint32_t i;
    for(i = 0; i < (us * 8); i++)
    {
        __NOP();
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
	  __HAL_RCC_GPIOB_CLK_ENABLE();
	    GPIOB->MODER |= (1 << (4*2));   // PB4 output
	    GPIOB->ODR   &= ~(1 << 4);
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
  HAL_Delay(100);
  MX_SPI1_Init();
  MX_I2C3_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  lcd1.hi2c = &hi2c3;     // hi2c1 is your I2C handler
      lcd1.address = 0x4E;    // I2C address for the first LCD
      lcd_init(&lcd1);
  	HAL_Delay(200);

  SX1278_hw.nss.port= GPIOB;
  SX1278_hw.nss.pin= GPIO_PIN_6;
  SX1278_hw.reset.port= GPIOC;
  SX1278_hw.reset.pin= GPIO_PIN_7;
  SX1278_hw.dio0.port= GPIOA;
  SX1278_hw.dio0.pin= GPIO_PIN_10;
  SX1278_hw.spi= &hspi1;
  SX1278.hw = &SX1278_hw;
  SX1278_hw_Reset(&SX1278_hw);
  HAL_Delay(500);
  SX1278_init(&SX1278, 433000000, SX1278_POWER_11DBM,
              SX1278_LORA_SF_7, SX1278_LORA_BW_125KHZ,
              SX1278_LORA_CR_4_5, SX1278_LORA_CRC_EN, 16);

  HAL_Delay(1000);

  uint8_t version = SX1278_SPIRead(&SX1278, 0x42);
  if (version == 0x12) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
  HAL_Delay(1000);
  	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_Delay(1000);
  } else {
         HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
  	HAL_Delay(500);
  	       HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	xTasksDoneGroup = xEventGroupCreate();

  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of QueueData */
  QueueDataHandle = osMessageQueueNew (8, sizeof(SensorData), &QueueData_attributes);

  /* creation of QueueLCD */
  QueueLCDHandle = osMessageQueueNew (8, sizeof(SensorData), &QueueLCD_attributes);

  /* creation of QueueAlert */
  QueueAlertHandle = osMessageQueueNew (8, sizeof(uint8_t), &QueueAlert_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of alertTask */
  alertTaskHandle = osThreadNew(alertTaskFunction, NULL, &alertTask_attributes);

  /* creation of LoraTask */
  LoraTaskHandle = osThreadNew(LoraTaskFunction, NULL, &LoraTask_attributes);

  /* creation of readingSensor */
  readingSensorHandle = osThreadNew(readingSensorFunction, NULL, &readingSensor_attributes);

  /* creation of LcdTask */
  LcdTaskHandle = osThreadNew(LcdTaskFunction, NULL, &LcdTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */
	__HAL_RCC_BACKUPRESET_FORCE();
	    HAL_Delay(10);
	    __HAL_RCC_BACKUPRESET_RELEASE();
	    HAL_Delay(10);
  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
     PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
     PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
     if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
     {
         Error_Handler();
     }
     __HAL_RCC_RTC_ENABLE();
  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2) {
      // Only set time/date on first boot
      if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) { Error_Handler(); }
      if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) { Error_Handler(); }
      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2);}
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 2000, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
  HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);

  __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_IT();
  __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_RISING_EDGE();
  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  HAL_NVIC_SetPriority(TIM3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_1|GPIO_PIN_7|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC1 PC7 PC11 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_1|GPIO_PIN_7|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB4 PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PD2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
  /* TIM3 CH2 — Echo input on PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 50);
    return len;
}
void Ultrasonic_Trigger(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
}
void vApplicationIdleHook(void)
{
		if (sleep_permitted) {
        sleep_permitted = 0;
    HAL_SuspendTick();
   HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    SystemClock_Config();
    HAL_ResumeTick();
			just_woke = 1;
		}
}
void SetRtcWakeup_ms( uint32_t mills){
if (mills <= 32767) {
        // Short durations: DIV16 gives 0.5ms per tick
        uint32_t ticks = mills * 2;
        if (ticks == 0) ticks = 1;
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, ticks, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    } else {
        // Long durations: 1Hz clock, 1 tick = 1 second
        uint32_t seconds = mills / 1000;
        if (seconds == 0) seconds = 1;
        if (seconds > 65535) seconds = 65535;
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
    }
}


void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    rtc_wakeup_flag = 1;
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_alertTaskFunction */
/**
  * @brief  Function implementing the alertTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_alertTaskFunction */
void alertTaskFunction(void *argument)
{
  /* USER CODE BEGIN 5 */
	uint32_t lastAlertLevel = 0;

  /* Infinite loop */
  for(;;)
  {

	  uint32_t alertLevel = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(500));
	  printf("Alert task received notification: %lu\r\n", alertLevel);

	  		 //pdfalse to clear the value , its like saying give me the value tha
	  				//was sent but if it was pdtrue it will be sayin how many times the notifi was send
	  if (alertLevel == lastAlertLevel) {
	            // Same level as before, just set the event bit and continue
	            xEventGroupSetBits(xTasksDoneGroup, ALERT_DONE_BIT);
	            continue;
	        }

	        lastAlertLevel = alertLevel;

	        // Always turn everything off first, then turn on what's needed
	        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);   // red OFF
	        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);   // yellow OFF
	        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);
	        if (alertLevel == 2) {
	                    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   // red ON
	                    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET);  // buzzer ON
	                    printf("CRITICAL: red+buzzer ON\r\n");
	                    vTaskDelay(100);
	                }
	                else if (alertLevel == 1) {
	                    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);   // yellow ON
	                    printf("WARNING: yellow ON\r\n");
	                    vTaskDelay(100);
	                }
	                else {
	                    printf("OK: all OFF\r\n");
	                }


	    xEventGroupSetBits(xTasksDoneGroup, ALERT_DONE_BIT);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_LoraTaskFunction */
/**
* @brief Function implementing the LoraTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LoraTaskFunction */
void LoraTaskFunction(void *argument)
{
  /* USER CODE BEGIN LoraTaskFunction */
	SensorData receivemessage;

		int message_lenght;
		int message_al;
		BaseType_t message;
		BaseType_t message_alert;
		 char txBuffer[64];
		char txalertBuffer[8];
  /* Infinite loop */
  for(;;)
  {
	  		//message_alert=xQueueReceive(QueueAlertHandle,&alert,0);
	/*  xQueueSend(QueueAlertHandle, &alertLevel, pdMS_TO_TICKS(10));
	  		if (message_alert == pdPASS){
	  			  	message_al=snprintf(txalertBuffer, sizeof(txBuffer),"A:%d",alert);
	  			  		ret = SX1278_LoRaEntryTx(&SX1278,message_al, 2000);
	  			  		 if (ret == 1) {
	  			  				ret= SX1278_LoRaTxPacket(&SX1278,(uint8_t*)txalertBuffer, message_al, 2000);
	  			  			 printf(ret == 1 ? "alert sent\r\n" : "alert failed\r\n");
	  			  	}
	  			  		}*/
	  	  message = xQueueReceive(QueueDataHandle,&receivemessage, pdMS_TO_TICKS(500));

	  		if(message == pdPASS){
	  			uint32_t d_int = (uint32_t)receivemessage.distancecm;
	  			uint32_t d_dec = (uint32_t)(receivemessage.distancecm * 10) % 10;
	  			uint32_t rain_int = (uint32_t)receivemessage.rainIn;
	  			message_lenght = snprintf(txBuffer, sizeof(txBuffer),
	  			    "D:%lu.%lu R:%lu W:%d A:%d RF:%d",
	  			    d_int, d_dec, rain_int,
	  			    receivemessage.rainexistence,
	  			    receivemessage.alertLevel,
	  			    receivemessage.rainCategory); // rainCategory = 0=NONE, 1=LIGHT, 2=HEAVY

	  			ret = SX1278_LoRaEntryTx(&SX1278,message_lenght, 2000);

	      if (ret == 1) {
	  				ret= SX1278_LoRaTxPacket(&SX1278,(uint8_t*)txBuffer, message_lenght, 2000);
	          if(ret==1){
	  					HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
	  					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
	      } else {
	          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
	          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
	      }}



    }
	  	xEventGroupSetBits(xTasksDoneGroup, LORA_DONE_BIT);
  }
  /* USER CODE END LoraTaskFunction */
}

/* USER CODE BEGIN Header_readingSensorFunction */
/**
* @brief Function implementing the readingSensor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_readingSensorFunction */
void readingSensorFunction(void *argument)
{
  /* USER CODE BEGIN readingSensorFunction */

	float samples[5];
	uint8_t valid_count = 0;

	const uint32_t CYCLES_PER_WORK_PHASE = (WORK_DURATION_MS / SENSOR_CYCLE_MS);
	uint32_t cycleCount = 0;


		SensorData data ;
  /* Infinite loop */
  for(;;)
  {
	   xEventGroupClearBits(xTasksDoneGroup, ALL_DONE_BITS);
	   valid_count = 0;
	          for (int i = 0; i < 5; i++)
	          {
	              /* Guarantee echo pin is low before triggering */
	              edgeState    = 0;
	              pulseWidth_us = 0;
	              IC_Value1    = 0;
	              IC_Value2    = 0;

	              vTaskDelay(pdMS_TO_TICKS(10));   /* >8ms guard between pings */

	              Ultrasonic_Trigger();
	              __HAL_TIM_SET_CAPTUREPOLARITY(&htim3, TIM_CHANNEL_2,
	                                             TIM_INPUTCHANNELPOLARITY_RISING);
	              HAL_TIM_IC_Start_IT(&htim3, TIM_CHANNEL_2);

	              /* Wait up to 35ms for full echo (max range ~600cm) */
	              uint32_t t0 = HAL_GetTick();
	              while (edgeState != 2 && (HAL_GetTick() - t0) < 35)
	                  vTaskDelay(pdMS_TO_TICKS(1));

	              HAL_TIM_IC_Stop_IT(&htim3, TIM_CHANNEL_2);

	              /* Accept only plausible pulses: 150µs–23200µs (≈2.5cm–400cm) */
	              if (edgeState == 2 && pulseWidth_us > 150 && pulseWidth_us < 23200)
	                  samples[valid_count++] = pulseWidth_us / 58.0f;
	          }

	          /* Insertion sort on valid samples */
	          for (int i = 1; i < valid_count; i++) {
	              float key = samples[i];
	              int j = i - 1;
	              while (j >= 0 && samples[j] > key) { samples[j+1] = samples[j]; j--; }
	              samples[j+1] = key;
	          }

	          /* Use median (or mean of middle values if ≥3 valid) */
	          if (valid_count >= 3) {
	              float sum = 0;
	              for (int i = 1; i < valid_count - 1; i++) sum += samples[i];
	              data.distancecm = sum / (valid_count - 2);
	          } else if (valid_count > 0) {
	              float sum = 0;
	              for (int i = 0; i < valid_count; i++) sum += samples[i];
	              data.distancecm = sum / valid_count;
	          } else {
	              data.distancecm = 0.0f;
	          }




	  		 HAL_ADC_Start(&hadc1);
	  	HAL_ADC_PollForConversion(&hadc1,HAL_MAX_DELAY);
	  	data.rainIn = HAL_ADC_GetValue(&hadc1);
	  	data.rainexistence = !HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);




			// In readingSensorFunction, after reading sensors, do this:

			uint8_t alertLevel = 0;

			// Check conditions and set alertLevel
			if (data.distancecm <= 50 ) {
			    alertLevel = 2;  // CRITICAL
				//xQueueSend(QueueAlertHandle, &alertLevel, pdMS_TO_TICKS(10));
				 uint32_t d = (uint32_t)data.distancecm;
				    uint32_t dd = (uint32_t)(data.distancecm * 10) % 10;
				    printf("CRITICAL! D:%lu.%lucm Rain:%d ADC:%d\r\n",
				           d, dd, data.rainexistence, data.rainIn);			}
			else if ((data.distancecm <= 70) && (data.rainexistence == 1) && (data.rainIn < 2000)) {
			    alertLevel = 1;  // WARNING
			    //xQueueSend(QueueAlertHandle, &alertLevel, pdMS_TO_TICKS(10));
			    uint32_t d = (uint32_t)data.distancecm;
			       uint32_t dd = (uint32_t)(data.distancecm * 10) % 10;
			       printf("WARNING! D:%lu.%lucm Rain:%d ADC:%d\r\n",
			              d, dd, data.rainexistence, data.rainIn);
			}
			else {
				alertLevel = 0;  // NO ALERT
				//xQueueSend(QueueAlertHandle, &alertLevel, pdMS_TO_TICKS(10));

			}

			if (!data.rainexistence)        data.rainCategory = 0;  // NONE
			else if (data.rainIn < 500)     data.rainCategory = 2;  // HEAVY
			else if (data.rainIn < 2000)    data.rainCategory = 1;  // LIGHT
			else                            data.rainCategory = 0;  // NONE
			// Send notification with the actual alertLevel value
			xTaskNotify(alertTaskHandle, alertLevel, eSetValueWithOverwrite);


			data.alertLevel = alertLevel;


	  	xQueueSend(QueueDataHandle, &data, pdMS_TO_TICKS(10));
	  	xQueueSend(QueueLCDHandle, &data, pdMS_TO_TICKS(10));

	  				 xEventGroupWaitBits(
	              xTasksDoneGroup,
	              ALL_DONE_BITS,
	              pdTRUE,                  // clear on exit
	              pdTRUE,                  // wait for ALL
	              pdMS_TO_TICKS(3000)      // 2s safety timeout
	          );

	  			cycleCount++;

	  if (cycleCount % 10 == 0) {
	      char dbg[40];
	      int len = snprintf(dbg, sizeof(dbg), "cycle:%lu/%lu\n",
	                         cycleCount, CYCLES_PER_WORK_PHASE);
	      HAL_UART_Transmit(&huart2, (uint8_t*)dbg, len, 50);
	  }

	  if (cycleCount >= CYCLES_PER_WORK_PHASE) {
	   EventBits_t finalBits = xEventGroupWaitBits(
	          xTasksDoneGroup,
	          ALL_DONE_BITS,
	          pdTRUE,
	          pdTRUE,
	          pdMS_TO_TICKS(5000)   // 5s max — si LoRa freeze, on dort quand même
	      );
	              // Wait for tasks to be done before sleeping (already waited above,
	              // but check result to be safe)
	    if ((finalBits & ALL_DONE_BITS) != ALL_DONE_BITS) {
	          // LoRa ou LCD n'a pas fini — log et on continue quand même
	          char warn[40];
	          int wlen = snprintf(warn, sizeof(warn), "WARN bits=0x%02lX forcing sleep\n",
	                              (unsigned long)finalBits);
	          HAL_UART_Transmit(&huart2, (uint8_t*)warn, wlen, 100);
	      }

	      HAL_UART_Transmit(&huart2, (uint8_t*)"ENTERING SLEEP\n", 15, 100);
	      while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {}

	      HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
	      HAL_Delay(2);
	      SetRtcWakeup_ms(SLEEP_DURATION_MS);
	      __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_IT();
	      __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_RISING_EDGE();

	      HAL_SuspendTick();
	      HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

	      // Réveil ici
	      SystemClock_Config();
	      HAL_ResumeTick();
	      HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
	      MX_SPI1_Init();
	      MX_I2C3_Init();
	  		MX_TIM3_Init();
	      vTaskDelay(pdMS_TO_TICKS(10));
	      SX1278_init(&SX1278, 433000000, SX1278_POWER_11DBM,
	                  SX1278_LORA_SF_7, SX1278_LORA_BW_125KHZ,
	                  SX1278_LORA_CR_4_5, SX1278_LORA_CRC_EN, 16);

	      HAL_UART_Transmit(&huart2, (uint8_t*)"WAKE FROM SLEEP\n", 16, 100);
	      cycleCount = 0;

	  }else {

	  					 vTaskDelay(pdMS_TO_TICKS(SENSOR_CYCLE_MS));
	          }



  }
  /* USER CODE END readingSensorFunction */
}

/* USER CODE BEGIN Header_LcdTaskFunction */
/**
* @brief Function implementing the LcdTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LcdTaskFunction */
void LcdTaskFunction(void *argument)
{
  /* USER CODE BEGIN LcdTaskFunction */
	SensorData receivemessage;
	BaseType_t message;
	char line1[32];
	char line2[32];
  /* Infinite loop */
  for(;;)
  {
	  message = xQueueReceive(QueueLCDHandle,&receivemessage, portMAX_DELAY);

	  		if( message == pdPASS){
	  			float voltage = receivemessage.rainIn * 3.3f / 4095.0f;
	  			// REPLACE WITH:
	  			uint32_t volt_raw = (uint32_t)(receivemessage.rainIn * 330 / 4095); // in 0.01V units
	  			uint32_t v_int = volt_raw / 100;
	  			uint32_t v_dec = volt_raw % 100;

	  			uint32_t d_int = (uint32_t)receivemessage.distancecm;
	  			uint32_t d_dec = (uint32_t)(receivemessage.distancecm * 10) % 10;

	  			snprintf(line1, sizeof(line1), "level:%lu.%lucm  ", d_int, d_dec);
	  			snprintf(line2, sizeof(line2), "rain:%lu.%02luV %s", v_int, v_dec,
	  			         receivemessage.rainexistence ? "YES" : "NO ");lcd_gotoxy(&lcd1, 0, 0);
	  		lcd_puts(&lcd1, line1);
	      lcd_gotoxy(&lcd1, 0, 1);
	      lcd_puts(&lcd1, line2);

	  		}
	      xEventGroupSetBits(xTasksDoneGroup, LCD_DONE_BIT);
  }
  /* USER CODE END LcdTaskFunction */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
