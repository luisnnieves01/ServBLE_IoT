/* src/cifrador_ascon/ascon_ccm.h */
#ifndef ASCON_CCM_H
#define ASCON_CCM_H

#include <stdint.h>

/* Tamaños fijos de ASCON-128 */
#define ASCON_KEY_LEN    16   /* 128 bits */
#define ASCON_NONCE_LEN  16   /* 128 bits */
#define ASCON_TAG_LEN    16   /* 128 bits de autenticación */

/*
 * ascon_ccm_enc — cifra y autentica con ASCON-128.
 *
 * Parámetros:
 *   key[16]      : clave derivada de H(R_x)[0:16]
 *   nonce[16]    : nonce m o n del dispositivo
 *   ad, ad_len   : datos asociados (ID del dispositivo)
 *   pt, pt_len   : plaintext = nonce_peer || Ø_x
 *   ct_out       : buffer de salida = ciphertext || tag
 *                  (debe tener al menos pt_len + ASCON_TAG_LEN bytes)
 *
 * Retorna: longitud total de ct_out (pt_len + 16), o -1 en error.
 */
int ascon_ccm_enc(const uint8_t key[ASCON_KEY_LEN],
                  const uint8_t nonce[ASCON_NONCE_LEN],
                  const uint8_t *ad,  uint32_t ad_len,
                  const uint8_t *pt,  uint32_t pt_len,
                  uint8_t       *ct_out);

/*
 * ascon_ccm_dec — descifra y verifica autenticidad.
 *
 * Retorna: longitud del plaintext recuperado, o -1 si el tag falla.
 */
int ascon_ccm_dec(const uint8_t key[ASCON_KEY_LEN],
                  const uint8_t nonce[ASCON_NONCE_LEN],
                  const uint8_t *ad,    uint32_t ad_len,
                  const uint8_t *ct,    uint32_t ct_len,
                  uint8_t       *pt_out);

#endif /* ASCON_CCM_H */
