/* src/ascon_hash/hash.c */
#include "hash.h"
#include <string.h>

/*
 * Reutilizamos la función crypto_hash() de lib/ascon/hash.c
 * que ya implementa ASCON-Hash correctamente.
 * Su firma es:
 *   int crypto_hash(unsigned char *out,
 *                   const unsigned char *in,
 *                   unsigned long long len);
 */
extern int crypto_hash(unsigned char *out,
                       const unsigned char *in,
                       unsigned long long len);

/* ── Hash simple ─────────────────────────────────────────────────── */
void ascon_hash_buf(const uint8_t *data, uint32_t len,
                    uint8_t digest[HASH_LEN])
{
    crypto_hash(digest, data, (unsigned long long)len);
}

/* ── Hash de concatenación sin buffer extra ──────────────────────── */
/*
 * Para no malloc, usamos el estado interno: llamamos a la función de
 * absorción incremental. Como lib/ascon solo expone la API de una
 * pasada, aquí copiamos los datos en un buffer de tamaño máximo
 * conocido (RESPONSE_LEN + ID_LEN = 32+16 = 48 bytes máximo).
 */
void ascon_hash_2(const uint8_t *a, uint32_t la,
                  const uint8_t *b, uint32_t lb,
                  uint8_t digest[HASH_LEN])
{
    /* Buffer temporal para la concatenación */
    uint8_t tmp[256];
    if (la + lb > sizeof(tmp)) {
        /* Fallback seguro: nunca debería ocurrir con nuestros tamaños */
        memset(digest, 0, HASH_LEN);
        return;
    }
    memcpy(tmp,      a, la);
    memcpy(tmp + la, b, lb);
    crypto_hash(digest, tmp, (unsigned long long)(la + lb));
    memset(tmp, 0, la + lb);
}

/* ── XOR de buffers ──────────────────────────────────────────────── */
void buf_xor(const uint8_t *a, const uint8_t *b,
             uint32_t len, uint8_t *out)
{
    for (uint32_t i = 0; i < len; i++)
        out[i] = a[i] ^ b[i];
}
