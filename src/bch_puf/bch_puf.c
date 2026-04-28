/* src/bch_puf/bch_puf.c
 *
 * Calcula el helper data BCH para un bloque físico completo,
 * operando en 64 sub-bloques de 16 bytes con máscara de estabilidad.
 * Esquema idéntico al del dispositivo nRF54L15 (puf_fuzzy.c).
 */

#include "bch_puf.h"
#include "../lib/bch_codec/bch_codec.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Instancia BCH global */
static struct bch_control *g_bch = NULL;

/* ── Inicializar ─────────────────────────────────────────────────── */
int bch_puf_init(void)
{
    g_bch = init_bch(BCH_M, BCH_T, 0);
    if (!g_bch) {
        fprintf(stderr, "[BCH] Error inicializando BCH(m=%d, t=%d)\n",
                BCH_M, BCH_T);
        return -1;
    }
    printf("[BCH] Inicializado: m=%d t=%d n=%d ecc_bits=%d ecc_bytes=%d\n",
           g_bch->m, g_bch->t, g_bch->n,
           g_bch->ecc_bits, g_bch->ecc_bytes);

    if ((int)g_bch->ecc_bytes > BCH_ECC_BYTES) {
        fprintf(stderr,
            "[BCH] ERROR: ecc_bytes=%d > BCH_ECC_BYTES=%d\n",
            g_bch->ecc_bytes, BCH_ECC_BYTES);
        return -1;
    }
    return 0;
}

/* ── Liberar ─────────────────────────────────────────────────────── */
void bch_puf_free(void)
{
    if (g_bch) { free_bch(g_bch); g_bch = NULL; }
}

/* ── Reducir bits con máscara ────────────────────────────────────── *
 * Replica exactamente reducir_bits() de puf_fuzzy.c del dispositivo:
 * recorre los 8 bits de cada byte; si el bit de máscara es 1 (estable),
 * copia ese bit al buffer de salida en orden.
 * Retorna la cantidad de bits útiles extraídos.
 * ─────────────────────────────────────────────────────────────────── */
static uint16_t reducir_bits(const uint8_t *entrada,
                              const uint8_t *mascara,
                              uint8_t       *salida,
                              uint16_t       tam_bytes)
{
    uint16_t idx_bit = 0;
    memset(salida, 0, tam_bytes);

    for (uint16_t b = 0; b < tam_bytes; b++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (((mascara[b] >> bit) & 1) == 1) {
                uint8_t  val      = (entrada[b] >> bit) & 1;
                uint16_t sal_byte = idx_bit / 8;
                uint8_t  sal_bit  = idx_bit % 8;
                if (val) salida[sal_byte] |= (1u << sal_bit);
                idx_bit++;
            }
        }
    }
    return idx_bit;
}

/* ── bch_puf_encode_block ────────────────────────────────────────── */
int bch_puf_encode_block(const uint8_t  ref_block [PUF_BLOCK_BYTES],
                         const uint8_t  mask_block[PUF_BLOCK_BYTES],
                         uint8_t        ha_out    [BCH_HA_BYTES],
                         uint8_t       *r_out)
{
    if (!g_bch) {
        fprintf(stderr, "[BCH] No inicializado — llama bch_puf_init()\n");
        return -1;
    }

    memset(ha_out, 0, BCH_HA_BYTES);
    if (r_out) memset(r_out, 0, PUF_BLOCK_BYTES);

    for (uint32_t sub = 0; sub < PUF_NUM_SUBS; sub++) {

        const uint8_t *sub_ref  = &ref_block [sub * PUF_SUB_BYTES];
        const uint8_t *sub_mask = &mask_block[sub * PUF_SUB_BYTES];

        /* ── Aplicar máscara: replica reducir_bits() del dispositivo */
        uint8_t bits_estables[PUF_SUB_BYTES];
        uint16_t bits_utiles = reducir_bits(sub_ref, sub_mask,
                                            bits_estables, PUF_SUB_BYTES);
        uint16_t bytes_utiles = (bits_utiles + 7u) / 8u;

        /* ── Calcular ECC sobre los bits enmascarados */
        uint8_t *ecc_dest = &ha_out[sub * BCH_ECC_BYTES];
        memset(ecc_dest, 0, BCH_ECC_BYTES);
        encode_bch(g_bch, bits_estables, bytes_utiles, ecc_dest);

        /* ── Guardar sub-bloque enmascarado en r_out si se pide */
        if (r_out) {
            memcpy(&r_out[sub * PUF_SUB_BYTES], bits_estables, PUF_SUB_BYTES);
        }
    }

    printf("[BCH] Helper data calculado: %d sub-bloques × %d bytes = %d bytes\n",
           PUF_NUM_SUBS, BCH_ECC_BYTES, BCH_HA_BYTES);
    return 0;
}
