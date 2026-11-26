#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>
#include <time.h>

/* Reloj basado en <time.h> de avr-libc (epoch: 2000-01-01 00:00:00 UTC).
 * No hay RTC: system_tick() se invoca cada segundo desde clock_task()
 * usando millis() como base. Antes de ajustarlo con el comando TIME,
 * time() cuenta los segundos desde el arranque (clock_synced() = 0).
 * Nota: la fecha/hora se pierde al quitar la alimentacion. */
void    clock_init(void);
void    clock_task(void);              /* llamar en cada vuelta del loop */
void    clock_set(time_t t);           /* ajusta y marca como sincronizado */
uint8_t clock_synced(void);
time_t  clock_now(void);               /* time(NULL) actualizado */

#endif
