/**
 * @file siphash.h
 * @brief Реализация алгоритма SipHash для генерации MAC
 *
 * SipHash-2-4 (2 раунда "сжатия" на каждый блок сообщения, 4 "финализирующих" раунда)
 * SipHash - быстрый короткоключевой PRF, разработанный для защиты от DoS-атак
 */

#ifndef SIPHASH_H_
#define SIPHASH_H_

#include <stdint.h>
#include <stddef.h>

/* Константы алгоритма */
#define SIPHASH_CROUND 2  // Кол-во раундов сжатия
#define SIPHASH_FROUND 4  // Кол-во финализирующих раундов
#define SIPHASH_KEY_SIZE 16  // Размер ключа в байтах (128 бит)

/**
 * @brief Генерация 64-битного значения MAC с использованием SipHash-2-4
 *
 * @param key Указатель на ключ (16 байт)
 * @param data Указатель на данные
 * @param len Длина данных в байтах
 * @return Значение MAC (64 бита)
 */
uint64_t SipHash_2_4(const uint8_t* key, const uint8_t* data, size_t len);

/**
 * @brief Генерация 64-битного значения MAC и запись в буфер
 *
 * @param key Указатель на ключ (16 байт)
 * @param data Указатель на данные
 * @param len Длина данных в байтах
 * @param out Буфер для записи MAC (8 байт)
 */
void SipHash_2_4_MAC(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* out);

#endif /* SIPHASH_H_ */
