#ifndef SPECK_H
#define SPECK_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Контекст для шифра Speck 64/128
 * Размер блока 64 бита (2x32 бит), размер ключа 128 бит (4x32 бит)
 */
typedef struct {
    uint32_t round_keys[27]; // Ключи для 27 раундов алгоритма Speck 64/128
} SpeckContext;

/**
 * @brief Инициализация контекста шифрования
 * @param ctx Указатель на структуру контекста
 * @param key Указатель на массив из 4-х 32-битных слов ключа
 */
void speck_init(SpeckContext *ctx, const uint32_t *key);

/**
 * @brief Шифрование 64-битного блока данных
 * @param ctx Указатель на инициализированный контекст
 * @param block Указатель на массив из 2-х 32-битных слов (блок для шифрования)
 */
void speck_encrypt(const SpeckContext *ctx, uint32_t *block);

/**
 * @brief Расшифрование 64-битного блока данных
 * @param ctx Указатель на инициализированный контекст
 * @param block Указатель на массив из 2-х 32-битных слов (блок для расшифрования)
 */
void speck_decrypt(const SpeckContext *ctx, uint32_t *block);

/**
 * @brief Вычисление кода аутентификации сообщения (MAC)
 * @param ctx Указатель на инициализированный контекст
 * @param data Указатель на данные для вычисления MAC
 * @param len Длина данных в байтах
 * @param mac Указатель на буфер для записи MAC (8 байт)
 */
void speck_mac(const SpeckContext *ctx, const uint8_t *data, size_t len, uint8_t *mac);

#endif // SPECK_H
