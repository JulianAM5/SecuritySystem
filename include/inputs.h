#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>

/* PCF8574 (0x20) con los sensores reed. Bit = 1 -> ABIERTO, 0 -> CERRADO.
 * Los cambios se detectan con INT1 (pin INT del PCF). */
void    inputs_init(void);
/* Retorna 1 si hubo cambios; 'changed' = mascara de bits que cambiaron */
uint8_t inputs_update(uint8_t *changed);
/* Ultimo estado leido (bit=1 abierto) */
uint8_t inputs_state(void);

#endif
