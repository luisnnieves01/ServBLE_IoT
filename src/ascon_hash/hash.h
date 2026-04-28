/* src/ascon_hash/hash.h */
#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include "../db/tabla_ls.h"  /* para HASH_LEN = 32 */

/*
 * H(data, len) → digest[32]
 * Usa ASCON-Hash internamente.
 */
void ascon_hash_buf(const uint8_t *data, uint32_t len,
                    uint8_t digest[HASH_LEN]);

/*
 * H(a || b) → digest[32]   (concatenación sin buffer extra)
 */
void ascon_hash_2(const uint8_t *a, uint32_t la,
                  const uint8_t *b, uint32_t lb,
                  uint8_t digest[HASH_LEN]);

/*
 * XOR de dos buffers de longitud len → out
 */
void buf_xor(const uint8_t *a, const uint8_t *b,
             uint32_t len, uint8_t *out);

#endif /* HASH_H */
