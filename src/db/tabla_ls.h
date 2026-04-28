/* src/tabla_ls.h
 *
 * Tabla L_S del servidor en memoria (sin base de datos).
 * Almacena los registros de pre-enrollment de cada dispositivo.
 */
#ifndef TABLA_LS_H
#define TABLA_LS_H

#include <stdint.h>
#include "bch_puf/bch_puf.h"

/* ── Constantes ─────────────────────────────────────────────────── */
#define ID_LEN        16   /* bytes del identificador de dispositivo */
#define RESPUESTA_LEN 32   /* bytes de R_sel almacenado en L_S       */
#define HASH_LEN      32   /* salida de ASCON-Hash (256 bits)        */
#define DESAFIO_LEN    1   /* 1 byte: índice de bloque ∈ {0,1,2,3}  */

#define LS_MAX_DISPOSITIVOS 16  /* máximo de dispositivos registrados */

/* ── Registro de un dispositivo en L_S ──────────────────────────── */
typedef struct {
    uint8_t id        [ID_LEN];         /* identificador del dispositivo  */
    uint8_t datos_ecc [BCH_HA_BYTES];   /* helper data BCH (320 bytes)    */
    uint8_t respuesta [RESPUESTA_LEN];  /* R_sel (32 bytes)               */
    uint8_t en_uso;                     /* 1 = ocupado, 0 = vacío         */
} registro_ls_t;

/* ── Tabla completa en memoria ──────────────────────────────────── */
typedef struct {
    registro_ls_t entradas[LS_MAX_DISPOSITIVOS];
    int           num_registros;
} tabla_ls_t;

/* ── API pública ────────────────────────────────────────────────── */

/* Inicializa la tabla (pone todo a cero) */
void ls_inicializar(tabla_ls_t *tabla);

/* Inserta o actualiza un registro. Retorna 0 si OK, -1 si tabla llena */
int ls_insertar(tabla_ls_t *tabla, const registro_ls_t *reg);

/* Busca un dispositivo por ID. Retorna puntero al registro o NULL */
registro_ls_t *ls_buscar(tabla_ls_t *tabla, const uint8_t id[ID_LEN]);

/* Muestra el contenido de la tabla (debug) */
void ls_mostrar(const tabla_ls_t *tabla);

#endif /* TABLA_LS_H */
