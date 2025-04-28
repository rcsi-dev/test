/**
 * @file secure_uart.c
 * @brief Реализация защищенного UART протокола
 */

#include "secure_uart.h"
#include <stdio.h>
#include <stdarg.h>

// Глобальный счетчик для последовательных номеров
static uint32_t g_sequence_counter = 0;

/**
 * @brief Инициализация контекста защищенного UART
 * @param ctx Указатель на структуру контекста
 * @param huart Дескриптор основного UART
 * @param debug_uart Дескриптор UART для отладки
 */
void SecureUart_Init(SecureUartContext *ctx, UART_HandleTypeDef *huart, UART_HandleTypeDef *debug_uart) {
    memset(ctx, 0, sizeof(SecureUartContext));
    ctx->huart = huart;
    ctx->debug_uart = debug_uart;
    ctx->rx_pos = 0;
    ctx->last_sequence_id = 0;

    SecureUart_DebugPrint(ctx->debug_uart, "Инициализация защищенного UART протокола\r\n");
}

/**
 * @brief Запуск приема данных по UART в режиме DMA с IDLE прерыванием
 * @param ctx Указатель на структуру контекста
 */
void SecureUart_StartReceive(SecureUartContext *ctx) {
    // Включаем прерывание по IDLE
    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);

    // Запускаем прием данных по DMA
    HAL_UART_Receive_DMA(ctx->huart, ctx->rx_buffer, MAX_FRAME_SIZE);

    SecureUart_DebugPrint(ctx->debug_uart, "Начат прием данных в режиме DMA с IDLE прерыванием\r\n");
}

/**
 * @brief Отправка защищенного фрейма по UART
 * @param ctx Указатель на структуру контекста
 * @param data Данные для отправки
 * @param length Длина данных
 * @return Статус операции
 */
SecureUartStatus SecureUart_Send(SecureUartContext *ctx, const uint8_t *data, uint8_t length) {
    if (length > MAX_DATA_SIZE) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка: превышен максимальный размер данных\r\n");
        return SECURE_UART_BUFFER_OVERFLOW;
    }

    // Увеличиваем счетчик последовательности
    uint32_t seq_id = g_sequence_counter++;

    // Буфер для фрейма
    uint8_t frame[MAX_FRAME_SIZE];
    uint16_t frame_pos = 0;

    // Заголовок пакета
    uint32_t header = FRAME_HEADER;
    memcpy(frame + frame_pos, &header, FRAME_HEADER_SIZE);
    frame_pos += FRAME_HEADER_SIZE;

    // Идентификатор последовательности
    memcpy(frame + frame_pos, &seq_id, SEQUENCE_ID_SIZE);
    frame_pos += SEQUENCE_ID_SIZE;

    // Длина данных
    frame[frame_pos++] = length;

    // Данные
    if (length > 0) {
        memcpy(frame + frame_pos, data, length);
        frame_pos += length;
    }

    // Расчет и добавление CRC
    uint16_t crc = SecureUart_CalculateCRC(frame, frame_pos);
    memcpy(frame + frame_pos, &crc, CRC_SIZE);
    frame_pos += CRC_SIZE;

    // Отладочный вывод
    SecureUart_DebugPrint(ctx->debug_uart, "Отправка фрейма (seq_id=%lu, длина=%u):\r\n", seq_id, length);
    SecureUart_PrintHexBuffer(ctx->debug_uart, "TX: ", frame, frame_pos);

    // Отправка фрейма
    HAL_StatusTypeDef hal_status = HAL_UART_Transmit(ctx->huart, frame, frame_pos, 1000);
    if (hal_status != HAL_OK) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка отправки: %d\r\n", hal_status);
        return SECURE_UART_ERROR;
    }

    return SECURE_UART_OK;
}

/**
 * @brief Обработка принятых данных
 * @param ctx Указатель на структуру контекста
 * @param packet Указатель на структуру для сохранения распакованных данных
 * @return Статус операции
 */
