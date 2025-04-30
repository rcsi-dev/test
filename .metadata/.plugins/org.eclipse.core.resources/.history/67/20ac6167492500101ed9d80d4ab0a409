/**
 * @file secure_uart.c
 * @brief Реализация защищенного протокола UART
 */

#include "secure_uart.h"
#include "speck.h"
#include <string.h>
#include <stdio.h>

// Статические вспомогательные функции
static void SecUart_EncryptBlock(const SpeckContext *ctx, uint8_t *data, uint8_t size);
static void SecUart_DecryptBlock(const SpeckContext *ctx, uint8_t *data, uint8_t size);
static void SecUart_CalculateMAC(const SpeckContext *ctx, const uint8_t *data, uint8_t size, uint8_t *mac);
static bool SecUart_VerifyMAC(const SpeckContext *ctx, const uint8_t *data, uint8_t size, const uint8_t *mac);
static void SecUart_PrepareFrame(SecUartContext *ctx, const uint8_t *data, uint8_t size, SecUartMsgType msg_type);

/**
 * @brief Инициализация контекста защищенного UART
 */
SecUartError SecUart_Init(SecUartContext *ctx,
		UART_HandleTypeDef *huart_tx,
		UART_HandleTypeDef *huart_rx,
		UART_HandleTypeDef *huart_monitor,
		const uint32_t *key) {

	if (ctx == NULL || huart_tx == NULL || huart_rx == NULL || key == NULL) {
		return SECUART_ERR_INVALID_SOF;
	}

	// Инициализация интерфейсов UART
	ctx->huart_tx = huart_tx;
	ctx->huart_rx = huart_rx;
	ctx->huart_monitor = huart_monitor;

	// Инициализация счетчиков и флагов
	ctx->tx_counter = 0;
	ctx->rx_counter = 0;
	ctx->rx_complete = false;
	ctx->tx_complete = true;
	ctx->packets_sent = 0;
	ctx->packets_received = 0;
	ctx->errors_detected = 0;

	// Инициализация контекста шифрования
	speck_init(&ctx->cipher_ctx, key);

	// Очистка буферов
	memset(ctx->tx_buffer, 0, SECUART_BUFFER_SIZE);
	memset(ctx->rx_buffer, 0, SECUART_BUFFER_SIZE);

	// Запуск приема данных по DMA
	return SecUart_StartReceive(ctx);
}

/**
 * @brief Запуск приема по DMA
 */
SecUartError SecUart_StartReceive(SecUartContext *ctx) {
	if (ctx == NULL || ctx->huart_rx == NULL) {
		return SECUART_ERR_INVALID_SOF;
	}

	// Сначала останавливаем любой текущий прием
	HAL_UART_AbortReceive(ctx->huart_rx);

	// Сброс флага завершения приема
	ctx->rx_complete = false;

	// Очистка буфера приема
	memset(ctx->rx_buffer, 0, SECUART_BUFFER_SIZE);

	// Запуск приема данных по DMA до прерывания IDLE
	if (HAL_UART_Receive_DMA(ctx->huart_rx, ctx->rx_buffer, SECUART_BUFFER_SIZE) != HAL_OK) {
		return SECUART_ERR_TIMEOUT;
	}

	// Включение прерывания IDLE
	__HAL_UART_ENABLE_IT(ctx->huart_rx, UART_IT_IDLE);

	// Отладочное сообщение
	SecUart_Log(ctx, "DMA receive restarted\r\n");

	return SECUART_OK;
}

/**
 * @brief Отправка данных через защищенный UART
 */
/**
 * @brief Отправка данных через защищенный UART
 */
