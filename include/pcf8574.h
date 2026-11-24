#ifndef PCF8574_H
#define PCF8574_H

#include <stdint.h>

/* addr = direccion de 7 bits. Retorna 0 si hubo ACK, 1 si error */
uint8_t pcf8574_write(uint8_t addr, uint8_t value);
/* Retorna el valor leido (0xFF si el dispositivo no responde) */
uint8_t pcf8574_read(uint8_t addr);

#endif
