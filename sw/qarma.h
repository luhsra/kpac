#ifndef QARMA_H
#define QARMA_H

#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t u8;

typedef u64 tweak_t;
typedef u64 text_t;
typedef u64 qkey_t;

text_t qarma64_enc(text_t plaintext, tweak_t tweak, qkey_t w0, qkey_t k0, int rounds);

text_t qarma64_dec(text_t plaintext, tweak_t tweak, qkey_t w0, qkey_t k0, int rounds);

#endif /* QARMA_H */
