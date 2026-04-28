/* src/bch_puf/bch_puf.h
 *
 *  Cambios respecto a versión anterior:
 *    BCH_T:         4 → 2    (capacidad de corrección t=2 bits)
 *    BCH_ECC_BYTES: 5 → 2    (t=2, m=8 → ecc_bits=16 → 2 bytes)
 *    BCH_HA_BYTES:  320 → 128 (64 sub-bloques × 2 bytes = 128)
 *
 *  El dispositivo nRF54L15 usa los mismos parámetros.
 *  El helper data que se envía en el Mensaje 1 tiene 128 bytes útiles.
 *  Si el frame MSG1 reserva 320 bytes para h_own, los 192 bytes
 *  restantes deben ser cero (padding) o bien actualizar PUF_HA_BYTES
 *  en puf_ble.h del nRF a 128.
 */
#ifndef BCH_PUF_H
#define BCH_PUF_H

#include <stdint.h>

/* ── Parámetros BCH ─────────────────────────────────────────────── */
#define BCH_M           8    /* GF(2^m)                              */
#define BCH_T           2    /* capacidad de corrección (bits)       */
#define BCH_ECC_BYTES   2    /* bytes de ECC por sub-bloque          *
                              * t=2, m=8 → ecc_bits=16 → 2 bytes    */

/* ── Parámetros del bloque físico PUF ──────────────────────────── */
#define PUF_BLOCK_BYTES 1024 /* 1 bloque completo (bytes)           */
#define PUF_SUB_BYTES   16   /* bytes por sub-bloque                */
#define PUF_NUM_SUBS    (PUF_BLOCK_BYTES / PUF_SUB_BYTES)  /* 64   */

/* ── Tamaños derivados ──────────────────────────────────────────── */
#ifndef BCH_HA_BYTES
#define BCH_HA_BYTES    (PUF_NUM_SUBS * BCH_ECC_BYTES)  /* 128     */
#endif

#define BCH_DATA_BYTES  PUF_SUB_BYTES   /* alias de compatibilidad  */

/* ── Tipos ──────────────────────────────────────────────────────── */
typedef struct {
    uint8_t ecc[BCH_ECC_BYTES];
} puf_helper_t;

/* ── API pública ────────────────────────────────────────────────── */
int  bch_puf_init(void);
void bch_puf_free(void);

int  bch_puf_encode_block(const uint8_t ref_block [PUF_BLOCK_BYTES],
                           const uint8_t mask_block[PUF_BLOCK_BYTES],
                           uint8_t       ha_out    [BCH_HA_BYTES],
                           uint8_t      *r_out);

void bch_puf_select_bits(const uint8_t *ref,
                          const uint8_t *mask,
                          uint8_t       *out);

int  bch_puf_encode(const uint8_t  r_ref  [BCH_DATA_BYTES],
                    const uint8_t  r_noisy[BCH_DATA_BYTES],
                    puf_helper_t  *helper,
                    uint8_t        r_sel  [BCH_DATA_BYTES]);

#endif /* BCH_PUF_H */
