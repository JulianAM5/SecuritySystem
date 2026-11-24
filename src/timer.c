#include "timer.h"
#include "board.h"
#include <avr/interrupt.h>

static volatile uint32_t ms_ticks = 0;

/* 16 MHz / 64 = 250 kHz -> 250 cuentas = 1 ms */
ISR(TIMER0_COMPA_vect)
{
    ms_ticks++;
}

void timer_init(void)
{
    TCCR0A = (1 << WGM01);                  /* CTC */
    TCCR0B = (1 << CS01) | (1 << CS00);     /* prescaler 64 */
    OCR0A  = 249;
    TIMSK0 |= (1 << OCIE0A);
}

uint32_t millis(void)
{
    uint8_t sreg = SREG;
    cli();
    uint32_t m = ms_ticks;
    SREG = sreg;
    return m;
}
