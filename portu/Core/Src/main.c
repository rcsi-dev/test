/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Основной файл программы
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "secure_uart.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TEST_DATA_SIZE 32
#define STATE_SEND_UART1 0
#define STATE_WAIT1 1
#define STATE_SEND_UART6 2
#define STATE_WAIT30 3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart6_rx;
DMA_HandleTypeDef hdma_usart6_tx;

/* USER CODE BEGIN PV */
static uint8_t test_data[TEST_DATA_SIZE];
static uint32_t last_test_time = 0;
static uint8_t current_state = STATE_SEND_UART1;
static uint8_t test_data_uart1[TEST_DATA_SIZE];
static uint8_t test_data_uart6[TEST_DATA_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */

  /* Инициализация DWT для бенчмарка */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* Вывод информации о начале работы */
  char startup_msg[100];
  sprintf(startup_msg, "\r\n\r\n*** Защищенный UART протокол ***\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  sprintf(startup_msg, "*** Тест базового взаимодействия USART1 <-> USART6 ***\r\n\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);

  /* Проверка соединений перед инициализацией */
  sprintf(startup_msg, "Проверка физического соединения...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);

  /* Проверка UART1 -> UART6 */
  uint8_t test_byte = 0xA5;
  sprintf(startup_msg, "Отправка тестового байта 0xA5 с UART1 на UART6...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  HAL_UART_Transmit(&huart1, &test_byte, 1, 100);
  HAL_Delay(10); // Небольшая задержка для приема

  uint8_t rx_byte = 0;
  if (HAL_UART_Receive(&huart6, &rx_byte, 1, 100) == HAL_OK) {
    sprintf(startup_msg, "UART6 получил: 0x%02X %s\r\n",
            rx_byte, (rx_byte == 0xA5) ? "- OK!" : "- ОШИБКА!");
    HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  } else {
    sprintf(startup_msg, "UART6 НЕ получил данные - проверьте соединение UART1_TX -> UART6_RX!\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  }

  /* Проверка UART6 -> UART1 */
  test_byte = 0x5A;
  sprintf(startup_msg, "Отправка тестового байта 0x5A с UART6 на UART1...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  HAL_UART_Transmit(&huart6, &test_byte, 1, 100);
  HAL_Delay(10); // Небольшая задержка для приема

  if (HAL_UART_Receive(&huart1, &rx_byte, 1, 100) == HAL_OK) {
    sprintf(startup_msg, "UART1 получил: 0x%02X %s\r\n",
            rx_byte, (rx_byte == 0x5A) ? "- OK!" : "- ОШИБКА!");
    HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  } else {
    sprintf(startup_msg, "UART1 НЕ получил данные - проверьте соединение UART6_TX -> UART1_RX!\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);
  }

  /* Инициализация защищенного UART */
  sprintf(startup_msg, "\r\nИнициализация защищенного UART протокола...\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)startup_msg, strlen(startup_msg), 100);

  SecureUart_Init(
    &huart1, &huart6, &huart2,
    &hdma_usart1_tx, &hdma_usart1_rx,
    &hdma_usart6_tx, &hdma_usart6_rx
  );

  /* Генерация тестовых данных */
  for (uint8_t i = 0; i < TEST_DATA_SIZE; i++) {
    test_data_uart1[i] = i;
    test_data_uart6[i] = i + 128; // Другой набор данных для UART6
  }

  // Инициализация последовательного цикла отправки
  current_state = STATE_SEND_UART1;
  last_test_time = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Обработка входящих данных */
    SecureUart_Process();

    /* Последовательная отправка тестовых данных с заданными задержками */
    uint32_t current_time = HAL_GetTick();

    switch (current_state) {
      case STATE_SEND_UART1:
        /* Отправка тестовых данных с UART1 на UART6 */
        SecureUart_Debug("\r\n---------------------------------------\r\n");
        SecureUart_Debug("Отправка тестовых данных с UART1 на UART6\r\n");
        SecureUart_SendFromUart1(test_data_uart1, TEST_DATA_SIZE);

        /* Переход к следующему состоянию */
        current_state = STATE_WAIT1;
        last_test_time = current_time;
        break;

      case STATE_WAIT1:
        /* Ожидание 1 секунду */
        if (current_time - last_test_time >= 1000) {
          current_state = STATE_SEND_UART6;
          last_test_time = current_time;
        }
        break;

      case STATE_SEND_UART6:
        /* Отправка тестовых данных с UART6 на UART1 */
        SecureUart_Debug("\r\n---------------------------------------\r\n");
        SecureUart_Debug("Отправка тестовых данных с UART6 на UART1\r\n");
        SecureUart_SendFromUart6(test_data_uart6, TEST_DATA_SIZE);

        /* Переход к следующему состоянию */
        current_state = STATE_WAIT30;
        last_test_time = current_time;
        break;

      case STATE_WAIT30:
        /* Ожидание 30 секунд */
        if (current_time - last_test_time >= 30000) {
          current_state = STATE_SEND_UART1;
          last_test_time = current_time;

          /* Модификация тестовых данных для следующего цикла */
          for (uint8_t i = 0; i < TEST_DATA_SIZE; i++) {
            test_data_uart1[i] = (test_data_uart1[i] + 1) % 256;
            test_data_uart6[i] = (test_data_uart6[i] + 1) % 256;
          }
        }
        break;
    }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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

/* Переопределение функций HAL для дополнительной отладки */
/**
 * @brief Переопределение функции HAL_UART_TxCpltCallback
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  char debug_msg[100];
  sprintf(debug_msg, "HAL_UART_TxCpltCallback вызван для UART%d\r\n",
          (huart->Instance == USART1) ? 1 :
          (huart->Instance == USART2) ? 2 :
          (huart->Instance == USART6) ? 6 : 0);
  HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);

  if (huart->Instance == USART1) {
    sprintf(debug_msg, "UART1: передача завершена\r\n");
    HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    if (suart_config.uart1.is_tx_busy) {
      suart_config.uart1.is_tx_busy = 0;
    } else {
      sprintf(debug_msg, "ВНИМАНИЕ: UART1 не был помечен как занятый!\r\n");
      HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    }
  } else if (huart->Instance == USART6) {
    sprintf(debug_msg, "UART6: передача завершена\r\n");
    HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    if (suart_config.uart6.is_tx_busy) {
      suart_config.uart6.is_tx_busy = 0;
    } else {
      sprintf(debug_msg, "ВНИМАНИЕ: UART6 не был помечен как занятый!\r\n");
      HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    }
  }
}

/**
 * @brief Переопределение функции HAL_UARTEx_RxEventCallback
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  char debug_msg[100];
  sprintf(debug_msg, "HAL_UARTEx_RxEventCallback вызван для UART%d, размер: %d\r\n",
          (huart->Instance == USART1) ? 1 :
          (huart->Instance == USART2) ? 2 :
          (huart->Instance == USART6) ? 6 : 0, Size);
  HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);

  if (huart->Instance == USART1) {
    uart1_rx_size = Size;
    sprintf(debug_msg, "UART1: получены данные, размер: %d байт\r\n", Size);
    HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    SecureUart_RxCpltCallback_UART1();
  } else if (huart->Instance == USART6) {
    uart6_rx_size = Size;
    sprintf(debug_msg, "UART6: получены данные, размер: %d байт\r\n", Size);
    HAL_UART_Transmit(huart2, (uint8_t*)debug_msg, strlen(debug_msg), 100);
    SecureUart_RxCpltCallback_UART6();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */
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
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
  /* DMA2_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
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
