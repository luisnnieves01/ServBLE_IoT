/* src/ble/ble_server_setup.c
 *
 * Servidor PUF-BLE: pre-enrollment + construcción + envío del Mensaje 1.
 *
 * Flujo:
 *   1. Inicializar BCH y tabla L_S en memoria
 *   2. Pre-enrollment de los dispositivos A y B
 *   3. Elegir desafíos aleatorios C_a, C_b ∈ {0,1,2,3}
 *   4. Calcular helper data h_a, h_b
 *   5. Generar nonces m (para A) y n (para B)
 *   6. Construir Mensaje 1 para cada dispositivo
 *   7. Enviar por BLE vía BlueZ D-Bus
 */

#define _DEFAULT_SOURCE   /* usleep(), strncasecmp() con -std=c11 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include "../db/tabla_ls.h"
#include "../bch_puf/bch_puf.h"
#include "../registro/pre_enrollment.h"
#include "../puf_ref/referencia.h"
#include "../puf_ref/mascara.h"
#include "ble_config.h"   /* BLE_ADAPTER, MAC_A, MAC_B, GATT_MSG1_UUID */

/* ── Parámetros del Mensaje 1 ────────────────────────────────────── *
 * Formato: [ID_propio:16][ID_ajeno:16][helper:BCH_HA_BYTES][nonce:16] */
#define NONCE_LEN    16
#define MSG1_TOTAL   (ID_LEN + ID_LEN + BCH_HA_BYTES + NONCE_LEN)

#define OFF_ID_PROPIO  0
#define OFF_ID_AJENO   (OFF_ID_PROPIO + ID_LEN)
#define OFF_HELPER     (OFF_ID_AJENO  + ID_LEN)
#define OFF_NONCE      (OFF_HELPER    + BCH_HA_BYTES)

/*
 * NUS sobre nRF54L15 negocia MTU 247 → 244 bytes útiles por fragmento.
 */
#define BLE_FRAG_SIZE          244
#define BLE_CONNECT_TIMEOUT_MS 10000   /* 10 s para Connect           */
#define BLE_PROP_TIMEOUT_MS    3000    /* 3 s para leer propiedades   */

/* Bloques de referencia y máscara para cada desafío */
static const uint8_t * const ref_bloques[4]  = {
    ref_bloque1, ref_bloque2, ref_bloque3, ref_bloque4
};
static const uint8_t * const mask_bloques[4] = {
    mask_bloque1, mask_bloque2, mask_bloque3, mask_bloque4
};

/* ═══════════════════════════════════════════════════════════════════
 * Utilidades
 * ═══════════════════════════════════════════════════════════════════ */

static void generar_nonce(uint8_t *salida, size_t len)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t r = fread(salida, 1, len, f);
        fclose(f);
        if (r == len) return;
    }
    fprintf(stderr, "[AVISO] /dev/urandom falló — usando rand()\n");
    for (size_t i = 0; i < len; i++) salida[i] = (uint8_t)(rand() & 0xFF);
}

static uint8_t elegir_desafio(void)
{
    uint8_t c;
    generar_nonce(&c, 1);
    return c % 4;
}

/* ═══════════════════════════════════════════════════════════════════
 * Construcción del Mensaje 1
 * ═══════════════════════════════════════════════════════════════════ */

static void construir_msg1(const uint8_t id_propio[ID_LEN],
                            const uint8_t id_ajeno [ID_LEN],
                            const uint8_t helper   [BCH_HA_BYTES],
                            const uint8_t nonce    [NONCE_LEN],
                            uint8_t       salida   [MSG1_TOTAL])
{
    memcpy(salida + OFF_ID_PROPIO, id_propio, ID_LEN);
    memcpy(salida + OFF_ID_AJENO,  id_ajeno,  ID_LEN);
    memcpy(salida + OFF_HELPER,    helper,    BCH_HA_BYTES);
    memcpy(salida + OFF_NONCE,     nonce,     NONCE_LEN);
}

