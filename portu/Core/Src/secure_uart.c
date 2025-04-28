/**
  * @file    secure_uart.c
  * @brief   Исходный файл библиотеки защищенного UART протокола
  */

#include "secure_uart.h"
#include <stdio.h>

/* Приватные переменные */
static SecureUartConfig suart_config;

/* Буферы для передачи и приема данных */
static uint8_t uart1_tx_buffer[FRAME_BUFFER_SIZE];
static uint8_t uart1_rx_buffer[FRAME_BUFFER_SIZE];
static uint8_t uart6_tx_buffer[FRAME_BUFFER_SIZE];
static uint8_t uart6_rx_buffer[FRAME_BUFFER_SIZE];
static uint8_t debug_tx_buffer[FRAME_BUFFER_SIZE];

/* Флаги приема данных */
static volatile uint8_t uart1_data_received = 0;
static volatile uint8_t uart6_data_received = 0;
static volatile uint16_t uart1_rx_size = 0;
static volatile uint16_t uart6_rx_size = 0;

/* CRC таблица */
static uint32_t crc32_table[256];

/* Приватные функции */
static void SecureUart_InitCRC32Table(void);
static void SecureUart_PrintBuffer(const char *prefix, const uint8_t *buffer, uint16_t size);

/**
 * @brief  Инициализация защищенного UART
 */
uint8_t SecureUart_Init(
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart6, UART_HandleTypeDef *huart2,
    DMA_HandleTypeDef *hdma_usart1_tx, DMA_HandleTypeDef *hdma_usart1_rx,
    DMA_HandleTypeDef *hdma_usart6_tx, DMA_HandleTypeDef *hdma_usart6_rx
) {
    /* Инициализация структуры конфигурации */
    suart_config.uart1.huart = huart1;
    suart_config.uart1.hdma_tx = hdma_usart1_tx;
    suart_config.uart1.hdma_rx = hdma_usart1_rx;
    suart_config.uart1.tx_buffer = uart1_tx_buffer;
    suart_config.uart1.rx_buffer = uart1_rx_buffer;
    suart_config.uart1.tx_buffer_size = FRAME_BUFFER_SIZE;
    suart_config.uart1.rx_buffer_size = FRAME_BUFFER_SIZE;
    suart_config.uart1.is_tx_busy = 0;
    suart_config.uart1.is_rx_busy = 0;

    suart_config.uart6.huart = huart6;
    suart_config.uart6.hdma_tx = hdma_usart6_tx;
    suart_config.uart6.hdma_rx = hdma_usart6_rx;
    suart_config.uart6.tx_buffer = uart6_tx_buffer;
    suart_config.uart6.rx_buffer = uart6_rx_buffer;
    suart_config.uart6.tx_buffer_size = FRAME_BUFFER_SIZE;
    suart_config.uart6.rx_buffer_size = FRAME_BUFFER_SIZE;
    suart_config.uart6.is_tx_busy = 0;
    suart_config.uart6.is_rx_busy = 0;

    suart_config.debug.huart = huart2;
    suart_config.debug.hdma_tx = NULL; // Не используем DMA для отладки
    suart_config.debug.hdma_rx = NULL; // Не используем DMA для отладки
    suart_config.debug.tx_buffer = debug_tx_buffer;
    suart_config.debug.rx_buffer = NULL; // Не используем прием для отладки
    suart_config.debug.tx_buffer_size = FRAME_BUFFER_SIZE;
    suart_config.debug.rx_buffer_size = 0;
    suart_config.debug.is_tx_busy = 0;
    suart_config.debug.is_rx_busy = 0;

    /* Инициализация таблицы CRC32 */
    SecureUart_InitCRC32Table();

    /* Очистка флагов */
    uart1_data_received = 0;
    uart6_data_received = 0;

    /* Вывод отладочного сообщения */
    SecureUart_Debug("Защищенный UART инициализирован\r\n");

    /* Запускаем прием данных на обоих UART */
    if (HAL_UARTEx_ReceiveToIdle_DMA(suart_config.uart1.huart, suart_config.uart1.rx_buffer, FRAME_BUFFER_SIZE) != HAL_OK) {
        SecureUart_Debug("ОШИБКА: Не удалось запустить прием на UART1\r\n");
    } else {
        SecureUart_Debug("UART1: Прием запущен успешно\r\n");
    }
    __HAL_DMA_DISABLE_IT(suart_config.uart1.hdma_rx, DMA_IT_HT);

    if (HAL_UARTEx_ReceiveToIdle_DMA(suart_config.uart6.huart, suart_config.uart6.rx_buffer, FRAME_BUFFER_SIZE) != HAL_OK) {
        SecureUart_Debug("ОШИБКА: Не удалось запустить прием на UART6\r\n");
    } else {
        SecureUart_Debug("UART6: Прием запущен успешно\r\n");
    }
    __HAL_DMA_DISABLE_IT(suart_config.uart6.hdma_rx, DMA_IT_HT);

    return 0;
}

