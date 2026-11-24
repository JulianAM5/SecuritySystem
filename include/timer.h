#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Timer0 en modo CTC: interrupcion cada 1 ms */
void     timer_init(void);
uint32_t millis(void);

#endif
