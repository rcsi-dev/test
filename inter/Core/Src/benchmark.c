/* benchmark.c */
#include "benchmark.h"
#include "uart_protocol.h"
#include <stdio.h>
#include <string.h>

// Инициализация DWT для точного измерения тактов
void Benchmark_Init(void) {
    // Разрешаем доступ к регистрам DWT
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Сбрасываем счетчик
    DWT->CYCCNT = 0;
    // Включаем счетчик
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Выводим информацию об инициализации
    char log_message[] = "Система измерения производительности инициализирована\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)log_message, strlen(log_message), HAL_MAX_DELAY);
}

// Получение текущего значения счетчика тактов
uint32_t Benchmark_GetCycles(void) {
    return DWT->CYCCNT;
}

// Вывод информации о затраченном времени
void Benchmark_ReportElapsed(const char *label, uint32_t start, uint32_t end) {
    uint32_t elapsed = end - start;
    char message[100];
    snprintf(message, sizeof(message), "Benchmark [%s]: %lu тактов\r\n", label, elapsed);
    HAL_UART_Transmit(&huart2, (uint8_t *)message, strlen(message), HAL_MAX_DELAY);
}
