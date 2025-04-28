/* uart_protocol.c */
#include "uart_protocol.h"

// Буферы для приема данных
static RxBuffer usart1_rx_buffer;
static RxBuffer usart6_rx_buffer;

// Инициализация протокола
void UartProtocol_Init(void) {
    // Инициализация буферов
    memset(&usart1_rx_buffer, 0, sizeof(RxBuffer));
    memset(&usart6_rx_buffer, 0, sizeof(RxBuffer));

    // Установка начального состояния
    usart1_rx_buffer.state = WAIT_START;
    usart6_rx_buffer.state = WAIT_START;

    // Выводим информацию об инициализации
    char log_message[] = "Протокол UART инициализирован\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
}

// Отправка фрейма по UART
void UartProtocol_SendFrame(UART_HandleTypeDef *huart, uint8_t *data, uint16_t length) {
    char source[10];

    // Определяем источник сообщения для логирования
    if (huart->Instance == USART1) {
        strcpy(source, "USART1");
    } else if (huart->Instance == USART6) {
        strcpy(source, "USART6");
    } else {
        strcpy(source, "UNKNOWN");
    }

    // Выводим информацию о начале отправки
    char log_message[100];
    snprintf(log_message, sizeof(log_message), "[%s] Отправка фрейма, длина данных: %u\r\n",
             source, length);
    HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);

    // Создаем фрейм
    UartFrame frame;

    // Заполнение структуры фрейма
    frame.start_marker = FRAME_START_MARKER;
    frame.data_length = length;
    memcpy(frame.data, data, length);
    frame.crc = UartProtocol_CalculateCRC(data, length);
    frame.end_marker = FRAME_END_MARKER;

    // Отправка фрейма по частям для надежности
    // Отправляем маркер начала
    HAL_UART_Transmit(huart, (uint8_t *)&frame.start_marker, sizeof(frame.start_marker), HAL_MAX_DELAY);
    // Отправляем длину данных
    HAL_UART_Transmit(huart, (uint8_t *)&frame.data_length, sizeof(frame.data_length), HAL_MAX_DELAY);
    // Отправляем данные
    HAL_UART_Transmit(huart, frame.data, length, HAL_MAX_DELAY);
    // Отправляем CRC
    HAL_UART_Transmit(huart, (uint8_t *)&frame.crc, sizeof(frame.crc), HAL_MAX_DELAY);
    // Отправляем маркер конца
    HAL_UART_Transmit(huart, (uint8_t *)&frame.end_marker, sizeof(frame.end_marker), HAL_MAX_DELAY);

    // Выводим информацию о содержимом отправленных данных
    snprintf(log_message, sizeof(log_message), "[%s] Отправлено: '%.*s'\r\n",
             source, length, data);
    HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
}

// Обработка принятого байта
void UartProtocol_ProcessRxByte(UART_HandleTypeDef *huart, uint8_t byte) {
    RxBuffer *rxBuffer;
    char source[10];

    // Определяем, какой буфер использовать
    if (huart->Instance == USART1) {
        rxBuffer = &usart1_rx_buffer;
        strcpy(source, "USART1");
    } else if (huart->Instance == USART6) {
        rxBuffer = &usart6_rx_buffer;
        strcpy(source, "USART6");
    } else {
        return; // Неизвестный UART
    }

    // Добавляем байт в буфер
    rxBuffer->buffer[rxBuffer->position++] = byte;

    // Обработка в зависимости от текущего состояния
    switch (rxBuffer->state) {
        case WAIT_START:
            if (rxBuffer->position == 2) {
                uint16_t marker = *(uint16_t *)rxBuffer->buffer;
                if (marker == FRAME_START_MARKER) {
                    rxBuffer->state = READ_LENGTH;
                } else {
                    // Неверный маркер, сбрасываем позицию
                    rxBuffer->position = 0;
                }
            }
            break;

        case READ_LENGTH:
            if (rxBuffer->position == 4) {
                // Маркер (2) + длина (2) = 4 байта
                rxBuffer->state = READ_DATA;
            }
            break;

        case READ_DATA: {
            uint16_t data_length = *(uint16_t *)(rxBuffer->buffer + 2);
            if (rxBuffer->position == 4 + data_length) {
                rxBuffer->state = READ_CRC;
            }
            break;
        }

        case READ_CRC:
            if (rxBuffer->position == 4 + *(uint16_t *)(rxBuffer->buffer + 2) + 4) {
                rxBuffer->state = READ_END;
            }
            break;

        case READ_END:
            if (rxBuffer->position == 4 + *(uint16_t *)(rxBuffer->buffer + 2) + 4 + 2) {
                // Полный фрейм получен, проверяем маркер конца
                uint16_t end_marker = *(uint16_t *)(rxBuffer->buffer + rxBuffer->position - 2);
                if (end_marker == FRAME_END_MARKER) {
                    // Проверяем CRC
                    uint16_t data_length = *(uint16_t *)(rxBuffer->buffer + 2);
                    uint8_t *data = rxBuffer->buffer + 4;
                    uint32_t received_crc = *(uint32_t *)(data + data_length);
                    uint32_t calculated_crc = UartProtocol_CalculateCRC(data, data_length);

                    if (received_crc == calculated_crc) {
                        // Фрейм успешно принят, выводим информацию
                        char log_message[MAX_DATA_SIZE + 100];
                        snprintf(log_message, sizeof(log_message),
                                 "[%s] Принят фрейм, длина данных: %u\r\n",
                                 source, data_length);
                        HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);

                        // Выводим принятые данные в консоль
                        snprintf(log_message, sizeof(log_message),
                                 "[%s] Принято: '%.*s'\r\n",
                                 source, data_length, data);
                        HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
                    } else {
                        // Ошибка CRC
                        char log_message[100];
                        snprintf(log_message, sizeof(log_message),
                                 "[%s] Ошибка CRC: получено 0x%08X, вычислено 0x%08X\r\n",
                                 source, received_crc, calculated_crc);
                        HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
                    }
                } else {
                    // Неверный маркер конца
                    char log_message[100];
                    snprintf(log_message, sizeof(log_message),
                             "[%s] Ошибка: неверный маркер конца 0x%04X\r\n",
                             source, end_marker);
                    HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
                }

                // Сбрасываем буфер для следующего фрейма
                rxBuffer->position = 0;
                rxBuffer->state = WAIT_START;
            }
            break;
    }
}

// Расчет CRC32
uint32_t UartProtocol_CalculateCRC(uint8_t *data, uint16_t length) {
    // Стандартная реализация CRC32
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}
