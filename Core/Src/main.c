/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "WS2812.h"
#include "string.h"
#include "SEGGER_RTT.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define LOG(fmt, ...) SEGGER_RTT_printf(0, fmt "\r\n%s", \
                          ##__VA_ARGS__,                          \
                          RTT_CTRL_RESET)
uint8_t receive_buf[255];
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Image_Display_with_Blocks(uint16_t led_block[][2], uint16_t block_num, uint8_t color);
void Image_Display_with_LED_Num(uint16_t leds_num, uint8_t color);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(htim);

  /* NOTE : This function should not be modified, when the callback is needed,
            the HAL_TIM_PeriodElapsedCallback could be implemented in the user file
   */
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
  const uint8_t c[] = "Receive.";
  HAL_UART_Transmit(&huart3, c, sizeof(c), 10);
  
  WS2812_Decoder(receive_buf);

  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, receive_buf, sizeof(receive_buf));
  __HAL_DMA_DISABLE_IT((&huart3)->hdmarx, DMA_IT_HT);
  // HAL_UARTEx_ReceiveToIdle_DMA(&huart3, receive_buf, 255);
  
  /* NOTE: This function should not be modified, when the callback is needed,
           the HAL_UART_RxCpltCallback could be implemented in the user file
   */
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
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  const uint8_t c[] = "start.";
  // HAL_TIM_Base_Start_IT(&htim1);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, receive_buf, sizeof(receive_buf));
  __HAL_DMA_DISABLE_IT((&huart3)->hdmarx, DMA_IT_HT);
  HAL_UART_Transmit(&huart3, c, sizeof(c), 10);
  
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
  HAL_Delay(1000);

  // OLED_Init();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  WS2812_INIT();
  HAL_Delay(100);
  WS2812_Turn_Off(10000);
  // WS2812_Test_Colorful(100, 0x00FF00, 0);
  
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_RESET);
  
  while (1) {
    static uint16_t pos = 1,last_pos = 1;
    if(HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET ||
        HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_RESET)
    {
      HAL_Delay(100);
      if(HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin) == GPIO_PIN_RESET)  pos++;
      else if(HAL_GPIO_ReadPin(KEY_2_GPIO_Port, KEY_2_Pin) == GPIO_PIN_RESET)  pos--;
      if(pos == 0)  pos = 1;
        // OLED_ShowChar(10, 10, (pos/1000%10)+'0', 12);
        // OLED_ShowChar(20, 10, (pos/100%10)+'0', 12);
        // OLED_ShowChar(30, 10, (pos/10%10)+'0', 12);
        // OLED_ShowChar(40, 10, (pos/1%10)+'0', 12);
        // OLED_Refresh();
    }
    else if(last_pos != pos)
    {
      last_pos = pos;
      WS2812_Test_Position(pos , 0xFF0000,1000);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    WS2812_Detect();
  
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

/* USER CODE BEGIN 4 */
void Image_Display_with_Blocks(uint16_t led_block[][2], uint16_t block_num, uint8_t color){
    // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_SET);
    
    // uint8_t buffer_selector = 0;
    // uint8_t transimit_start_flag = 0;
    // uint16_t block_offset = 0;

    // WS2812_Clear(buffer_selector);
    // for(uint8_t i = 0; i <= block_num; i++){
    //     uint8_t empty_block_num = led_block[i][0] / LED_NUM_SINGLE_FRAME - block_offset;
    //     while(empty_block_num){
    //         WS2812_Clear(buffer_selector);
    //         WS2812_transmit_flag[buffer_selector] = Ready;
    //         if(!transimit_start_flag){
    //             transimit_start_flag = 1;
    //             WS2812_Start(buffer_selector);
    //         }
    //         buffer_selector = (buffer_selector+1)%WS2812_BUFFER_NUM;
    //         block_offset++;
    //         while(WS2812_transmit_flag[buffer_selector] != Free);

    //         empty_block_num--;
    //     }

    //     uint16_t led_block_start_idx = led_block[i][0] - block_offset * WS2812_BUFFER_NUM;
    //     uint16_t led_block_end_idx = led_block[i][1] - block_offset * WS2812_BUFFER_NUM;
    
    //     for (uint16_t j = led_block_start_idx; j < led_block_end_idx; j++){
    //         uint8_t idx_builking = 0;
    //         switch(color){
    //             case 'R':WS2812_Set(buffer_selector, j - idx_builking, 0xFF0000);break;
    //             case 'B':WS2812_Set(buffer_selector, j - idx_builking, 0x0000FF);break;
    //             case 'G':WS2812_Set(buffer_selector, j - idx_builking, 0x00FF00);break;
    //             default:WS2812_Set(buffer_selector, j - idx_builking, 0x000000);break;
    //         }
    //         if (j > (idx_builking+1) * LED_NUM_SINGLE_FRAME){
    //             idx_builking++;
                
    //             WS2812_transmit_flag[buffer_selector] = Ready;
    //             buffer_selector = (buffer_selector+1)%WS2812_BUFFER_NUM;
    //             block_offset++;
    //             while(WS2812_transmit_flag[buffer_selector] != Free);
    //         }
    //     }
    // }
    // WS2812_transmit_flag[buffer_selector] = Ready;
    
    // HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin,GPIO_PIN_RESET);
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
    while (1) {
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
