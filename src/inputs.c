#include "inputs.h"
#include "pcf8574.h"
#include "timer.h"
#include "board.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

/* Lectura de respaldo por si se perdiera una interrupcion */
#define INPUTS_POLL_MS  250UL

static volatile uint8_t in_flag = 0;
static uint8_t  state = 0;
static uint32_t last_poll = 0;

/* INT del PCF8574 de entradas (activo en bajo) */
ISR(INT1_vect)
{
    in_flag = 1;
}

void inputs_init(void)
{
    DDRD  &= (uint8_t)~(1 << PD3);
    PORTD |= (1 << PD3);
    EICRA |=  (1 << ISC11);                    /* flanco de bajada */
    EICRA &= (uint8_t)~(1 << ISC10);

    pcf8574_write(PCF_ADDR_INPUTS, 0xFF);      /* todos como entradas */
    state = pcf8574_read(PCF_ADDR_INPUTS);     /* lectura inicial (libera INT) */
    last_poll = millis();

    EIFR  = (1 << INTF1);
    EIMSK |= (1 << INT1);
}

uint8_t inputs_update(uint8_t *changed)
{
    uint8_t f;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { f = in_flag; in_flag = 0; }

    if (!f) {
        if ((uint32_t)(millis() - last_poll) < INPUTS_POLL_MS) return 0;
    }
    last_poll = millis();

    uint8_t now = pcf8574_read(PCF_ADDR_INPUTS);
    uint8_t diff = now ^ state;
    state = now;
    if (!diff) return 0;
    *changed = diff;
    return 1;
}

uint8_t inputs_state(void)
{
    return state;
}