/**
 * @brief  Отправка данных через UART1
 */
FrameStatus SecureUart_SendFromUart1(uint8_t *data, uint16_t size) {
    if (suart_config.uart1.is_tx_busy) {
        SecureUart_Debug("UART1: Ошибка - передатчик занят\r\n");
        return FRAME_BUSY;
    }

    if (size > FRAME_MAX_DATA_SIZE) {
        SecureUart_Debug("Ошибка: размер данных превышает максимальный\r\n");
        return FRAME_ERROR_LENGTH;
    }

    /* Строим фрейм данных */
    uint16_t frame_size = SecureUart_BuildFrame(suart_config.uart1.tx_buffer, data, size);

    /* Отладочный вывод */
    char debug_msg[100];
    sprintf(debug_msg, "UART1 -> UART6: отправка %d байт данных (размер фрейма %d байт)\r\n", size, frame_size);
    SecureUart_Debug(debug_msg);
    SecureUart_PrintBuffer("TX данные UART1", data, size);
    SecureUart_PrintBuffer("TX фрейм UART1", suart_config.uart1.tx_buffer, frame_size);

    /* Отправляем данные по DMA */
    suart_config.uart1.is_tx_busy = 1;
    HAL_UART_Transmit_DMA(suart_config.uart1.huart, suart_config.uart1.tx_buffer, frame_size);

    return FRAME_OK;
}

/**
 * @brief  Отправка данных через UART6
 */
FrameStatus SecureUart_SendFromUart6(uint8_t *data, uint16_t size) {
    if (suart_config.uart6.is_tx_busy) {
        SecureUart_Debug("UART6: Ошибка - передатчик занят\r\n");
        return FRAME_BUSY;
    }

    if (size > FRAME_MAX_DATA_SIZE) {
        SecureUart_Debug("Ошибка: размер данных превышает максимальный\r\n");
        return FRAME_ERROR_LENGTH;
    }

    /* Строим фрейм данных */
    uint16_t frame_size = SecureUart_BuildFrame(suart_config.uart6.tx_buffer, data, size);

    /* Отладочный вывод */
    char debug_msg[100];
    sprintf(debug_msg, "UART6 -> UART1: отправка %d байт данных (размер фрейма %d байт)\r\n", size, frame_size);
    SecureUart_Debug(debug_msg);
    SecureUart_PrintBuffer("TX данные UART6", data, size);
    SecureUart_PrintBuffer("TX фрейм UART6", suart_config.uart6.tx_buffer, frame_size);

    /* Отправляем данные по DMA */
    suart_config.uart6.is_tx_busy = 1;
    HAL_UART_Transmit_DMA(suart_config.uart6.huart, suart_config.uart6.tx_buffer, frame_size);

    return FRAME_OK;
}

/**
 * @brief  Отправка отладочного сообщения через UART2
 */
void SecureUart_Debug(const char *message) {
    uint16_t len = strlen(message);
    HAL_UART_Transmit(suart_config.debug.huart, (uint8_t*)message, len, 100);
}

/**
 * @brief  Вывод буфера в отладочный порт
 */
static void SecureUart_PrintBuffer(const char *prefix, const uint8_t *buffer, uint16_t size) {
    char debug_msg[100];
    sprintf(debug_msg, "%s [%d bytes]: ", prefix, size);
    SecureUart_Debug(debug_msg);

    for (uint16_t i = 0; i < size && i < 32; i++) {
        sprintf(debug_msg, "%02X ", buffer[i]);
        SecureUart_Debug(debug_msg);

        // Добавляем перенос строки каждые 16 байт для удобства чтения
        if ((i + 1) % 16 == 0 && i < 31) {
            SecureUart_Debug("\r\n                  ");
        }
    }

    if (size > 32) {
        SecureUart_Debug("...");
    }

    SecureUart_Debug("\r\n");
}

/**
 * @brief  Инициализация таблицы CRC32
 */
