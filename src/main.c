/* src/main.c — Servidor PUF-BLE: pre-enrollment + Mensaje 1
 *  Luiso
 * Flujo según el protocolo:
 *   1. Inicializar BCH y tabla L_S en memoria
 *   2. Pre-enrollment de los dos dispositivos (si no están ya)
 *   3. Elegir desafíos aleatorios C_a y C_b ∈ {0,1,2,3}
 *   4. Calcular helper data h_a y h_b para los desafíos elegidos
 *   5. Generar nonces aleatorios m (para A) y n (para B)
 *   6. Construir Mensaje 1:
 *        msg1_A = <ID_A, D_A, ID_B, D_B, h_a, m>  (190 bytes)
 *        msg1_B = <ID_B, D_B, ID_A, D_A, h_b, n>  (190 bytes)
 *   7. TODO: transmitir por BLE (siguiente fase)
 *
 * Formato de M1 (190 bytes):
 *   [ID_propio:3][D_propio:4][ID_ajeno:3][D_ajeno:4][h_propio:160][nonce:16]
 *
 * Nota: los tamaños exactos dependen de BCH_HA_BYTES e ID_LEN.
 *       Ajusta los #define en tabla_ls.h y bch_puf.h si cambian.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "db/tabla_ls.h"
#include "bch_puf/bch_puf.h"
#include "registro/pre_enrollment.h"
#include "puf_ref/referencia.h"
#include "puf_ref/mascara.h"

/* ── Parámetros del Mensaje 1 ───────────────────────────────────── */
#define NONCE_BYTES  16
#define MSG1_TOTAL   (ID_LEN + ID_LEN + BCH_HA_BYTES + NONCE_BYTES)

/* Offsets dentro del mensaje */
#define OFF_ID_PROPIO  0
#define OFF_ID_AJENO   (OFF_ID_PROPIO + ID_LEN)
#define OFF_HELPER     (OFF_ID_AJENO  + ID_LEN)
#define OFF_NONCE      (OFF_HELPER    + BCH_HA_BYTES)

/* Bloques de referencia y máscara para cada desafío */
static const uint8_t * const ref_bloques[4]  = {
    ref_bloque1, ref_bloque2, ref_bloque3, ref_bloque4
};
static const uint8_t * const mask_bloques[4] = {
    mask_bloque1, mask_bloque2, mask_bloque3, mask_bloque4
};

/* ── Utilidades ─────────────────────────────────────────────────── */

/* Genera 'len' bytes aleatorios usando /dev/urandom */
static void generar_nonce(uint8_t *salida, size_t len)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (f && fread(salida, 1, len, f) == len) {
        fclose(f);
        return;
    }
    if (f) fclose(f);
    fprintf(stderr, "[AVISO] Usando rand() para nonce (solo para pruebas)\n");
    for (size_t i = 0; i < len; i++)
        salida[i] = (uint8_t)(rand() & 0xFF);
}

/* Elige un desafío aleatorio ∈ {0,1,2,3} */
static uint8_t elegir_desafio(void)
{
    uint8_t c;
    generar_nonce(&c, 1);
    return c % 4;
}

/* Construye el Mensaje 1 en el buffer 'salida' */
static void construir_msg1(const uint8_t id_propio[ID_LEN],
                            const uint8_t id_ajeno [ID_LEN],
                            const uint8_t helper   [BCH_HA_BYTES],
                            const uint8_t nonce    [NONCE_BYTES],
                            uint8_t       salida   [MSG1_TOTAL])
{
    memcpy(salida + OFF_ID_PROPIO, id_propio, ID_LEN);
    memcpy(salida + OFF_ID_AJENO,  id_ajeno,  ID_LEN);
    memcpy(salida + OFF_HELPER,    helper,    BCH_HA_BYTES);
    memcpy(salida + OFF_NONCE,     nonce,     NONCE_BYTES);
}

/* Muestra el contenido del Mensaje 1 por pantalla */
static void mostrar_msg1(const char *destino, const uint8_t msg[MSG1_TOTAL])
{
    printf("\n══════════════════════════════════════════════\n");
    printf(" Mensaje 1 → %s (%d bytes)\n", destino, MSG1_TOTAL);
    printf("══════════════════════════════════════════════\n");

    printf(" ID propio:  ");
    for (int i = 0; i < ID_LEN; i++) printf("%02X", msg[OFF_ID_PROPIO + i]);
    printf("\n");

    printf(" ID ajeno:   ");
    for (int i = 0; i < ID_LEN; i++) printf("%02X", msg[OFF_ID_AJENO + i]);
    printf("\n");

    printf(" h[0..9]:    ");
    for (int i = 0; i < 10; i++) printf("%02X ", msg[OFF_HELPER + i]);
    printf("... (%d bytes)\n", BCH_HA_BYTES);

    printf(" nonce:      ");
    for (int i = 0; i < NONCE_BYTES; i++) printf("%02X", msg[OFF_NONCE + i]);
    printf("\n");

    printf("══════════════════════════════════════════════\n");
}

