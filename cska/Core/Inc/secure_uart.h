#ifndef SECURE_UART_H
#define SECURE_UART_H

#include "main.h"
#include "speck.h"

/* Константы */
#define SECURE_UART_HEADER          0xA55A5AA5
#define SECURE_UART_MAX_DATA_SIZE   64
#define SECURE_UART_FRAME_OVERHEAD  17  // 4 (header) + 4 (counter) + 1 (len) + 8 (CMAC)

/* Типы */
typedef struct {
    uint32_t header;        // Заголовок фрейма
    uint32_t counter;       // Счетчик для защиты от повторов
    uint8_t data_len;       // Длина данных
    uint8_t data[SECURE_UART_MAX_DATA_SIZE]; // Буфер данных
    uint8_t cmac[8];        // Код аутентификации сообщения
} secure_uart_frame_t;

typedef struct {
    UART_HandleTypeDef *huart;     // Хэндл UART
    speck_key_t key;               // Ключ шифрования
    uint32_t last_rx_counter;      // Последний принятый счетчик
    uint32_t tx_counter;           // Счетчик отправки
    secure_uart_frame_t tx_frame;  // Фрейм отправки
    secure_uart_frame_t rx_frame;  // Фрейм приема
    uint8_t rx_buffer[SECURE_UART_MAX_DATA_SIZE + SECURE_UART_FRAME_OVERHEAD]; // Буфер приема
    uint8_t tx_buffer[SECURE_UART_MAX_DATA_SIZE + SECURE_UART_FRAME_OVERHEAD]; // Буфер отправки
    uint8_t processing;            // Флаг обработки данных
    void (*receive_callback)(uint8_t *data, uint8_t length); // Коллбэк приема данных
} secure_uart_handle_t;

/* Функции */
void secure_uart_init(secure_uart_handle_t *handle, UART_HandleTypeDef *huart, const uint32_t key[4],
                       void (*receive_callback)(uint8_t *data, uint8_t length));
void secure_uart_send(secure_uart_handle_t *handle, const uint8_t *data, uint8_t length);
void secure_uart_process_received(secure_uart_handle_t *handle);
void secure_uart_handle_idle(secure_uart_handle_t *handle);

#endif /* SECURE_UART_H */