SecUartError SecUart_Send(SecUartContext *ctx,
		const uint8_t *data,
		uint8_t size,
		SecUartMsgType msg_type) {

	if (ctx == NULL || data == NULL || size == 0 || size > SECUART_MAX_DATA_SIZE) {
		return SECUART_ERR_INVALID_SOF;
	}

	// Проверяем, завершена ли предыдущая передача
	if (!ctx->tx_complete) {
		// Добавляем лог для отладки
		SecUart_Log(ctx, "TX busy, tx_complete is false\r\n");

		// Проверяем статус UART и DMA
		uint32_t uart_status = ctx->huart_tx->gState;
		char status_buf[64];
		snprintf(status_buf, sizeof(status_buf),
				"UART State: 0x%lX\r\n", uart_status);
		SecUart_Log(ctx, status_buf);

		// Если UART находится в состоянии ошибки или неизвестном состоянии, сбрасываем его
		if (uart_status != HAL_UART_STATE_READY && uart_status != HAL_UART_STATE_BUSY_TX) {
			SecUart_Log(ctx, "Resetting UART TX state\r\n");
			HAL_UART_AbortTransmit(ctx->huart_tx);
			ctx->tx_complete = true;
		} else {
			return SECUART_ERR_TIMEOUT;
		}
	}

	// Подготовка фрейма для отправки
	SecUart_PrepareFrame(ctx, data, size, msg_type);

	// Общий размер фрейма: заголовок + размер данных + MAC
	uint16_t frame_size = SECUART_HEADER_SIZE + size + SECUART_MAC_SIZE;

	// Сброс флага завершения передачи
	ctx->tx_complete = false;

	// Отправка данных по DMA
	HAL_StatusTypeDef hal_status = HAL_UART_Transmit_DMA(ctx->huart_tx, ctx->tx_buffer, frame_size);
	if (hal_status != HAL_OK) {
		// Выводим код ошибки HAL для диагностики
		char err_buf[64];
		snprintf(err_buf, sizeof(err_buf),
				"HAL TX Error: %d\r\n", hal_status);
		SecUart_Log(ctx, err_buf);

		ctx->tx_complete = true;
		return SECUART_ERR_TIMEOUT;
	}

	// Увеличиваем счетчик отправленных пакетов
	ctx->packets_sent++;

	// Отладочное сообщение в монитор
	char log_buffer[64];
	snprintf(log_buffer, sizeof(log_buffer),
			"TX: Counter=%lu, Size=%u, Type=%u\r\n",
			ctx->tx_counter, size, msg_type);
	SecUart_Log(ctx, log_buffer);

	return SECUART_OK;
}

/**
 * @brief Подготовка фрейма для отправки
 */
static void SecUart_PrepareFrame(SecUartContext *ctx, const uint8_t *data, uint8_t size, SecUartMsgType msg_type) {
	// Очистка буфера передачи
	memset(ctx->tx_buffer, 0, SECUART_BUFFER_SIZE);

	// Заполнение заголовка
	ctx->tx_buffer[0] = SECUART_START_BYTE;                  // SOF
	ctx->tx_counter++;                                       // Увеличиваем счетчик
	ctx->tx_buffer[1] = (ctx->tx_counter >> 24) & 0xFF;      // CNT (MSB)
	ctx->tx_buffer[2] = (ctx->tx_counter >> 16) & 0xFF;
	ctx->tx_buffer[3] = (ctx->tx_counter >> 8) & 0xFF;
	ctx->tx_buffer[4] = ctx->tx_counter & 0xFF;              // CNT (LSB)
	ctx->tx_buffer[5] = size;                                // LEN

	// Копирование данных с учетом типа сообщения
	ctx->tx_buffer[SECUART_HEADER_SIZE] = msg_type;          // Тип сообщения

	// Убедимся, что мы не выходим за границы размера, особенно для ACK
	if (size > 1) {
		memcpy(ctx->tx_buffer + SECUART_HEADER_SIZE + 1, data, size - 1);
	}
	// Шифрование данных
	SecUart_EncryptBlock(&ctx->cipher_ctx, ctx->tx_buffer + SECUART_HEADER_SIZE, size);

	// Вычисление MAC для всего фрейма (заголовок + зашифрованные данные)
	SecUart_CalculateMAC(&ctx->cipher_ctx, ctx->tx_buffer, SECUART_HEADER_SIZE + size,
			ctx->tx_buffer + SECUART_HEADER_SIZE + size);
}

