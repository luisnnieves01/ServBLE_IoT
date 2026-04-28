/* src/ble/ble_conf.h
 *
 * Configuración BLE del servidor — ajusta estos valores
 * según tu hardware antes de compilar.
 */
#ifndef BLE_CONF_H
#define BLE_CONF_H

/* Adaptador BlueZ (hciX) */
#define BLE_ADAPTER     "hci0"

/* Direcciones MAC BLE de los dispositivos nRF54L15.
 * Formato: "XX:XX:XX:XX:XX:XX"  (mayúsculas)             */
#define MAC_A           "EE:3B:74:FF:F2:F3"   /* ← ajusta */
#define MAC_B           "EE:3B:74:FF:F2:F3"   /* ← ajusta */

/* UUID de la característica GATT que recibe el Mensaje 1.
 * Debe coincidir con el descriptor del firmware nRF54L15. */
#define GATT_MSG1_UUID  "12345678-1234-5678-1234-56789abcdef0"  /* ← ajusta */

#endif /* BLE_CONF_H */
