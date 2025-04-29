#include "speck.h"

/* Макрофункции для операций алгоритма Speck */
#define ROR(x, r) ((x >> r) | (x << (32 - r)))
#define ROL(x, r) ((x << r) | (x >> (32 - r)))
#define R(x, y, k) (x = ROR(x, 8), x += y, x ^= k, y = ROL(y, 3), y ^= x)
#define ROUNDS 27  // Количество раундов для Speck 64/128

/* Инициализация ключа шифрования */
void speck_init_key(const uint32_t key[4], speck_key_t *exp_key) {
    uint32_t i;
    uint32_t b = key[0];
    uint32_t a[3] = {key[1], key[2], key[3]};

    exp_key->l[0] = b;

    for (i = 0; i < ROUNDS - 1; i++) {
        R(a[i % 3], b, i);
        exp_key->l[i + 1] = b;
    }
}

/* Функция шифрования блока данных */
void speck_encrypt(const speck_key_t *exp_key, uint32_t plaintext[2], uint32_t ciphertext[2]) {
    uint32_t i;
    ciphertext[0] = plaintext[0];
    ciphertext[1] = plaintext[1];

    for (i = 0; i < ROUNDS; i++) {
        R(ciphertext[1], ciphertext[0], exp_key->l[i]);
    }
}

/* Функция дешифрования блока данных */
void speck_decrypt(const speck_key_t *exp_key, uint32_t ciphertext[2], uint32_t plaintext[2]) {
    int32_t i;
    plaintext[0] = ciphertext[0];
    plaintext[1] = ciphertext[1];

    for (i = ROUNDS - 1; i >= 0; i--) {
        plaintext[1] ^= plaintext[0];
        plaintext[1] = ROR(plaintext[1], 3);
        plaintext[0] ^= exp_key->l[i];
        plaintext[0] -= plaintext[1];
        plaintext[0] = ROL(plaintext[0], 8);
    }
}

/* Вычисление CMAC на базе Speck */
void speck_cmac(const speck_key_t *exp_key, const uint8_t *data, uint32_t length, uint8_t mac[8]) {
    uint32_t i, j;
    uint32_t block[2] = {0, 0};
    uint32_t last_block[2] = {0, 0};

    // Отладочный вывод
    char debug_msg[100];
    sprintf(debug_msg, "speck_cmac: длина данных=%lu", length);
    // Используйте функцию monitor_print, если она доступна
    // monitor_print(debug_msg);

    // Обработка полных блоков
    for (i = 0; i + 8 <= length; i += 8) {
        // Корректная загрузка данных
        block[0] = ((uint32_t)data[i]) |
                  ((uint32_t)data[i+1] << 8) |
                  ((uint32_t)data[i+2] << 16) |
                  ((uint32_t)data[i+3] << 24);

        block[1] = ((uint32_t)data[i+4]) |
                  ((uint32_t)data[i+5] << 8) |
                  ((uint32_t)data[i+6] << 16) |
                  ((uint32_t)data[i+7] << 24);

        // XOR с предыдущим результатом
        // Первый блок не нужно XORить с предыдущим
        if (i > 0) {
            block[0] ^= ((uint32_t*)mac)[0];
            block[1] ^= ((uint32_t*)mac)[1];
        }

        // Шифрование блока
        speck_encrypt(exp_key, block, (uint32_t*)mac);
    }

    // Обработка последнего блока
    uint32_t remaining = length - i;
    if (remaining > 0) {
        memset(last_block, 0, sizeof(last_block));

        // Копирование оставшихся байтов
        for (j = 0; j < remaining; j++) {
            ((uint8_t*)last_block)[j] = data[i + j];
        }

        // Добавление бита 1 и дополнение нулями
        ((uint8_t*)last_block)[remaining] = 0x80;

        // XOR с предыдущим результатом
        if (i > 0) {
            last_block[0] ^= ((uint32_t*)mac)[0];
            last_block[1] ^= ((uint32_t*)mac)[1];
        }

        // Шифрование последнего блока
        speck_encrypt(exp_key, last_block, (uint32_t*)mac);
    }
}
