/* src/cifrador_ascon/ascon_ccm.c
 *
 * Implementación de ASCON-128 AEAD.
 * Sigue la especificación oficial: https://ascon.iaik.tugraz.at
 *
 * Estado interno: [K0|K1, N0|N1, S0, S1, S2] — 5 palabras de 64 bits.
 *
 * Fase de inicialización:
 *   S = P12( IV || K || N )
 *   S[3] ^= K[0];  S[4] ^= K[1]
 *
 * Absorción de AD (bloques de 8 bytes, Pb=6 rondas):
 *   Por cada bloque: S[0] ^= bloque; P6(S)
 *   Tras el último bloque: S[4] ^= 1  (separador de dominio)
 *
 * Cifrado (bloques de 8 bytes):
 *   Por cada bloque: C[i] = S[0] ^ P[i]; S[0] = C[i]; P6(S)
 *   Último bloque parcial: XOR solo los bytes relevantes + padding
 *
 * Finalización:
 *   S[1] ^= K[0]; S[2] ^= K[1]
 *   P12(S)
 *   Tag = (S[3] ^ K[0]) || (S[4] ^ K[1])
 */

#include "ascon_ccm.h"
#include <string.h>

/* Incluimos las primitivas de lib/ascon */
#include "ascon.h"
#include "permutations.h"
#include "word.h"
#include "constants.h"

/* IV de ASCON-128 según la especificación */
/* k=128, r=64, a=12, b=6 → IV = 0x80400c0600000000 */
#define ASCON128_IV  UINT64_C(0x80400c0600000000)

/* ── Carga de 64 bits big-endian ─────────────────────────────────── */
static inline uint64_t load64be(const uint8_t *p)
{
    uint64_t x = 0;
    for (int i = 0; i < 8; i++)
        x = (x << 8) | p[i];
    return x;
}

/* ── Almacenamiento de 64 bits big-endian ────────────────────────── */
static inline void store64be(uint8_t *p, uint64_t x)
{
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(x & 0xFF);
        x >>= 8;
    }
}

/* ── Carga parcial (n < 8 bytes) big-endian con padding ─────────── */
static inline uint64_t load_partial(const uint8_t *p, int n)
{
    uint64_t x = 0;
    for (int i = 0; i < n; i++)
        x = (x << 8) | p[i];
    /* Padding: bit 1 justo después del último byte */
    x |= (uint64_t)0x80 << (8 * (7 - n));
    return x;
}

/* ── Inicialización del estado ASCON-128 ─────────────────────────── */
static void ascon128_init(ascon_state_t *s,
                          const uint8_t key[16],
                          const uint8_t nonce[16])
{
    uint64_t K0 = load64be(key);
    uint64_t K1 = load64be(key + 8);
    uint64_t N0 = load64be(nonce);
    uint64_t N1 = load64be(nonce + 8);

    s->x[0] = ASCON128_IV;
    s->x[1] = K0;
    s->x[2] = K1;
    s->x[3] = N0;
    s->x[4] = N1;

    P12(s);

    s->x[3] ^= K0;
    s->x[4] ^= K1;
}

/* ── Absorción de datos asociados (AD) ───────────────────────────── */
static void ascon128_absorb_ad(ascon_state_t *s,
                               const uint8_t *ad, uint32_t ad_len)
{
    if (ad_len == 0) {
        /* Sin AD: solo el separador de dominio */
        s->x[4] ^= 1ULL;
        return;
    }

    /* Bloques completos de 8 bytes */
    while (ad_len >= 8) {
        s->x[0] ^= load64be(ad);
        P6(s);
        ad     += 8;
        ad_len -= 8;
    }

    /* Último bloque parcial (con padding) */
    s->x[0] ^= load_partial(ad, (int)ad_len);
    P6(s);

    /* Separador de dominio */
    s->x[4] ^= 1ULL;
}

/* ── Cifrado del plaintext ───────────────────────────────────────── */
static void ascon128_encrypt(ascon_state_t *s,
                             const uint8_t *pt, uint32_t pt_len,
                             uint8_t       *ct)
{
    /* Bloques completos */
    while (pt_len >= 8) {
        s->x[0] ^= load64be(pt);
        store64be(ct, s->x[0]);
        P6(s);
        pt     += 8;
        ct     += 8;
        pt_len -= 8;
    }

    /* Último bloque parcial */
    if (pt_len > 0) {
        /* Cargar bytes restantes del plaintext */
        uint64_t last = 0;
        for (uint32_t i = 0; i < pt_len; i++)
            last = (last << 8) | pt[i];
        /* Padding */
        last |= (uint64_t)0x80 << (8 * (7 - pt_len));

        s->x[0] ^= last;

        /* Extraer solo los bytes cifrados (sin el padding) */
        for (uint32_t i = 0; i < pt_len; i++) {
            ct[i] = (uint8_t)(s->x[0] >> (8 * (7 - i)));
        }
    }
}

