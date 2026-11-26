#include "keypad.h"
#include "timer.h"
#include "board.h"
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

/* Filas: F1=PB2 (D10), F2=PB4 (D12), F3=PB5 (D13), F4=PD2 (D2)
 * Columnas: PC0..PC3 (A0..A3)                                    */
#define KP_ROWS_B_MASK   ((1 << PB2) | (1 << PB4) | (1 << PB5))
#define KP_ROWS_D_MASK   (1 << PD2)
#define KP_COLS_MASK     0x0F
#define HELD_POLL_MS     20UL

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static volatile uint8_t kp_flag = 0;
static uint8_t  held = 0;
static uint32_t last_poll = 0;

/* Cambio en alguna columna (A0..A3) */
ISR(PCINT1_vect)
{
    kp_flag = 1;
}

/* Las filas se mantienen siempre con PORT = 0. Solo se cambia DDR:
 *   DDR = 1 -> la fila se maneja en 0 ;  DDR = 0 -> alta impedancia.
 * Asi nunca se maneja una fila en 1, y dos teclas presionadas a la vez
 * no pueden provocar un corto entre salidas.                         */
static void rows_drive(uint8_t mask4)
{
    uint8_t b = 0;
    if (mask4 & 0x01) b |= (1 << PB2);
    if (mask4 & 0x02) b |= (1 << PB4);
    if (mask4 & 0x04) b |= (1 << PB5);
    DDRB = (uint8_t)((DDRB & (uint8_t)~KP_ROWS_B_MASK) | b);
    if (mask4 & 0x08) DDRD |=  KP_ROWS_D_MASK;
    else              DDRD &= (uint8_t)~KP_ROWS_D_MASK;
}

/* Barrido de filas. Retorna la tecla o 0 */
static char keypad_scan(void)
{
    char key = 0;

    for (uint8_t r = 0; r < 4 && !key; r++) {
        rows_drive((uint8_t)(1 << r));            /* solo la fila r en 0 */
        _delay_us(10);                            /* estabilizar */
        uint8_t cols = (uint8_t)(PINC & KP_COLS_MASK);
        for (uint8_t c = 0; c < 4; c++) {
            if (!(cols & (1 << c))) { key = keymap[r][c]; break; }
        }
    }
    rows_drive(0x0F);                             /* regresar a reposo */
    return key;
}

void keypad_init(void)
{
    /* Filas: PORT = 0 y, en reposo, todas manejadas en 0 */
    PORTB &= (uint8_t)~KP_ROWS_B_MASK;
    PORTD &= (uint8_t)~KP_ROWS_D_MASK;
    rows_drive(0x0F);

    /* Columnas: entradas con pull-up */
    DDRC  &= (uint8_t)~KP_COLS_MASK;
    PORTC |= KP_COLS_MASK;

    /* Interrupcion por cambio de pin del puerto C, solo A0..A3 */
    PCMSK1 = KP_COLS_MASK;
    PCIFR  = (1 << PCIF1);
    PCICR |= (1 << PCIE1);
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
        _delay_ms(5);                             /* antirrebote */
    }

    k = keypad_scan();
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { kp_flag = 0; }  /* ignorar PCINT provocada por el barrido */

    if (k) {
        if (!held) {
            held = 1;
            last_poll = millis();
            return k;                             /* tecla nueva */
        }
        return 0;                                 /* sigue presionada */
    }
    held = 0;
    return 0;
}
