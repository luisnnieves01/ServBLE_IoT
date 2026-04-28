#ifndef MASCARA_H
#define MASCARA_H

#include <stdint.h>

/* Cuatro bloques de máscara de 1024 bytes cada uno.
 * Datos actuales: misma máscara en los 4 bloques. */
extern const uint8_t mask_bloque1[1024];
extern const uint8_t mask_bloque2[1024];
extern const uint8_t mask_bloque3[1024];
extern const uint8_t mask_bloque4[1024];

#endif /* MASCARA_H */
