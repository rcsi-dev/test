/**
 * @file siphash.c
 * @brief Реализация алгоритма SipHash для генерации MAC
 */

#include "siphash.h"
#include <string.h>

/* Макросы для циклического сдвига влево */
#define ROTL64(x, r) (((x) << (r)) | ((x) >> (64 - (r))))

/* Макрос для преобразования 8 байт в 64-битное слово (little-endian) */
#define U8TO64_LE(p) \
    (((uint64_t)((p)[0])) | \
     ((uint64_t)((p)[1]) << 8) | \
     ((uint64_t)((p)[2]) << 16) | \
     ((uint64_t)((p)[3]) << 24) | \
     ((uint64_t)((p)[4]) << 32) | \
     ((uint64_t)((p)[5]) << 40) | \
     ((uint64_t)((p)[6]) << 48) | \
     ((uint64_t)((p)[7]) << 56))

/* Макрос для преобразования 64-битного слова в 8 байт (little-endian) */
#define U64TO8_LE(p, v) \
    do { \
        (p)[0] = (uint8_t)((v)); \
        (p)[1] = (uint8_t)((v) >> 8); \
        (p)[2] = (uint8_t)((v) >> 16); \
        (p)[3] = (uint8_t)((v) >> 24); \
        (p)[4] = (uint8_t)((v) >> 32); \
        (p)[5] = (uint8_t)((v) >> 40); \
        (p)[6] = (uint8_t)((v) >> 48); \
        (p)[7] = (uint8_t)((v) >> 56); \
    } while (0)

/* SipHash раунд */
#define SIPROUND \
    do { \
        v0 += v1; v1 = ROTL64(v1, 13); v1 ^= v0; v0 = ROTL64(v0, 32); \
        v2 += v3; v3 = ROTL64(v3, 16); v3 ^= v2; \
        v0 += v3; v3 = ROTL64(v3, 21); v3 ^= v0; \
        v2 += v1; v1 = ROTL64(v1, 17); v1 ^= v2; v2 = ROTL64(v2, 32); \
    } while (0)

uint64_t SipHash_2_4(const uint8_t* key, const uint8_t* data, size_t len) {
    /* "константа" инициализации */
    const uint64_t k0 = U8TO64_LE(key);
    const uint64_t k1 = U8TO64_LE(key + 8);

    /* Инициализация состояния */
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;

    /* Смешивание ключа с начальным состоянием */
    v0 ^= k0;
    v1 ^= k1;
    v2 ^= k0;
    v3 ^= k1;

    /* Обработка сообщения по блокам */
    const uint8_t* end = data + len - (len % 8);
    const int left = len & 7;
    uint64_t b = ((uint64_t)len) << 56;

    for (; data < end; data += 8) {
        uint64_t m = U8TO64_LE(data);
        v3 ^= m;

        /* Сжимающие раунды */
        for (int i = 0; i < SIPHASH_CROUND; i++) {
            SIPROUND;
        }

        v0 ^= m;
    }

    /* Последний блок с дополнением */
    switch (left) {
        case 7: b |= ((uint64_t)data[6]) << 48; /* fallthrough */
        case 6: b |= ((uint64_t)data[5]) << 40; /* fallthrough */
        case 5: b |= ((uint64_t)data[4]) << 32; /* fallthrough */
        case 4: b |= ((uint64_t)data[3]) << 24; /* fallthrough */
        case 3: b |= ((uint64_t)data[2]) << 16; /* fallthrough */
        case 2: b |= ((uint64_t)data[1]) << 8;  /* fallthrough */
        case 1: b |= ((uint64_t)data[0]);       /* fallthrough */
        case 0: break;
    }

    v3 ^= b;

    /* Сжимающие раунды для последнего блока */
    for (int i = 0; i < SIPHASH_CROUND; i++) {
        SIPROUND;
    }

    v0 ^= b;

    /* Финализирующие раунды */
    v2 ^= 0xff;
    for (int i = 0; i < SIPHASH_FROUND; i++) {
        SIPROUND;
    }

    /* Финальное XOR смешивание */
    return v0 ^ v1 ^ v2 ^ v3;
}

void SipHash_2_4_MAC(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* out) {
    uint64_t h = SipHash_2_4(key, data, len);
    U64TO8_LE(out, h);
}
