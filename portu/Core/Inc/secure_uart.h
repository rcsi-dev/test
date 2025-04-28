/**
  * @file    secure_uart.h
  * @brief   Заголовочный файл библиотеки защищенного UART протокола
  */

#ifndef SECURE_UART_H
#define SECURE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <string.h>
#include <stdint.h>

/* Определения для фрейма данных */
#define FRAME_HEADER          0xAA55  // Заголовок фрейма
#define FRAME_HEADER_SIZE     2       // Размер заголовка в байтах
#define FRAME_LENGTH_SIZE     2       // Размер поля длины в байтах
#define FRAME_CRC_SIZE        4       // Размер CRC в байтах
#define FRAME_MAX_DATA_SIZE   256     // Максимальный размер данных
#define FRAME_OVERHEAD        (FRAME_HEADER_SIZE + FRAME_LENGTH_SIZE + FRAME_CRC_SIZE) // Общий оверхед фрейма

/* Максимальный размер буфера фрейма */
#define FRAME_BUFFER_SIZE     (FRAME_OVERHEAD + FRAME_MAX_DATA_SIZE)

/* Статусы обработки фрейма */
typedef enum {
    FRAME_OK = 0,          // Фрейм успешно обработан
    FRAME_ERROR_HEADER,    // Ошибка в заголовке
    FRAME_ERROR_LENGTH,    // Ошибка в длине
    FRAME_ERROR_CRC,       // Ошибка CRC
    FRAME_ERROR_OVERFLOW,  // Переполнение буфера
    FRAME_ERROR_TIMEOUT,   // Таймаут приема
    FRAME_BUSY,            // Передача/прием занят
    FRAME_ERROR_DMA        // Ошибка DMA
} FrameStatus;

/* Структура настроек UART интерфейса */
typedef struct {
    UART_HandleTypeDef *huart;   // Хэндл UART
    DMA_HandleTypeDef *hdma_tx;  // Хэндл DMA TX
    DMA_HandleTypeDef *hdma_rx;  // Хэндл DMA RX
    uint8_t *tx_buffer;          // Буфер передачи
    uint8_t *rx_buffer;          // Буфер приема
    uint16_t tx_buffer_size;     // Размер буфера передачи
    uint16_t rx_buffer_size;     // Размер буфера приема
    uint8_t is_tx_busy;          // Флаг занятости передачи
    uint8_t is_rx_busy;          // Флаг занятости приема
} UartInterface;

/* Структура конфигурации защищенного UART */
typedef struct {
    UartInterface uart1;      // Интерфейс UART1
    UartInterface uart6;      // Интерфейс UART6
    UartInterface debug;      // Интерфейс отладки (UART2)
} SecureUartConfig;

/* Структура фрейма данных */
typedef struct {
    uint16_t header;         // Заголовок (FRAME_HEADER)
    uint16_t length;         // Длина данных
    uint8_t data[FRAME_MAX_DATA_SIZE]; // Данные
    uint32_t crc;            // CRC32
} DataFrame;

/**
 * @brief  Инициализация защищенного UART
 * @param  huart1: указатель на хэндл UART1
 * @param  huart6: указатель на хэндл UART6
 * @param  huart2: указатель на хэндл UART2 (отладка)
 * @param  hdma_usart1_tx: указатель на хэндл DMA TX для UART1
 * @param  hdma_usart1_rx: указатель на хэндл DMA RX для UART1
 * @param  hdma_usart6_tx: указатель на хэндл DMA TX для UART6
 * @param  hdma_usart6_rx: указатель на хэндл DMA RX для UART6
 * @retval Статус инициализации
 */
uint8_t SecureUart_Init(
    UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart6, UART_HandleTypeDef *huart2,
    DMA_HandleTypeDef *hdma_usart1_tx, DMA_HandleTypeDef *hdma_usart1_rx,
    DMA_HandleTypeDef *hdma_usart6_tx, DMA_HandleTypeDef *hdma_usart6_rx
);

/**
 * @brief  Отправка данных через UART1
 * @param  data: указатель на буфер данных
 * @param  size: размер данных
 * @retval Статус отправки
 */
FrameStatus SecureUart_SendFromUart1(uint8_t *data, uint16_t size);

/**
 * @brief  Отправка данных через UART6
 * @param  data: указатель на буфер данных
 * @param  size: размер данных
 * @retval Статус отправки
 */
FrameStatus SecureUart_SendFromUart6(uint8_t *data, uint16_t size);

/**
 * @brief  Отправка отладочного сообщения через UART2
 * @param  message: указатель на строку сообщения
 * @retval None
 */
void SecureUart_Debug(const char *message);

/**
 * @brief  Функция для вычисления CRC32
 * @param  data: указатель на буфер данных
 * @param  size: размер данных
 * @retval Значение CRC32
 */
uint32_t SecureUart_CalculateCRC32(const uint8_t *data, uint16_t size);

/**
 * @brief  Обработчик завершения приема по DMA для UART1
 * @retval None
 */
void SecureUart_RxCpltCallback_UART1(void);

/**
 * @brief  Обработчик завершения приема по DMA для UART6
 * @retval None
 */
void SecureUart_RxCpltCallback_UART6(void);

/**
 * @brief  Обработчик завершения передачи по DMA для UART1
 * @retval None
 */
void SecureUart_TxCpltCallback_UART1(void);

/**
 * @brief  Обработчик завершения передачи по DMA для UART6
 * @retval None
 */
void SecureUart_TxCpltCallback_UART6(void);

/**
 * @brief  Проверка наличия принятых данных
 * @retval 1 если есть принятые данные, 0 если нет
 */
uint8_t SecureUart_IsDataReceived(void);

/**
 * @brief  Получение принятых данных
 * @param  data: указатель на буфер для данных
 * @param  size: указатель на переменную для размера данных
 * @retval Статус получения данных
 */
FrameStatus SecureUart_GetReceivedData(uint8_t *data, uint16_t *size);

/**
 * @brief  Сборка фрейма данных
 * @param  frame: указатель на структуру фрейма
 * @param  data: указатель на буфер данных
 * @param  size: размер данных
 * @retval Размер собранного фрейма
 */
uint16_t SecureUart_BuildFrame(uint8_t *frame, const uint8_t *data, uint16_t size);

/**
 * @brief  Разбор принятого фрейма
 * @param  frame: указатель на буфер с фреймом
 * @param  frame_size: размер фрейма
 * @param  data: указатель на буфер для данных
 * @param  size: указатель на переменную для размера данных
 * @retval Статус разбора фрейма
 */
FrameStatus SecureUart_ParseFrame(const uint8_t *frame, uint16_t frame_size, uint8_t *data, uint16_t *size);

/**
 * @brief  Старт приема данных
 * @retval Статус запуска приема
 */
FrameStatus SecureUart_StartReceive(void);

/**
 * @brief  Обработка задач UART (вызывать в основном цикле)
 * @retval None
 */
void SecureUart_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* SECURE_UART_H */
