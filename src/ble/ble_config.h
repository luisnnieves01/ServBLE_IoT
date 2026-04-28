/* src/ble/ble_config.h
 *
 * Configuración BLE del servidor Linux.
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  El firmware nRF54L15 (puf_ble.c) define:                       │
 * │    PUF_MSG1_UUID = 6e400002-... → BT_GATT_CHRC_WRITE            │
 * │    PUF_MSG2_UUID = 6e400003-... → BT_GATT_CHRC_NOTIFY           │
 * │                                                                 │
 * │  bluetoothctl los etiqueta con nombres NUS "al revés":          │
 * │    char0011  6e400002  "Nordic UART TX"  ← tiene WRITE          │
 * │    char0013  6e400003  "Nordic UART RX"  ← tiene NOTIFY         │
 * │                                                                 │
 * │  Lo que importa es la propiedad GATT, no el nombre BlueZ.       │
 * │  → El servidor escribe MSG1 en 6e400002.                        │
 * └─────────────────────────────────────────────────────────────────┘
 */

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

/* Adaptador BlueZ: ejecutar `hciconfig` para confirmar */
#define BLE_ADAPTER     "hci0"

/* MACs de los dispositivos nRF54L15 */
#define MAC_A           "EE:3B:74:FF:F2:F3"
#define MAC_B           "EE:3B:74:FF:F2:F3"   /* cambiar cuando haya 2do dispositivo */

/* UUID donde el servidor ESCRIBE el Mensaje 1 (= PUF_MSG1_UUID del firmware) */
#define GATT_MSG1_UUID  "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

/* UUID donde el servidor ESCUCHA el Mensaje 2 por Notify (fase siguiente) */
#define GATT_MSG2_UUID  "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#endif /* BLE_CONFIG_H */