SecureUartStatus SecureUart_ProcessReceivedData(SecureUartContext *ctx, SecureUartPacket *packet) {
    uint16_t frame_size = ctx->rx_pos;

    // Проверка минимального размера фрейма
    if (frame_size < MIN_FRAME_SIZE) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка: недостаточный размер фрейма (%u)\r\n", frame_size);
        return SECURE_UART_INVALID_FRAME;
    }

    // Проверка заголовка
    uint32_t received_header;
    memcpy(&received_header, ctx->rx_buffer, FRAME_HEADER_SIZE);
    if (received_header != FRAME_HEADER) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка: неверный заголовок фрейма (0x%08lX)\r\n", received_header);
        return SECURE_UART_INVALID_FRAME;
    }

    // Извлечение sequence_id
    uint32_t sequence_id;
    memcpy(&sequence_id, ctx->rx_buffer + FRAME_HEADER_SIZE, SEQUENCE_ID_SIZE);

    // Проверка на replay-атаку
    if (sequence_id <= ctx->last_sequence_id) {
        SecureUart_DebugPrint(ctx->debug_uart, "Обнаружена возможная replay-атака (seq_id=%lu, last=%lu)\r\n",
                              sequence_id, ctx->last_sequence_id);
        return SECURE_UART_REPLAY_ATTACK;
    }

    // Обновляем последний известный sequence_id
    ctx->last_sequence_id = sequence_id;

    // Извлечение длины данных
    uint8_t data_length = ctx->rx_buffer[FRAME_HEADER_SIZE + SEQUENCE_ID_SIZE];

    // Проверка корректности длины данных
    if (data_length > MAX_DATA_SIZE) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка: некорректная длина данных (%u)\r\n", data_length);
        return SECURE_UART_INVALID_FRAME;
    }

    // Проверка полного размера фрейма
    uint16_t expected_frame_size = MIN_FRAME_SIZE + data_length;
    if (frame_size < expected_frame_size) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка: неполный фрейм (ожидалось %u, получено %u)\r\n",
                              expected_frame_size, frame_size);
        return SECURE_UART_INVALID_FRAME;
    }

    // Проверка CRC
    uint16_t received_crc;
    memcpy(&received_crc, ctx->rx_buffer + FRAME_HEADER_SIZE + SEQUENCE_ID_SIZE + 1 + data_length, CRC_SIZE);

    uint16_t calculated_crc = SecureUart_CalculateCRC(ctx->rx_buffer, FRAME_HEADER_SIZE + SEQUENCE_ID_SIZE + 1 + data_length);
    if (received_crc != calculated_crc) {
        SecureUart_DebugPrint(ctx->debug_uart, "Ошибка CRC (получено 0x%04X, рассчитано 0x%04X)\r\n",
                              received_crc, calculated_crc);
        return SECURE_UART_CRC_ERROR;
    }

    // Заполнение структуры пакета
    packet->sequence_id = sequence_id;
    packet->data_length = data_length;
    if (data_length > 0) {
        memcpy(packet->data, ctx->rx_buffer + FRAME_HEADER_SIZE + SEQUENCE_ID_SIZE + 1, data_length);
    }

    // Отладочный вывод
    SecureUart_DebugPrint(ctx->debug_uart, "Принят фрейм (seq_id=%lu, длина=%u):\r\n", sequence_id, data_length);
    SecureUart_PrintHexBuffer(ctx->debug_uart, "RX: ", ctx->rx_buffer, frame_size);

    // Если данные есть, выводим их
    if (data_length > 0) {
        SecureUart_PrintHexBuffer(ctx->debug_uart, "Данные: ", packet->data, data_length);
    }

    return SECURE_UART_OK;
}

/**
 * @brief Простой расчет CRC16
 * @param data Указатель на данные
 * @param length Длина данных
 * @return Значение CRC16
 */
uint16_t SecureUart_CalculateCRC(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // Полином 0x8005 в отраженном виде
            } else {
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Обработчик события приема данных по UART
 * @param ctx Указатель на структуру контекста
 */
void SecureUart_HandleUartRxEvent(SecureUartContext *ctx) {
    // Проверка на IDLE прерывание
    if (__HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE)) {
        // Сброс флага IDLE
        __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);

        // Остановка DMA
        HAL_UART_DMAStop(ctx->huart);

        // Расчет количества полученных байт
        ctx->rx_pos = MAX_FRAME_SIZE - __HAL_DMA_GET_COUNTER(ctx->huart->hdmarx);

        // Обработка принятых данных
        SecureUartPacket packet;
        SecureUartStatus status = SecureUart_ProcessReceivedData(ctx, &packet);

        if (status == SECURE_UART_OK) {
            SecureUart_DebugPrint(ctx->debug_uart, "Пакет успешно обработан\r\n");
        }

        // Сброс указателя приема
        ctx->rx_pos = 0;

        // Перезапуск приема
        HAL_UART_Receive_DMA(ctx->huart, ctx->rx_buffer, MAX_FRAME_SIZE);
    }
}

/**
 * @brief Отладочный вывод форматированной строки в UART
 * @param huart Дескриптор UART для отладки
 * @param format Формат строки
 * @param ... Аргументы
 */
void SecureUart_DebugPrint(UART_HandleTypeDef *huart, const char *format, ...) {
    if (huart == NULL) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
}

/**
 * @brief Вывод содержимого буфера в HEX формате
 * @param huart Дескриптор UART для отладки
 * @param prefix Префикс для вывода
 * @param buffer Указатель на буфер данных
 * @param length Длина данных
 */
void SecureUart_PrintHexBuffer(UART_HandleTypeDef *huart, const char *prefix, const uint8_t *buffer, uint16_t length) {
    if (huart == NULL) return;

    char outstr[16];

    // Вывод префикса
    HAL_UART_Transmit(huart, (uint8_t*)prefix, strlen(prefix), 100);

    // Вывод буфера в HEX формате
    for (uint16_t i = 0; i < length; i++) {
        sprintf(outstr, "%02X ", buffer[i]);
        HAL_UART_Transmit(huart, (uint8_t*)outstr, strlen(outstr), 100);

        // Перенос строки каждые 16 байт
        if ((i + 1) % 16 == 0 && i < length - 1) {
            sprintf(outstr, "\r\n       ");
            HAL_UART_Transmit(huart, (uint8_t*)outstr, strlen(outstr), 100);
        }
    }

    // Завершающий перенос строки
    HAL_UART_Transmit(huart, (uint8_t*)"\r\n", 2, 100);
}