static void SecureUart_InitCRC32Table(void) {
    uint32_t c;
    for (uint32_t i = 0; i < 256; i++) {
        c = i;
        for (uint32_t j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
}

/**
 * @brief  Функция для вычисления CRC32
 */
uint32_t SecureUart_CalculateCRC32(const uint8_t *data, uint16_t size) {
    uint32_t crc = 0xFFFFFFFF;

    for (uint16_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }

    return ~crc;
}

/**
 * @brief  Сборка фрейма данных
 */
uint16_t SecureUart_BuildFrame(uint8_t *frame, const uint8_t *data, uint16_t size) {
    uint16_t idx = 0;

    /* Заголовок */
    frame[idx++] = (FRAME_HEADER >> 8) & 0xFF;
    frame[idx++] = FRAME_HEADER & 0xFF;

    /* Длина данных */
    frame[idx++] = (size >> 8) & 0xFF;
    frame[idx++] = size & 0xFF;

    /* Данные */
    memcpy(&frame[idx], data, size);
    idx += size;

    /* CRC32 */
    uint32_t crc = SecureUart_CalculateCRC32(frame, idx);
    frame[idx++] = (crc >> 24) & 0xFF;
    frame[idx++] = (crc >> 16) & 0xFF;
    frame[idx++] = (crc >> 8) & 0xFF;
    frame[idx++] = crc & 0xFF;

    return idx;
}

/**
 * @brief  Разбор принятого фрейма
 */
FrameStatus SecureUart_ParseFrame(const uint8_t *frame, uint16_t frame_size, uint8_t *data, uint16_t *size) {
    if (frame_size < FRAME_OVERHEAD) {
        return FRAME_ERROR_LENGTH;
    }

    /* Проверка заголовка */
    uint16_t header = (frame[0] << 8) | frame[1];
    if (header != FRAME_HEADER) {
        return FRAME_ERROR_HEADER;
    }

    /* Получение длины данных */
    uint16_t data_length = (frame[2] << 8) | frame[3];

    /* Проверка длины данных */
    if (data_length > FRAME_MAX_DATA_SIZE || data_length + FRAME_OVERHEAD > frame_size) {
        return FRAME_ERROR_LENGTH;
    }

    /* Проверка CRC */
    uint32_t received_crc =
        ((uint32_t)frame[4 + data_length] << 24) |
        ((uint32_t)frame[5 + data_length] << 16) |
        ((uint32_t)frame[6 + data_length] << 8) |
        (uint32_t)frame[7 + data_length];

    uint32_t calculated_crc = SecureUart_CalculateCRC32(frame, 4 + data_length);

    if (received_crc != calculated_crc) {
        return FRAME_ERROR_CRC;
    }

    /* Копирование данных */
    memcpy(data, &frame[4], data_length);
    *size = data_length;

    return FRAME_OK;
}

/**
 * @brief  Обработчик завершения приема по DMA для UART1
 */
void SecureUart_RxCpltCallback_UART1(void) {
    uart1_data_received = 1;
    suart_config.uart1.is_rx_busy = 0;

    /* Вывод информации о полученных данных */
    SecureUart_Debug("UART1: Прием завершен, буфер помечен для обработки\r\n");

    /* Сразу запускаем прием для следующих данных */
    HAL_UARTEx_ReceiveToIdle_DMA(suart_config.uart1.huart, suart_config.uart1.rx_buffer, FRAME_BUFFER_SIZE);
    __HAL_DMA_DISABLE_IT(suart_config.uart1.hdma_rx, DMA_IT_HT);
}

/**
 * @brief  Обработчик завершения приема по DMA для UART6
 */
void SecureUart_RxCpltCallback_UART6(void) {
    uart6_data_received = 1;
    suart_config.uart6.is_rx_busy = 0;

    /* Вывод информации о полученных данных */
    SecureUart_Debug("UART6: Прием завершен, буфер помечен для обработки\r\n");

    /* Сразу запускаем прием для следующих данных */
    HAL_UARTEx_ReceiveToIdle_DMA(suart_config.uart6.huart, suart_config.uart6.rx_buffer, FRAME_BUFFER_SIZE);
    __HAL_DMA_DISABLE_IT(suart_config.uart6.hdma_rx, DMA_IT_HT);
}

/**
 * @brief  Обработчик завершения передачи по DMA для UART1
 */
void SecureUart_TxCpltCallback_UART1(void) {
    suart_config.uart1.is_tx_busy = 0;
    SecureUart_Debug("UART1: передача завершена\r\n");
}

/**
 * @brief  Обработчик завершения передачи по DMA для UART6
 */
void SecureUart_TxCpltCallback_UART6(void) {
    suart_config.uart6.is_tx_busy = 0;
    SecureUart_Debug("UART6: передача завершена\r\n");
}

/**
 * @brief  Проверка наличия принятых данных
 */
uint8_t SecureUart_IsDataReceived(void) {
    return uart1_data_received || uart6_data_received;
}

/**
 * @brief  Обработка полученных данных
 */
void SecureUart_Process(void) {
    uint8_t data[FRAME_MAX_DATA_SIZE];
    uint16_t size;
    FrameStatus status;

    /* Обработка данных с UART6 */
    if (uart6_data_received) {
        SecureUart_Debug("\r\nНачало разбора данных от UART6...\r\n");
        SecureUart_PrintBuffer("Буфер UART6", suart_config.uart6.rx_buffer, uart6_rx_size);

        status = SecureUart_ParseFrame(suart_config.uart6.rx_buffer, uart6_rx_size, data, &size);

        if (status == FRAME_OK) {
            char debug_msg[100];
            sprintf(debug_msg, "UART1 <- UART6: Успешно получено %d байт данных\r\n", size);
            SecureUart_Debug(debug_msg);
            SecureUart_PrintBuffer("RX UART1", data, size);
        } else {
            char debug_msg[100];
            sprintf(debug_msg, "UART1 <- UART6: Ошибка разбора фрейма: %d\r\n", status);
            SecureUart_Debug(debug_msg);

            // Дополнительный дамп для отладки в случае ошибки
            uint16_t header = (suart_config.uart6.rx_buffer[0] << 8) | suart_config.uart6.rx_buffer[1];
            uint16_t length = (suart_config.uart6.rx_buffer[2] << 8) | suart_config.uart6.rx_buffer[3];
            sprintf(debug_msg, "  Заголовок: 0x%04X (ожидается 0x%04X)\r\n", header, FRAME_HEADER);
            SecureUart_Debug(debug_msg);
            sprintf(debug_msg, "  Длина данных: %d\r\n", length);
            SecureUart_Debug(debug_msg);
        }

        uart6_data_received = 0;
    }

    /* Обработка данных с UART1 */
    if (uart1_data_received) {
        SecureUart_Debug("\r\nНачало разбора данных от UART1...\r\n");
        SecureUart_PrintBuffer("Буфер UART1", suart_config.uart1.rx_buffer, uart1_rx_size);

        status = SecureUart_ParseFrame(suart_config.uart1.rx_buffer, uart1_rx_size, data, &size);

        if (status == FRAME_OK) {
            char debug_msg[100];
            sprintf(debug_msg, "UART6 <- UART1: Успешно получено %d байт данных\r\n", size);
            SecureUart_Debug(debug_msg);
            SecureUart_PrintBuffer("RX UART6", data, size);
        } else {
            char debug_msg[100];
            sprintf(debug_msg, "UART6 <- UART1: Ошибка разбора фрейма: %d\r\n", status);
            SecureUart_Debug(debug_msg);

            // Дополнительный дамп для отладки в случае ошибки
            uint16_t header = (suart_config.uart1.rx_buffer[0] << 8) | suart_config.uart1.rx_buffer[1];
            uint16_t length = (suart_config.uart1.rx_buffer[2] << 8) | suart_config.uart1.rx_buffer[3];
            sprintf(debug_msg, "  Заголовок: 0x%04X (ожидается 0x%04X)\r\n", header, FRAME_HEADER);
            SecureUart_Debug(debug_msg);
            sprintf(debug_msg, "  Длина данных: %d\r\n", length);
            SecureUart_Debug(debug_msg);
        }

        uart1_data_received = 0;
    }
}

/**
 * @brief  Обработчик события приема данных по UART с DMA (для обоих UART)
 * @note   Эта функция должна быть вызвана из колбека HAL_UARTEx_RxEventCallback
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    char debug_msg[100];

    /* Определяем, какой UART вызвал колбек */
    if (huart == suart_config.uart1.huart) {
        uart1_rx_size = Size;
        sprintf(debug_msg, "UART1: получены данные, размер: %d байт\r\n", Size);
        SecureUart_Debug(debug_msg);
        SecureUart_RxCpltCallback_UART1();
    } else if (huart == suart_config.uart6.huart) {
        uart6_rx_size = Size;
        sprintf(debug_msg, "UART6: получены данные, размер: %d байт\r\n", Size);
        SecureUart_Debug(debug_msg);
        SecureUart_RxCpltCallback_UART6();
    }
}

/**
 * @brief  Обработчик завершения передачи по UART с DMA (для обоих UART)
 * @note   Эта функция должна быть вызвана из колбека HAL_UART_TxCpltCallback
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    /* Определяем, какой UART вызвал колбек */
    if (huart == suart_config.uart1.huart) {
        SecureUart_TxCpltCallback_UART1();
    } else if (huart == suart_config.uart6.huart) {
        SecureUart_TxCpltCallback_UART6();
    }
}
