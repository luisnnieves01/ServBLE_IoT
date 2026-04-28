/* src/registro/pre_enrollment.h */
#ifndef PRE_ENROLLMENT_H
#define PRE_ENROLLMENT_H

#include <stdint.h>
#include "../db/tabla_ls.h"

/*
 * pre_enrollment_desde_ref
 *
 * Registra un dispositivo en la tabla L_S calculando el helper data
 * BCH (320 bytes) desde los bloques de referencia y máscara compilados
 * en el servidor.
 *
 * Parámetros:
 *   tabla      → tabla L_S donde se guarda el registro
 *   id_disp    → ID del dispositivo (ID_LEN = 16 bytes)
 *   bloque_idx → índice del bloque PUF a usar (0-3)
 *
 * Retorna 0 si OK, -1 si error.
 */
int pre_enrollment_desde_ref(tabla_ls_t    *tabla,
                              const uint8_t  id_disp[ID_LEN],
                              int            bloque_idx);

#endif /* PRE_ENROLLMENT_H */
