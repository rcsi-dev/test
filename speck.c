#include "speck.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Циклический сдвиг вправо для 32-битного слова
 * @param x Значение для сдвига
 * @param n Количество бит для сдвига
 * @return Результат циклического сдвига
 */
static inline uint32_t ror32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

/**
 * @brief Циклический сдвиг влево для 32-битного слова
 * @param x Значение для сдвига
 * @param n Количество бит для сдвига
 * @return Результат циклического сдвига
 */
static inline uint32_t rol32(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}

void speck_init(SpeckContext *ctx, const uint32_t *key) {
    // Параметры алгоритма Speck для 32-битных слов (согласно спецификации)
    const uint32_t alpha = 8; // Параметр сдвига
    const uint32_t beta = 3;  // Параметр сдвига

    // Раундовые ключи для алгоритма
    uint32_t k[4];

    // Копируем ключ в рабочий буфер
    k[0] = key[0];
    k[1] = key[1];
    k[2] = key[2];
    k[3] = key[3];

    // Расписание ключей для Speck64/128 (27 раундов)
    uint32_t i;
    uint32_t l[3] = {k[1], k[2], k[3]};

    ctx->round_keys[0] = k[0];

    for (i = 0; i < 26; i++) {
        uint32_t idx = i % 3;
        l[idx] = ((ror32(l[idx], alpha) + ctx->round_keys[i]) ^ i);
        ctx->round_keys[i+1] = rol32(ctx->round_keys[i], beta) ^ l[idx];
    }
}

void speck_encrypt(const SpeckContext *ctx, uint32_t *block) {
    // Параметры алгоритма Speck (согласно спецификации)
    const uint32_t alpha = 8; // Параметр сдвига
    const uint32_t beta = 3;  // Параметр сдвига

    uint32_t x = block[0];
    uint32_t y = block[1];

    // Применяем 27 раундов шифрования
    for (uint32_t i = 0; i < 27; i++) {
        x = ror32(x, alpha);
        x = (x + y) ^ ctx->round_keys[i];
        y = rol32(y, beta) ^ x;
    }

    block[0] = x;
    block[1] = y;
}

void speck_decrypt(const SpeckContext *ctx, uint32_t *block) {
    // Параметры алгоритма Speck (согласно спецификации)
    const uint32_t alpha = 8; // Параметр сдвига
    const uint32_t beta = 3;  // Параметр сдвига

    uint32_t x = block[0];
    uint32_t y = block[1];

    // Выполняем 27 раундов расшифрования в обратном порядке
    for (uint32_t i = 0; i < 27; i++) {
        uint32_t round_key = ctx->round_keys[26 - i];

        y = y ^ x;
        y = ror32(y, beta);
        x = ((x ^ round_key) - y);
        x = rol32(x, alpha);
    }

    block[0] = x;
    block[1] = y;
}

void speck_mac(const SpeckContext *ctx, const uint8_t *data, size_t len, uint8_t *mac) {
    uint32_t mac_block[2] = {0, 0}; // Инициализационный вектор - нули
    uint32_t block[2];
    uint8_t *padded_data;
    size_t padded_len;

    // Дополняем данные до кратности 8 байт (64 бит)
    padded_len = ((len + 7) / 8) * 8;
    padded_data = (uint8_t*)calloc(padded_len, 1);
    if (padded_data == NULL) {
        return; // Ошибка выделения памяти
    }

    // Копируем исходные данные в дополненный буфер
    memcpy(padded_data, data, len);

    // Обрабатываем данные блоками по 8 байт (64 бит) - CBC-MAC на основе Speck
    for (size_t i = 0; i < padded_len; i += 8) {
        // Преобразуем 8 байт в два 32-битных слова (big-endian)
        block[0] = ((uint32_t)padded_data[i] << 24) |
                  ((uint32_t)padded_data[i+1] << 16) |
                  ((uint32_t)padded_data[i+2] << 8) |
                  padded_data[i+3];

        block[1] = ((uint32_t)padded_data[i+4] << 24) |
                  ((uint32_t)padded_data[i+5] << 16) |
                  ((uint32_t)padded_data[i+6] << 8) |
                  padded_data[i+7];

        // XOR с предыдущим результатом (для CBC режима)
        block[0] ^= mac_block[0];
        block[1] ^= mac_block[1];

        // Шифруем блок
        speck_encrypt(ctx, block);

        // Сохраняем результат для следующей итерации
        mac_block[0] = block[0];
        mac_block[1] = block[1];
    }

    free(padded_data);

    // Преобразуем 64-битный MAC (2 слова по 32 бита) в 8 байт
    for (int i = 0; i < 4; i++) {
        mac[i] = (mac_block[0] >> (24 - i*8)) & 0xFF;
        mac[i+4] = (mac_block[1] >> (24 - i*8)) & 0xFF;
    }
}