/* ═══════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════ */
int main(void)
{
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   Servidor PUF-BLE — Pre-enrollment + M1    ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* ── 1. Inicializar BCH y tabla L_S ─────────────────────────── */
    if (bch_puf_init() != 0) {
        fprintf(stderr, "[ERROR] No se pudo inicializar BCH\n");
        return 1;
    }

    tabla_ls_t tabla_ls;
    ls_inicializar(&tabla_ls);

    /* ── 2. IDs de los dispositivos (16 bytes cada uno) ─────────── */
    const uint8_t ID_A[ID_LEN] = {
        0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    const uint8_t ID_B[ID_LEN] = {
        0xBB, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    /* ── 3. Pre-enrollment (registra en L_S si no están ya) ─────── */
    if (ls_buscar(&tabla_ls, ID_A) == NULL) {
        printf("[SETUP] Pre-enrollando dispositivo A...\n");
        if (pre_enrollment_desde_ref(&tabla_ls, ID_A, 0) != 0) goto error;
    } else {
        printf("[SETUP] Dispositivo A ya en L_S.\n");
    }

    if (ls_buscar(&tabla_ls, ID_B) == NULL) {
        printf("[SETUP] Pre-enrollando dispositivo B...\n");
        if (pre_enrollment_desde_ref(&tabla_ls, ID_B, 0) != 0) goto error;
    } else {
        printf("[SETUP] Dispositivo B ya en L_S.\n");
    }

    ls_mostrar(&tabla_ls);

    /* ── 4. Elegir desafíos aleatorios ───────────────────────────── */
    uint8_t desafio_a = elegir_desafio();
    uint8_t desafio_b = elegir_desafio();
    printf("[MSG1] Desafío A = %u  Desafío B = %u\n",
           (unsigned)desafio_a, (unsigned)desafio_b);

    /* ── 5. Calcular helper data para cada desafío ───────────────── */
    uint8_t h_a[BCH_HA_BYTES];
    uint8_t h_b[BCH_HA_BYTES];

    printf("[MSG1] Calculando h_a (bloque %u)...\n", (unsigned)desafio_a);
    if (bch_puf_encode_block(ref_bloques[desafio_a],
                              mask_bloques[desafio_a],
                              h_a, NULL) != 0) goto error;

    printf("[MSG1] Calculando h_b (bloque %u)...\n", (unsigned)desafio_b);
    if (bch_puf_encode_block(ref_bloques[desafio_b],
                              mask_bloques[desafio_b],
                              h_b, NULL) != 0) goto error;

    /* ── 6. Generar nonces ───────────────────────────────────────── */
    uint8_t nonce_m[NONCE_BYTES]; /* nonce para dispositivo A */
    uint8_t nonce_n[NONCE_BYTES]; /* nonce para dispositivo B */
    generar_nonce(nonce_m, NONCE_BYTES);
    generar_nonce(nonce_n, NONCE_BYTES);

    /* ── 7. Construir Mensaje 1 para cada dispositivo ────────────── */
    uint8_t msg1_a[MSG1_TOTAL];
    uint8_t msg1_b[MSG1_TOTAL];

    construir_msg1(ID_A, ID_B, h_a, nonce_m, msg1_a);
    construir_msg1(ID_B, ID_A, h_b, nonce_n, msg1_b);

    mostrar_msg1("Dispositivo A", msg1_a);
    mostrar_msg1("Dispositivo B", msg1_b);

    printf("\n[MAIN] Mensajes listos.\n");
    printf("[MAIN]   Tamaño por mensaje : %d bytes\n", MSG1_TOTAL);
    printf("[MAIN]   TODO: enviar por BLE a cada dispositivo.\n");

    /* Limpiar datos sensibles */
    memset(h_a,     0, BCH_HA_BYTES);
    memset(h_b,     0, BCH_HA_BYTES);
    memset(nonce_m, 0, NONCE_BYTES);
    memset(nonce_n, 0, NONCE_BYTES);

    bch_puf_free();
    return 0;

error:
    bch_puf_free();
    return 1;
}
