/* benchmark.h */
#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "main.h"
#include <stdint.h>

// Инициализация системы измерения производительности
void Benchmark_Init(void);

// Получение текущего значения счетчика тактов
uint32_t Benchmark_GetCycles(void);

// Вывод информации о затраченном времени
void Benchmark_ReportElapsed(const char *label, uint32_t start, uint32_t end);

#endif /* BENCHMARK_H */