/**
 * @brief Обработка принятых данных
 */
SecUartError SecUart_ProcessRxData(SecUartContext *ctx,
		uint8_t *data,
		uint8_t *size,
		SecUartMsgType *msg_type) {

	if (ctx == NULL || data == NULL || size == NULL || msg_type == NULL) {
		return SECUART_ERR_INVALID_SOF;
	}

	// Проверяем флаг завершения приема
	if (!ctx->rx_complete) {
		return SECUART_ERR_TIMEOUT;
	}

	// Проверяем стартовый байт
	if (ctx->rx_buffer[0] != SECUART_START_BYTE) {
		ctx->errors_detected++;
		SecUart_Log(ctx, "ERR: Invalid SOF\r\n");
		return SECUART_ERR_INVALID_SOF;
	}

	// Извлекаем счетчик и размер данных
	uint32_t rx_counter = ((uint32_t)ctx->rx_buffer[1] << 24) |
			((uint32_t)ctx->rx_buffer[2] << 16) |
			((uint32_t)ctx->rx_buffer[3] << 8) |
			ctx->rx_buffer[4];
	uint8_t rx_size = ctx->rx_buffer[5];

	// Проверяем защиту от Replay-атак (счетчик должен быть больше предыдущего)
	if (rx_counter <= ctx->rx_counter && ctx->rx_counter > 0) {
		ctx->errors_detected++;

		char log_buffer[64];
		snprintf(log_buffer, sizeof(log_buffer),
				"ERR: Replay attack detected (%lu <= %lu)\r\n",
				rx_counter, ctx->rx_counter);
		SecUart_Log(ctx, log_buffer);

		return SECUART_ERR_REPLAY;
	}

	// Проверяем размер данных
	if (rx_size == 0 || rx_size > SECUART_MAX_DATA_SIZE) {
		ctx->errors_detected++;
		SecUart_Log(ctx, "ERR: Invalid data size\r\n");
		return SECUART_ERR_BUFFER_OVERFLOW;
	}

	// Проверяем MAC
	uint8_t *rx_mac = ctx->rx_buffer + SECUART_HEADER_SIZE + rx_size;
	bool mac_valid = SecUart_VerifyMAC(&ctx->cipher_ctx,
			ctx->rx_buffer,
			SECUART_HEADER_SIZE + rx_size,
			rx_mac);

	if (!mac_valid) {
		ctx->errors_detected++;
		SecUart_Log(ctx, "ERR: Invalid MAC\r\n");
		return SECUART_ERR_INVALID_MAC;
	}

	// Дешифруем данные
	SecUart_DecryptBlock(&ctx->cipher_ctx, ctx->rx_buffer + SECUART_HEADER_SIZE, rx_size);

	// Извлекаем тип сообщения
	*msg_type = (SecUartMsgType)ctx->rx_buffer[SECUART_HEADER_SIZE];

	// Если размер данных равен 0 или 1, то данных нет, только тип сообщения
	if (rx_size <= 1) {
		*size = 0;
	} else {
		// Иначе копируем данные без учета типа сообщения
		*size = rx_size - 1;
		memcpy(data, ctx->rx_buffer + SECUART_HEADER_SIZE + 1, *size);
	}

	// Обновляем счетчик
	ctx->rx_counter = rx_counter;
	ctx->rx_complete = false;

	// Увеличиваем счетчик принятых пакетов
	ctx->packets_received++;

	// Отладочное сообщение в монитор
	char log_buffer[64];
	snprintf(log_buffer, sizeof(log_buffer),
			"RX: Counter=%lu, Size=%u, Type=%u\r\n",
			rx_counter, *size, *msg_type);
	SecUart_Log(ctx, log_buffer);

	// Перезапускаем прием
	SecUart_StartReceive(ctx);

	return SECUART_OK;
}