static void mostrar_msg1(const char *destino, const uint8_t msg[MSG1_TOTAL])
{
    printf("\n══════════════════════════════════════════════\n");
    printf(" Mensaje 1 → %s (%d bytes)\n", destino, MSG1_TOTAL);
    printf("══════════════════════════════════════════════\n");
    printf(" ID propio : ");
    for (int i = 0; i < ID_LEN; i++) printf("%02X", msg[OFF_ID_PROPIO + i]);
    printf("\n ID ajeno  : ");
    for (int i = 0; i < ID_LEN; i++) printf("%02X", msg[OFF_ID_AJENO + i]);
    printf("\n h[0..11]  : ");
    for (int i = 0; i < 12; i++) printf("%02X ", msg[OFF_HELPER + i]);
    printf("... (%d bytes)\n", BCH_HA_BYTES);
    printf(" nonce     : ");
    for (int i = 0; i < NONCE_LEN; i++) printf("%02X", msg[OFF_NONCE + i]);
    printf("\n══════════════════════════════════════════════\n");
}

/* ═══════════════════════════════════════════════════════════════════
 * Capa BLE — BlueZ D-Bus
 * ═══════════════════════════════════════════════════════════════════ */

/* Convierte "EE:3B:74:FF:F2:F3" → "/org/bluez/hci0/dev_EE_3B_74_FF_F2_F3" */
static void mac_a_ruta_dbus(const char *mac, char *salida, size_t len)
{
    char m[18];
    strncpy(m, mac, sizeof(m) - 1);
    m[17] = '\0';
    for (int i = 0; m[i]; i++)
        if (m[i] == ':') m[i] = '_';
    snprintf(salida, len, "/org/bluez/%s/dev_%s", BLE_ADAPTER, m);
}

/* Leer propiedad booleana D-Bus de org.bluez.Device1 */
static int ble_leer_prop_bool(DBusConnection *conn,
                               const char *ruta_disp,
                               const char *nombre_prop,
                               int        *valor)
{
    DBusError err;
    dbus_error_init(&err);

    DBusMessage *msg = dbus_message_new_method_call(
        "org.bluez", ruta_disp,
        "org.freedesktop.DBus.Properties", "Get");
    if (!msg) return -1;

    const char *iface = "org.bluez.Device1";
    dbus_message_append_args(msg,
        DBUS_TYPE_STRING, &iface,
        DBUS_TYPE_STRING, &nombre_prop,
        DBUS_TYPE_INVALID);

    DBusMessage *resp = dbus_connection_send_with_reply_and_block(
        conn, msg, BLE_PROP_TIMEOUT_MS, &err);
    dbus_message_unref(msg);

    if (!resp || dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        return -1;
    }

    DBusMessageIter raiz, var;
    dbus_message_iter_init(resp, &raiz);
    dbus_message_iter_recurse(&raiz, &var);
    dbus_bool_t val = FALSE;
    dbus_message_iter_get_basic(&var, &val);
    dbus_message_unref(resp);

    *valor = (int)val;
    return 0;
}

