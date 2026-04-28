/* src/registro/pre_enrollment.c
 *
 * Pre-enrollment: registra un dispositivo en L_S calculando
 * el helper data BCH con el bloque de referencia indicado.
 */
#include "pre_enrollment.h"
#include "../bch_puf/bch_puf.h"
#include "../puf_ref/referencia.h"
#include "../puf_ref/mascara.h"

#include <stdio.h>
#include <string.h>

/* Tablas de bloques de referencia y máscara */
static const uint8_t * const ref_bloques[4] = {
    ref_bloque1, ref_bloque2, ref_bloque3, ref_bloque4
};
static const uint8_t * const mask_bloques[4] = {
    mask_bloque1, mask_bloque2, mask_bloque3, mask_bloque4
};

int pre_enrollment_desde_ref(tabla_ls_t    *tabla,
                              const uint8_t  id_disp[ID_LEN],
                              int            bloque_idx)
{
    if (bloque_idx < 0 || bloque_idx > 3) {
        fprintf(stderr, "[PRE-ENROLL] bloque_idx debe ser 0-3\n");
        return -1;
    }

    /* ── 1. Calcular helper data BCH (320 bytes) ─────────────────── */
    uint8_t datos_ha[BCH_HA_BYTES];
    uint8_t bloque_enmascarado[PUF_BLOCK_BYTES]; /* 1024 bytes */

    if (bch_puf_encode_block(ref_bloques[bloque_idx],
                              mask_bloques[bloque_idx],
                              datos_ha,
                              bloque_enmascarado) != 0) {
        fprintf(stderr, "[PRE-ENROLL] Error calculando helper data BCH\n");
        return -1;
    }

    /* ── 2. R_sel = primeros 32 bytes del bloque enmascarado ─────── */
    uint8_t r_sel[RESPUESTA_LEN];
    memcpy(r_sel, bloque_enmascarado, RESPUESTA_LEN);

    /* ── 3. Guardar en L_S ───────────────────────────────────────── */
    registro_ls_t reg;
    memset(&reg, 0, sizeof(reg));
    memcpy(reg.id,        id_disp,  ID_LEN);
    memcpy(reg.datos_ecc, datos_ha, BCH_HA_BYTES);
    memcpy(reg.respuesta, r_sel,    RESPUESTA_LEN);

    if (ls_insertar(tabla, &reg) != 0) {
        fprintf(stderr, "[PRE-ENROLL] Error insertando en L_S\n");
        return -1;
    }

    /* ── Debug ───────────────────────────────────────────────────── */
    printf("[PRE-ENROLL] Dispositivo registrado (bloque %d)\n", bloque_idx);
    printf("[PRE-ENROLL]   ID:    ");
    for (int i = 0; i < ID_LEN; i++) printf("%02X", id_disp[i]);
    printf("\n");
    printf("[PRE-ENROLL]   R_sel: ");
    for (int i = 0; i < RESPUESTA_LEN; i++) printf("%02X", r_sel[i]);
    printf("\n");
    printf("[PRE-ENROLL]   h_a[0..9]: ");
    for (int i = 0; i < 10; i++) printf("%02X ", datos_ha[i]);
    printf("... (%d bytes total)\n", BCH_HA_BYTES);

    /* Limpiar datos sensibles de la pila */
    memset(bloque_enmascarado, 0, sizeof(bloque_enmascarado));
    memset(r_sel, 0, sizeof(r_sel));

    return 0;
}