/* ── Descifrado del ciphertext ───────────────────────────────────── */
static void ascon128_decrypt(ascon_state_t *s,
                             const uint8_t *ct, uint32_t ct_len,
                             uint8_t       *pt)
{
    while (ct_len >= 8) {
        uint64_t c = load64be(ct);
        uint64_t p = s->x[0] ^ c;
        store64be(pt, p);
        s->x[0] = c;
        P6(s);
        ct     += 8;
        pt     += 8;
        ct_len -= 8;
    }

    if (ct_len > 0) {
        uint64_t c = 0;
        for (uint32_t i = 0; i < ct_len; i++)
            c = (c << 8) | ct[i];

        uint64_t pad = (uint64_t)0x80 << (8 * (7 - ct_len));
        uint64_t p   = (s->x[0] ^ c);

        /* Recuperar solo los bytes de plaintext */
        for (uint32_t i = 0; i < ct_len; i++)
            pt[i] = (uint8_t)(p >> (8 * (7 - i)));

        /* Actualizar estado: reemplazar bytes cifrados en s->x[0] */
        uint64_t mask = ~( ((uint64_t)1 << (8*(8-ct_len))) - 1 );
        s->x[0] = (s->x[0] & ~mask) | (c << (8*(8-ct_len)));
        s->x[0] ^= pad;
    }
}

/* ── Finalización: genera el tag ─────────────────────────────────── */
static void ascon128_finalize(ascon_state_t *s,
                              const uint8_t key[16],
                              uint8_t tag[16])
{
    uint64_t K0 = load64be(key);
    uint64_t K1 = load64be(key + 8);

    s->x[1] ^= K0;
    s->x[2] ^= K1;

    P12(s);

    tag[0] = 0; /* aseguramos limpieza */
    store64be(tag,     s->x[3] ^ K0);
    store64be(tag + 8, s->x[4] ^ K1);
}

/* ═══════════════════════════════════════════════════════════════════
 * API pública
 * ═══════════════════════════════════════════════════════════════════ */

int ascon_ccm_enc(const uint8_t key[ASCON_KEY_LEN],
                  const uint8_t nonce[ASCON_NONCE_LEN],
                  const uint8_t *ad,  uint32_t ad_len,
                  const uint8_t *pt,  uint32_t pt_len,
                  uint8_t       *ct_out)
{
    ascon_state_t s;

    ascon128_init     (&s, key, nonce);
    ascon128_absorb_ad(&s, ad, ad_len);
    ascon128_encrypt  (&s, pt, pt_len, ct_out);
    ascon128_finalize (&s, key, ct_out + pt_len);  /* tag al final */

    memset(&s, 0, sizeof(s));
    return (int)(pt_len + ASCON_TAG_LEN);
}

int ascon_ccm_dec(const uint8_t key[ASCON_KEY_LEN],
                  const uint8_t nonce[ASCON_NONCE_LEN],
                  const uint8_t *ad,    uint32_t ad_len,
                  const uint8_t *ct,    uint32_t ct_len,
                  uint8_t       *pt_out)
{
    if (ct_len < ASCON_TAG_LEN) return -1;

    uint32_t pt_len = ct_len - ASCON_TAG_LEN;
    ascon_state_t s;

    ascon128_init     (&s, key, nonce);
    ascon128_absorb_ad(&s, ad, ad_len);
    ascon128_decrypt  (&s, ct, pt_len, pt_out);

    uint8_t tag_computed[ASCON_TAG_LEN];
    ascon128_finalize(&s, key, tag_computed);

    /* Comparación en tiempo constante */
    uint8_t diff = 0;
    for (int i = 0; i < ASCON_TAG_LEN; i++)
        diff |= tag_computed[i] ^ ct[ct_len - ASCON_TAG_LEN + i];

    memset(&s, 0, sizeof(s));
    if (diff != 0) {
        memset(pt_out, 0, pt_len);
        return -1;
    }
    return (int)pt_len;
}