/* Conectar solo si no está ya conectado */
static int ble_conectar_si_hace_falta(DBusConnection *conn,
                                       const char *ruta_disp)
{
    int conectado = 0;
    if (ble_leer_prop_bool(conn, ruta_disp, "Connected", &conectado) == 0
        && conectado)
    {
        printf("[BLE] Ya conectado a %s — se omite Connect()\n", ruta_disp);
        return 0;
    }

    printf("[BLE] Conectando a %s ...\n", ruta_disp);
    DBusError err;
    dbus_error_init(&err);

    DBusMessage *msg = dbus_message_new_method_call(
        "org.bluez", ruta_disp, "org.bluez.Device1", "Connect");
    if (!msg) return -1;

    DBusMessage *resp = dbus_connection_send_with_reply_and_block(
        conn, msg, BLE_CONNECT_TIMEOUT_MS, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "[BLE] Error en Connect: %s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    if (resp) dbus_message_unref(resp);
    printf("[BLE] Conectado\n");
    return 0;
}

/* Buscar ruta D-Bus de una característica GATT por UUID */
static int buscar_ruta_caracteristica(DBusConnection *conn,
                                       const char *ruta_disp,
                                       const char *uuid,
                                       char *salida, size_t len_salida)
{
    DBusError err;
    dbus_error_init(&err);

    DBusMessage *msg = dbus_message_new_method_call(
        "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!msg) return -1;

    DBusMessage *resp = dbus_connection_send_with_reply_and_block(
        conn, msg, 5000, &err);
    dbus_message_unref(msg);

    if (!resp || dbus_error_is_set(&err)) {
        fprintf(stderr, "[BLE] GetManagedObjects: %s\n",
                dbus_error_is_set(&err) ? err.message : "sin respuesta");
        dbus_error_free(&err);
        return -1;
    }

    DBusMessageIter raiz, objetos;
    dbus_message_iter_init(resp, &raiz);
    dbus_message_iter_recurse(&raiz, &objetos);

    int encontrado = 0;
    while (!encontrado &&
           dbus_message_iter_get_arg_type(&objetos) == DBUS_TYPE_DICT_ENTRY)
    {
        DBusMessageIter entrada, ifaces;
        dbus_message_iter_recurse(&objetos, &entrada);

        const char *ruta_obj = NULL;
        dbus_message_iter_get_basic(&entrada, &ruta_obj);
        dbus_message_iter_next(&entrada);

        if (!ruta_obj ||
            strncmp(ruta_obj, ruta_disp, strlen(ruta_disp)) != 0) {
            dbus_message_iter_next(&objetos);
            continue;
        }

        dbus_message_iter_recurse(&entrada, &ifaces);
        while (!encontrado &&
               dbus_message_iter_get_arg_type(&ifaces) == DBUS_TYPE_DICT_ENTRY)
        {
            DBusMessageIter ie, props;
            dbus_message_iter_recurse(&ifaces, &ie);

            const char *iface = NULL;
            dbus_message_iter_get_basic(&ie, &iface);

            if (iface &&
                strcmp(iface, "org.bluez.GattCharacteristic1") == 0)
            {
                dbus_message_iter_next(&ie);
                dbus_message_iter_recurse(&ie, &props);

                while (dbus_message_iter_get_arg_type(&props)
                       == DBUS_TYPE_DICT_ENTRY)
                {
                    DBusMessageIter pe, pv;
                    dbus_message_iter_recurse(&props, &pe);
                    const char *nombre_prop = NULL;
                    dbus_message_iter_get_basic(&pe, &nombre_prop);
                    dbus_message_iter_next(&pe);

                    if (nombre_prop && strcmp(nombre_prop, "UUID") == 0) {
                        dbus_message_iter_recurse(&pe, &pv);
                        const char *u = NULL;
                        dbus_message_iter_get_basic(&pv, &u);
                        if (u && strncasecmp(u, uuid, 37) == 0) {
                            strncpy(salida, ruta_obj, len_salida - 1);
                            salida[len_salida - 1] = '\0';
                            encontrado = 1;
                        }
                    }
                    dbus_message_iter_next(&props);
                }
            }
            dbus_message_iter_next(&ifaces);
        }
        dbus_message_iter_next(&objetos);
    }
    dbus_message_unref(resp);
    return encontrado ? 0 : -1;
}

/* Escribir un fragmento en la característica GATT (Write Without Response) */
static int escribir_fragmento(DBusConnection *conn,
                               const char *ruta_carac,
                               const uint8_t *datos, uint16_t len,
                               uint16_t offset)
{
    DBusMessage *msg = dbus_message_new_method_call(
        "org.bluez", ruta_carac,
        "org.bluez.GattCharacteristic1", "WriteValue");
    if (!msg) return -1;

    DBusMessageIter args, arr, dict, ent, var;
    dbus_message_iter_init_append(msg, &args);

    /* Primer parámetro: array de bytes */
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                                     DBUS_TYPE_BYTE_AS_STRING, &arr);
    for (uint16_t i = 0; i < len; i++)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_BYTE, &datos[i]);
    dbus_message_iter_close_container(&args, &arr);

    /* Segundo parámetro: opciones {sv} */
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    /* "type" = "command"  →  Write Without Response */
    {
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &ent);
        const char *k = "type";
        dbus_message_iter_append_basic(&ent, DBUS_TYPE_STRING, &k);
        dbus_message_iter_open_container(&ent, DBUS_TYPE_VARIANT,
                                         DBUS_TYPE_STRING_AS_STRING, &var);
        const char *v = "command";
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
        dbus_message_iter_close_container(&ent, &var);
        dbus_message_iter_close_container(&dict, &ent);
    }

    /* "offset" = posición dentro del mensaje completo */
    {
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &ent);
        const char *k = "offset";
        dbus_message_iter_append_basic(&ent, DBUS_TYPE_STRING, &k);
        dbus_message_iter_open_container(&ent, DBUS_TYPE_VARIANT,
                                         DBUS_TYPE_UINT16_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT16, &offset);
        dbus_message_iter_close_container(&ent, &var);
        dbus_message_iter_close_container(&dict, &ent);
    }

    dbus_message_iter_close_container(&args, &dict);

    dbus_uint32_t serie;
    int ok = dbus_connection_send(conn, msg, &serie);
    dbus_connection_flush(conn);
    dbus_message_unref(msg);
    return ok ? 0 : -1;
}

