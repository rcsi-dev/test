/**
 * @file secure_uart.h
 * @brief Заголовочный файл для защищенного протокола UART
 */

#ifndef SECURE_UART_H
#define SECURE_UART_H

#include "main.h"
#include "speck.h"
#include <stdint.h>
#include <stdbool.h>

// Определение размеров и констант
#define SECUART_MAX_DATA_SIZE      255                 // Максимальный размер полезных данных
#define SECUART_HEADER_SIZE        6                   // SOF(1) + CNT(4) + LEN(1)
#define SECUART_MAC_SIZE           8                   // Размер MAC в байтах
#define SECUART_BLOCK_SIZE         8                   // Размер блока шифрования Speck
#define SECUART_START_BYTE         0xAA                // Стартовый байт фрейма
#define SECUART_BUFFER_SIZE        (SECUART_HEADER_SIZE + SECUART_MAX_DATA_SIZE + SECUART_MAC_SIZE)  // Размер буфера

// Типы сообщений
typedef enum {
    SECUART_MSG_DATA = 0x01,     // Обычные данные
    SECUART_MSG_ACK = 0x02,      // Подтверждение
    SECUART_MSG_NACK = 0x03      // Отрицательное подтверждение
} SecUartMsgType;

// Коды ошибок
typedef enum {
    SECUART_OK = 0,              // Нет ошибок
    SECUART_ERR_INVALID_SOF,     // Неверный стартовый байт
    SECUART_ERR_INVALID_MAC,     // Неверный MAC
    SECUART_ERR_REPLAY,          // Обнаружена Replay-атака
    SECUART_ERR_BUFFER_OVERFLOW, // Переполнение буфера
    SECUART_ERR_TIMEOUT          // Таймаут операции
} SecUartError;

// Структура контекста защищенного UART
typedef struct {
    // UART-интерфейсы
    UART_HandleTypeDef *huart_tx;       // UART для передачи
    UART_HandleTypeDef *huart_rx;       // UART для приема
    UART_HandleTypeDef *huart_monitor;  // UART для мониторинга

    // Буферы DMA
    uint8_t tx_buffer[SECUART_BUFFER_SIZE];  // Буфер передачи
    uint8_t rx_buffer[SECUART_BUFFER_SIZE];  // Буфер приема

    // Счетчики
    uint32_t tx_counter;    // Счетчик отправленных пакетов
    uint32_t rx_counter;    // Последний принятый счетчик

    // Флаги состояния
    volatile bool rx_complete;   // Флаг завершения приема
    volatile bool tx_complete;   // Флаг завершения передачи

    // Размеры данных
    uint8_t rx_data_size;        // Размер принятых данных

    // Контекст шифрования
    SpeckContext cipher_ctx;     // Контекст шифра Speck

    // Статистика
    uint32_t packets_sent;       // Отправлено пакетов
    uint32_t packets_received;   // Принято пакетов
    uint32_t errors_detected;    // Обнаружено ошибок
} SecUartContext;

/**
 * @brief Инициализация контекста защищенного UART
 * @param ctx Указатель на структуру контекста
 * @param huart_tx UART для передачи
 * @param huart_rx UART для приема
 * @param huart_monitor UART для мониторинга
 * @param key Ключ шифрования (4 слова по 32 бита)
 * @return Код ошибки
 */
SecUartError SecUart_Init(SecUartContext *ctx,
                         UART_HandleTypeDef *huart_tx,
                         UART_HandleTypeDef *huart_rx,
                         UART_HandleTypeDef *huart_monitor,
                         const uint32_t *key);

/**
 * @brief Отправка данных через защищенный UART
 * @param ctx Указатель на структуру контекста
 * @param data Указатель на данные для отправки
 * @param size Размер данных в байтах
 * @param msg_type Тип сообщения
 * @return Код ошибки
 */
SecUartError SecUart_Send(SecUartContext *ctx,
                         const uint8_t *data,
                         uint8_t size,
                         SecUartMsgType msg_type);

/**
 * @brief Обработка принятых данных
 * @param ctx Указатель на структуру контекста
 * @param data Указатель на буфер для декодированных данных
 * @param size Указатель на переменную для размера данных
 * @param msg_type Указатель на переменную для типа сообщения
 * @return Код ошибки
 */
SecUartError SecUart_ProcessRxData(SecUartContext *ctx,
                                  uint8_t *data,
                                  uint8_t *size,
                                  SecUartMsgType *msg_type);

/**
 * @brief Обработчик прерывания IDLE для UART
 * @param ctx Указатель на структуру контекста
 * @param huart Дескриптор UART, вызвавшего прерывание
 */
void SecUart_RxIdleCallback(SecUartContext *ctx, UART_HandleTypeDef *huart);

/**
 * @brief Запуск приема по DMA
 * @param ctx Указатель на структуру контекста
 * @return Код ошибки
 */
SecUartError SecUart_StartReceive(SecUartContext *ctx);

/**
 * @brief Отправка отладочного сообщения через монитор
 * @param ctx Указатель на структуру контекста
 * @param msg Сообщение для вывода
 */
void SecUart_Log(SecUartContext *ctx, const char *msg);

#endif // SECURE_UART_H
