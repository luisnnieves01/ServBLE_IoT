/* src/tabla_ls.c
 *
 * Implementación de la tabla L_S en memoria.
 * Reemplaza la base de datos SQLite.
 */
#include "tabla_ls.h"
#include <stdio.h>
#include <string.h>

/* ── Inicializar ─────────────────────────────────────────────────── */
void ls_inicializar(tabla_ls_t *tabla)
{
    memset(tabla, 0, sizeof(*tabla));
}

/* ── Insertar o actualizar ───────────────────────────────────────── */
int ls_insertar(tabla_ls_t *tabla, const registro_ls_t *reg)
{
    /* Buscar si ya existe para actualizar */
    for (int i = 0; i < LS_MAX_DISPOSITIVOS; i++) {
        if (tabla->entradas[i].en_uso &&
            memcmp(tabla->entradas[i].id, reg->id, ID_LEN) == 0) {
            tabla->entradas[i] = *reg;
            tabla->entradas[i].en_uso = 1;
            return 0;
        }
    }

    /* Buscar hueco vacío */
    for (int i = 0; i < LS_MAX_DISPOSITIVOS; i++) {
        if (!tabla->entradas[i].en_uso) {
            tabla->entradas[i] = *reg;
            tabla->entradas[i].en_uso = 1;
            tabla->num_registros++;
            return 0;
        }
    }

    fprintf(stderr, "[LS] Error: tabla llena (%d dispositivos máximo)\n",
            LS_MAX_DISPOSITIVOS);
    return -1;
}

/* ── Buscar por ID ───────────────────────────────────────────────── */
registro_ls_t *ls_buscar(tabla_ls_t *tabla, const uint8_t id[ID_LEN])
{
    for (int i = 0; i < LS_MAX_DISPOSITIVOS; i++) {
        if (tabla->entradas[i].en_uso &&
            memcmp(tabla->entradas[i].id, id, ID_LEN) == 0) {
            return &tabla->entradas[i];
        }
    }
    return NULL;
}

/* ── Mostrar contenido (debug) ───────────────────────────────────── */
void ls_mostrar(const tabla_ls_t *tabla)
{
    printf("\n[LS] ─── Contenido de L_S ───────────────────────────\n");
    printf("     %-34s\n", "ID del dispositivo");

    if (tabla->num_registros == 0) {
        printf("     (vacía)\n");
    } else {
        for (int i = 0; i < LS_MAX_DISPOSITIVOS; i++) {
            if (!tabla->entradas[i].en_uso) continue;
            printf("     ");
            for (int j = 0; j < ID_LEN; j++)
                printf("%02X", tabla->entradas[i].id[j]);
            printf("\n");
        }
    }
    printf("[LS] ─── Total: %d dispositivo(s) ───────────────────\n\n",
           tabla->num_registros);
}
