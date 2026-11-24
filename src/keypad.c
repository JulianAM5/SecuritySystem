#include "keypad.h"
#include "pcf8574.h"
#include "timer.h"
#include "board.h"
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#define KP_IDLE        0xF0   /* filas en 0 (salida), columnas en 1 (entrada) */
#define HELD_POLL_MS   20UL

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static volatile uint8_t kp_flag = 0;
static uint8_t  held = 0;
static uint32_t last_poll = 0;

/* INT del PCF8574 del teclado (activo en bajo) */
ISR(INT0_vect)
{
    kp_flag = 1;
}

/* Barrido de filas. Retorna la tecla o 0 */
static char keypad_scan(void)
{
    char key = 0;

    for (uint8_t r = 0; r < 4 && !key; r++) {
        /* fila r en 0, demas filas en 1, columnas en 1 (entradas) */
        pcf8574_write(PCF_ADDR_KEYPAD, (uint8_t)(0xF0 | (~(1 << r) & 0x0F)));
        uint8_t cols = (uint8_t)((pcf8574_read(PCF_ADDR_KEYPAD) >> 4) & 0x0F);
        for (uint8_t c = 0; c < 4; c++) {
            if (!(cols & (1 << c))) { key = keymap[r][c]; break; }
        }
    }
    pcf8574_write(PCF_ADDR_KEYPAD, KP_IDLE);   /* regresar a reposo */
    return key;
}

void keypad_init(void)
{
    DDRD  &= (uint8_t)~(1 << PD2);
    PORTD |= (1 << PD2);                       /* pull-up (INT es open-drain) */
    EICRA |=  (1 << ISC01);                    /* flanco de bajada */
    EICRA &= (uint8_t)~(1 << ISC00);
    pcf8574_write(PCF_ADDR_KEYPAD, KP_IDLE);
    (void)pcf8574_read(PCF_ADDR_KEYPAD);       /* liberar INT */
    EIFR  = (1 << INTF0);
    EIMSK |= (1 << INT0);
}

char keypad_get(void)
{
    char k;

    if (held) {
        /* Mientras haya una tecla sostenida, verificar si ya se solto */
        if ((uint32_t)(millis() - last_poll) < HELD_POLL_MS) return 0;
        last_poll = millis();
    } else {
        uint8_t f;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { f = kp_flag; kp_flag = 0; }
        if (!f) return 0;
        _delay_ms(5);                          /* antirrebote */
    }

    k = keypad_scan();
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { kp_flag = 0; }  /* ignorar INT provocada por el barrido */

    if (k) {
        if (!held) {
            held = 1;
            last_poll = millis();
            return k;                          /* tecla nueva */
        }
        return 0;                              /* sigue presionada */
    }
    held = 0;
    return 0;
}
