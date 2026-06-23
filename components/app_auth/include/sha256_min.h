#ifndef SHA256_MIN_H
#define SHA256_MIN_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_context_t;

void sha256_init(sha256_context_t *ctx);
void sha256_update(sha256_context_t *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_context_t *ctx, uint8_t hash[32]);

#endif