/* Enviar Mensaje 1 completo a un dispositivo BLE */
static int ble_enviar_msg1(DBusConnection *conn,
                            const char *mac,
                            const uint8_t msg1[MSG1_TOTAL])
{
    char ruta_disp[128];
    mac_a_ruta_dbus(mac, ruta_disp, sizeof(ruta_disp));

    if (ble_conectar_si_hace_falta(conn, ruta_disp) != 0) return -1;

    /* Esperar a que BlueZ descubra los servicios GATT */
    usleep(500000);

    char ruta_carac[256];
    if (buscar_ruta_caracteristica(conn, ruta_disp, GATT_MSG1_UUID,
                                   ruta_carac, sizeof(ruta_carac)) != 0) {
        fprintf(stderr,
            "[BLE] No se encontró la característica %s en %s\n"
            "      Comprueba que el UUID en ble_config.h es el NUS-RX\n",
            GATT_MSG1_UUID, mac);
        return -1;
    }
    printf("[BLE] Característica encontrada: %s\n", ruta_carac);

    int num_frags = (MSG1_TOTAL + BLE_FRAG_SIZE - 1) / BLE_FRAG_SIZE;
    printf("[BLE] Enviando %d bytes en %d fragmento(s) a %s\n",
           MSG1_TOTAL, num_frags, mac);

    uint16_t offset = 0;
    int frag = 0;
    while (offset < MSG1_TOTAL) {
        uint16_t trozo = (uint16_t)(MSG1_TOTAL - offset);
        if (trozo > BLE_FRAG_SIZE) trozo = BLE_FRAG_SIZE;

        printf("[BLE]   frag %d/%d  offset=%-4u  len=%u\n",
               ++frag, num_frags, offset, trozo);

        if (escribir_fragmento(conn, ruta_carac,
                               msg1 + offset, trozo, offset) != 0) {
            fprintf(stderr, "[BLE] Error en fragmento offset=%u\n", offset);
            return -1;
        }
        usleep(10000);   /* 10 ms entre fragmentos */
        offset += trozo;
    }
    printf("[BLE] Mensaje 1 enviado a %s (%d bytes)\n", mac, MSG1_TOTAL);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════ */
int main(void)
{
    srand((unsigned)time(NULL));

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Servidor PUF-BLE — Setup + Mensaje 1 (BLE) ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* ── 1. Inicializar BCH y tabla L_S ─────────────────────────── */
    if (bch_puf_init() != 0) return 1;

    tabla_ls_t tabla_ls;
    ls_inicializar(&tabla_ls);

    /* ── 2. IDs de los dispositivos ─────────────────────────────── */
    const uint8_t ID_A[ID_LEN] = {
        0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    const uint8_t ID_B[ID_LEN] = {
        0xBB, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    /* ── 3. Pre-enrollment ───────────────────────────────────────── */
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

    /* ── 4. Desafíos aleatorios ──────────────────────────────────── */
    uint8_t desafio_a = elegir_desafio();
    uint8_t desafio_b = elegir_desafio();
    printf("[MSG1] Desafío A=%u  Desafío B=%u\n",
           (unsigned)desafio_a, (unsigned)desafio_b);

    /* ── 5. Helper data BCH ──────────────────────────────────────── */
    uint8_t h_a[BCH_HA_BYTES];
    uint8_t h_b[BCH_HA_BYTES];

    printf("[MSG1] Calculando h_a (bloque %u)...\n", (unsigned)desafio_a);
    if (bch_puf_encode_block(ref_bloques[desafio_a], mask_bloques[desafio_a],
                              h_a, NULL) != 0) goto error;

    printf("[MSG1] Calculando h_b (bloque %u)...\n", (unsigned)desafio_b);
    if (bch_puf_encode_block(ref_bloques[desafio_b], mask_bloques[desafio_b],
                              h_b, NULL) != 0) goto error;

    /* ── 6. Nonces ───────────────────────────────────────────────── */
    uint8_t nonce_m[NONCE_LEN];
    uint8_t nonce_n[NONCE_LEN];
    generar_nonce(nonce_m, NONCE_LEN);
    generar_nonce(nonce_n, NONCE_LEN);

    /* ── 7. Construir mensajes ───────────────────────────────────── */
    uint8_t msg1_a[MSG1_TOTAL];
    uint8_t msg1_b[MSG1_TOTAL];
    construir_msg1(ID_A, ID_B, h_a, nonce_m, msg1_a);
    construir_msg1(ID_B, ID_A, h_b, nonce_n, msg1_b);
    mostrar_msg1("Dispositivo A", msg1_a);
    mostrar_msg1("Dispositivo B", msg1_b);

    printf("\n[MSG1] %d bytes | %d fragmento(s) BLE (%d bytes c/u)\n",
           MSG1_TOTAL,
           (MSG1_TOTAL + BLE_FRAG_SIZE - 1) / BLE_FRAG_SIZE,
           BLE_FRAG_SIZE);

    /* ── 8. Conectar D-Bus y enviar por BLE ─────────────────────── */
    DBusError err_dbus;
    dbus_error_init(&err_dbus);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err_dbus);
    if (!conn || dbus_error_is_set(&err_dbus)) {
        fprintf(stderr, "[DBUS] No se pudo conectar al bus del sistema: %s\n",
                dbus_error_is_set(&err_dbus) ? err_dbus.message : "(sin detalles)");
        dbus_error_free(&err_dbus);
        goto error;
    }
    printf("[DBUS] Conectado al bus del sistema\n");

    printf("\n[BLE] ══ Enviando a Dispositivo A (%s) ══\n", MAC_A);
    int rc_a = ble_enviar_msg1(conn, MAC_A, msg1_a);
    if (rc_a != 0)
        fprintf(stderr, "[BLE] Falló el envío a Dispositivo A\n");

    printf("\n[BLE] ══ Enviando a Dispositivo B (%s) ══\n", MAC_B);
    int rc_b = ble_enviar_msg1(conn, MAC_B, msg1_b);
    if (rc_b != 0)
        fprintf(stderr, "[BLE] Falló el envío a Dispositivo B\n");

    /* Limpiar material criptográfico */
    memset(h_a,     0, BCH_HA_BYTES);
    memset(h_b,     0, BCH_HA_BYTES);
    memset(nonce_m, 0, NONCE_LEN);
    memset(nonce_n, 0, NONCE_LEN);
    memset(msg1_a,  0, MSG1_TOTAL);
    memset(msg1_b,  0, MSG1_TOTAL);

    dbus_connection_unref(conn);
    bch_puf_free();

    if (rc_a != 0 || rc_b != 0) {
        printf("\n[MAIN] Finalizado con errores BLE.\n");
        return 1;
    }
    printf("\n[MAIN] Setup completado — Mensaje 1 enviado a ambos dispositivos.\n");
    return 0;

error:
    bch_puf_free();
    return 1;
}
