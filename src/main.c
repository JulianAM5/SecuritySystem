/*  blink_builtin.c
 *
 *  Blink the built‑in LED (D13) of an Arduino Nano.
 *  Uses pure AVR C, not the Arduino IDE framework.
 */

#include <avr/io.h>
#include <util/delay.h>
#include "i2c.h"

/* ------------------------------------------------------------------
   Pin mapping on the Arduino Nano:
     D13 → PB5 (PORTB bit 5)
------------------------------------------------------------------ */
#define LED_DDR DDRB
#define LED_PORT PORTB
#define LED_PIN  PB5

int main(void) {
    /* Configure pin 13 as an output. */
    LED_DDR |= _BV(LED_PIN);          // DDRB = DDRB | (1 << PB5)
    
    while (1) {
        /* Turn the LED on: set bit 5 of PORTB to 1. */
        LED_PORT |= _BV(LED_PIN);
        _delay_ms(500);

        /* Turn the LED off: clear bit 5 of PORTB. */
        LED_PORT &= ~_BV(LED_PIN);
        _delay_ms(500);
    }

    return 0;   // never reached
}