/**
 * @brief Обработчик прерывания IDLE для UART
 */
void SecUart_RxIdleCallback(SecUartContext *ctx, UART_HandleTypeDef *huart) {
	if (ctx == NULL || huart != ctx->huart_rx) {
		return;
	}

	// Отключаем прерывание IDLE
	__HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);

	// Останавливаем DMA
	HAL_UART_AbortReceive(huart);

	// Вычисляем количество принятых байт
	uint32_t dma_index = __HAL_DMA_GET_COUNTER(huart->hdmarx);
	ctx->rx_data_size = SECUART_BUFFER_SIZE - dma_index;

	// Проверяем минимальный размер принятых данных
	// (заголовок + тип сообщения + MAC)
	if (ctx->rx_data_size < SECUART_HEADER_SIZE + 1 + SECUART_MAC_SIZE) {
		// Сбрасываем прием из-за недостаточного размера пакета
		SecUart_StartReceive(ctx);
		return;
	}

	// Устанавливаем флаг завершения приема
	ctx->rx_complete = true;

	// Отладочное сообщение в монитор
	char log_buffer[64];
	snprintf(log_buffer, sizeof(log_buffer),
			"IDLE: Received %u bytes\r\n", ctx->rx_data_size);
	SecUart_Log(ctx, log_buffer);
}

/**
 * @brief Отправка отладочного сообщения через монитор
 */
void SecUart_Log(SecUartContext *ctx, const char *msg) {
	if (ctx == NULL || ctx->huart_monitor == NULL || msg == NULL) {
		return;
	}

	HAL_UART_Transmit(ctx->huart_monitor, (uint8_t*)msg, strlen(msg), 100);
}

/**
 * @brief Шифрование блока данных
 */
static void SecUart_EncryptBlock(const SpeckContext *ctx, uint8_t *data, uint8_t size) {
	uint8_t padded_size = ((size + SECUART_BLOCK_SIZE - 1) / SECUART_BLOCK_SIZE) * SECUART_BLOCK_SIZE;

	// Обрабатываем данные блоками по 8 байт (64 бит)
	for (uint8_t i = 0; i < padded_size; i += SECUART_BLOCK_SIZE) {
		uint32_t block[2];

		// Преобразуем 8 байт в два 32-битных слова
		if (i + 3 < size) {
			block[0] = ((uint32_t)data[i] << 24) |
					((uint32_t)data[i+1] << 16) |
					((uint32_t)data[i+2] << 8) |
					data[i+3];
		} else {
			// Дополнение нулями, если недостаточно данных
			block[0] = 0;
			for (uint8_t j = 0; j < 4 && i + j < size; j++) {
				block[0] |= ((uint32_t)data[i+j] << ((3-j) * 8));
			}
		}

		if (i + 7 < size) {
			block[1] = ((uint32_t)data[i+4] << 24) |
					((uint32_t)data[i+5] << 16) |
					((uint32_t)data[i+6] << 8) |
					data[i+7];
		} else {
			// Дополнение нулями, если недостаточно данных
			block[1] = 0;
			for (uint8_t j = 0; j < 4 && i + 4 + j < size; j++) {
				block[1] |= ((uint32_t)data[i+4+j] << ((3-j) * 8));
			}
		}

		// Шифруем блок
		speck_encrypt(ctx, block);

		// Преобразуем два 32-битных слова обратно в 8 байт
		// и записываем обратно в буфер
		if (i + 3 < size) {
			data[i] = (block[0] >> 24) & 0xFF;
			data[i+1] = (block[0] >> 16) & 0xFF;
			data[i+2] = (block[0] >> 8) & 0xFF;
			data[i+3] = block[0] & 0xFF;
		} else {
			// Записываем только нужное количество байт
			for (uint8_t j = 0; j < 4 && i + j < size; j++) {
				data[i+j] = (block[0] >> ((3-j) * 8)) & 0xFF;
			}
		}

		if (i + 7 < size) {
			data[i+4] = (block[1] >> 24) & 0xFF;
			data[i+5] = (block[1] >> 16) & 0xFF;
			data[i+6] = (block[1] >> 8) & 0xFF;
			data[i+7] = block[1] & 0xFF;
		} else {
			// Записываем только нужное количество байт
			for (uint8_t j = 0; j < 4 && i + 4 + j < size; j++) {
				data[i+4+j] = (block[1] >> ((3-j) * 8)) & 0xFF;
			}
		}
	}
}

