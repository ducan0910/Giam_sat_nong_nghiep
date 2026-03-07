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
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "LoRa.h"
#include "my_lora.h"
#include "my_sensor.h"
#include "my_power.h"
#include <stdlib.h>
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
/*Khai bao data*/
data_parameters testData = {0};

/*Nguong*/
uint8_t TEMP_THRESHOLD_HIGH = 60;     // 60 C
uint8_t TEMP_THRESHOLD_LOW  = 0;      // 0  C
uint8_t HUMIDITY_AIR_HIGH   = 90;     // 90 %
uint8_t HUMIDITY_AIR_LOW    = 10;     // 10 %
uint8_t HUMIDITY_SOIL_HIGH  = 80;     // 80 %
uint8_t HUMIDITY_SOIL_LOW   = 20;     // 20 %
uint16_t Time_Interval			= 42;			// 42s

/*cac bien check*/
uint8_t isJoined = 0;
uint8_t check;
uint8_t type_of_trans_check;

/**/
extern uint32_t current_time_in_gateway;
int32_t time_remaining_ms;

#define WAKEUP_CYCLE_SEC     5
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI1_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
	my_lora_init_default();
	if(LoRa_init(&myLoRa) == LORA_OK){
      for(int i=0; i<6; i++) { HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); HAL_Delay(100); }
  } else {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
      while(1);
  }
	LoRa_gotoMode(&myLoRa, RXCONTIN_MODE);
	
	testData.ID = ID_NEWDEVICE;
	testData.cmd = CMD_JOIN;
	
	//GIAI DOAN 1: JOIN
	while(isJoined == 0)
	{
		if(TDMA_Join_network(&testData, 3000))
    {
      isJoined = 1;

      for(int i=0; i<3; i++) { HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); HAL_Delay(200); }
    }
    else
    {
      HAL_Delay(200 + rand()%500);
    }
	}
	
	time_remaining_ms = calculate_sleep_seconds(testData.ID, (uint16_t)current_time_in_gateway) * 1000;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		/*Kiem tra thoi gian con lai cua chu ki*/
		if(time_remaining_ms > (WAKEUP_CYCLE_SEC + 5) * 1000)		
		{
			check = 1;
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
			my_power_enter_stop_mode(WAKEUP_CYCLE_SEC);
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
		}
		else
		{
			check = 2;
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
			my_power_enter_stop_mode(time_remaining_ms / 1000);
			HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
			time_remaining_ms = 0;
		}
		
		uint32_t start_tick = HAL_GetTick();
		
		/*Do sensor*/
		check = 3;
		testData.cmd = CMD_DATA;
		read_sensors(&testData);
		
		/*Tinh toan thoi gian con lai*/
		time_remaining_ms = time_remaining_ms - WAKEUP_CYCLE_SEC * 1000 - (HAL_GetTick() - start_tick);		 
		
    /*Kiem tra nguong*/
    uint8_t vuot_nguong = check_thresholds(&testData);

		/*Gui data*/
		if(vuot_nguong == 1)		//Truong hop 1: vuot nguong
		{
			if(TDMA_Send_Emergency(&testData))		//neu gui thanh cong (trong API nay da co retry=5)
			{
				type_of_trans_check = 1;
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1); 
				HAL_Delay(200); 
				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
				time_remaining_ms = calculate_sleep_seconds(testData.ID, (uint16_t)current_time_in_gateway) * 1000;		//tinh toan thoi gian ngu cho chu ki tiep
			}
		}
		else if(time_remaining_ms <= 0)			//Truong hop 2: het thoi gian
		{
			uint32_t start_send_data = HAL_GetTick();
			while(HAL_GetTick() - start_send_data <= TDMA_WINDOW * 1000)		//chi gui trong 3000ms dau cua slot
			{
				testData.cmd = CMD_DATA;
				if(my_lora_transmit_wait_data(&testData, 500 + rand()%500))		//neu gui thanh cong
				{
					type_of_trans_check = 2;
					HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
					HAL_Delay(100);
					HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
					HAL_Delay(100);
					time_remaining_ms = calculate_sleep_seconds(testData.ID, (uint16_t)current_time_in_gateway) * 1000;		//tinh toan thoi gian ngu cho chu ki tiep
					break; 
				}
				else time_remaining_ms = calculate_sleep_seconds(testData.ID, (uint16_t)current_time_in_gateway) * 1000;		//tinh toan thoi gian ngu cho chu ki tiep
			}
		}		
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == DIO0_Pin)
  {
    LoRa_Rx_Flag = 1;
  }
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{

}
/* USER CODE END 4 */

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
