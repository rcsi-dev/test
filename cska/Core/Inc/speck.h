#ifndef SPECK_H
#define SPECK_H

#include <stdint.h>

/* Определение типов для работы с блоками и ключами Speck */
typedef struct {
    uint32_t l[4]; // Массив для хранения расширенных ключей
} speck_key_t;

/* Функции шифрования/дешифрования */
void speck_init_key(const uint32_t key[4], speck_key_t *exp_key);
void speck_encrypt(const speck_key_t *exp_key, uint32_t plaintext[2], uint32_t ciphertext[2]);
void speck_decrypt(const speck_key_t *exp_key, uint32_t ciphertext[2], uint32_t plaintext[2]);

/* Функции для CMAC */
void speck_cmac(const speck_key_t *exp_key, const uint8_t *data, uint32_t length, uint8_t mac[8]);

#endif /* SPECK_H */