/**
 * @brief Расшифрование блока данных
 */
static void SecUart_DecryptBlock(const SpeckContext *ctx, uint8_t *data, uint8_t size) {
	uint8_t padded_size = ((size + SECUART_BLOCK_SIZE - 1) / SECUART_BLOCK_SIZE) * SECUART_BLOCK_SIZE;

	// Обрабатываем данные блоками по 8 байт (64 бит)
	for (uint8_t i = 0; i < padded_size; i += SECUART_BLOCK_SIZE) {
		uint32_t block[2];

		// Преобразуем 8 байт в два 32-битных слова
		if (i + 3 < size) {
			block[0] = ((uint32_t)data[i] << 24) |
					((uint32_t)data[i+1] << 16) |
					((uint32_t)data[i+2] << 8) |
					data[i+3];
		} else {
			// Дополнение нулями, если недостаточно данных
			block[0] = 0;
			for (uint8_t j = 0; j < 4 && i + j < size; j++) {
				block[0] |= ((uint32_t)data[i+j] << ((3-j) * 8));
			}
		}

		if (i + 7 < size) {
			block[1] = ((uint32_t)data[i+4] << 24) |
					((uint32_t)data[i+5] << 16) |
					((uint32_t)data[i+6] << 8) |
					data[i+7];
		} else {
			// Дополнение нулями, если недостаточно данных
			block[1] = 0;
			for (uint8_t j = 0; j < 4 && i + 4 + j < size; j++) {
				block[1] |= ((uint32_t)data[i+4+j] << ((3-j) * 8));
			}
		}

		// Расшифровываем блок
		speck_decrypt(ctx, block);

		// Преобразуем два 32-битных слова обратно в 8 байт
		// и записываем обратно в буфер
		if (i + 3 < size) {
			data[i] = (block[0] >> 24) & 0xFF;
			data[i+1] = (block[0] >> 16) & 0xFF;
			data[i+2] = (block[0] >> 8) & 0xFF;
			data[i+3] = block[0] & 0xFF;
		} else {
			// Записываем только нужное количество байт
			for (uint8_t j = 0; j < 4 && i + j < size; j++) {
				data[i+j] = (block[0] >> ((3-j) * 8)) & 0xFF;
			}
		}

		if (i + 7 < size) {
			data[i+4] = (block[1] >> 24) & 0xFF;
			data[i+5] = (block[1] >> 16) & 0xFF;
			data[i+6] = (block[1] >> 8) & 0xFF;
			data[i+7] = block[1] & 0xFF;
		} else {
			// Записываем только нужное количество байт
			for (uint8_t j = 0; j < 4 && i + 4 + j < size; j++) {
				data[i+4+j] = (block[1] >> ((3-j) * 8)) & 0xFF;
			}
		}
	}
}

/**
 * @brief Вычисление MAC для данных
 */
static void SecUart_CalculateMAC(const SpeckContext *ctx, const uint8_t *data, uint8_t size, uint8_t *mac) {
	speck_mac(ctx, data, size, mac);
}

/**
 * @brief Проверка MAC для данных
 */
static bool SecUart_VerifyMAC(const SpeckContext *ctx, const uint8_t *data, uint8_t size, const uint8_t *mac) {
	uint8_t calculated_mac[SECUART_MAC_SIZE];

	// Вычисляем MAC
	SecUart_CalculateMAC(ctx, data, size, calculated_mac);

	// Сравниваем MAC
	return (memcmp(calculated_mac, mac, SECUART_MAC_SIZE) == 0);
}
