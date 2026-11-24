#ifndef BOARD_H
#define BOARD_H

/* Frecuencia del Arduino NANO. Idealmente defínela en
 * Project > Properties > Toolchain > Symbols: F_CPU=16000000UL
 * (i2c.c la necesita y no incluye este archivo). */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

/* ---------- Direcciones I2C (7 bits) ---------- */
#define PCF_ADDR_INPUTS   0x20   /* PCF8574 de entradas (puertas/ventanas) */
#define PCF_ADDR_KEYPAD   0x21   /* PCF8574 del teclado 4x4                */
#define LCD_I2C_ADDR      0x27   /* LCD 16x2 con modulo I2C                */

/* ---------- Pines ---------- */
/* INT0 (PD2/D2) <- INT del PCF8574 del teclado
 * INT1 (PD3/D3) <- INT del PCF8574 de entradas
 * OC2A (PB3/D11) -> Buzzer (PWM Timer2)
 * PB5 (D13)      -> LED de alarma                                  */
#define BUZZER_PIN        PB3
#define LED_PIN           PB5

/* ---------- UART ---------- */
#define UART_UBRR_9600    103    /* 16 MHz, U2X=0 -> 9600 baudios */

/* ---------- Configuracion del sistema ---------- */
#define MAX_ENTRIES       8
#define ENTRY_NAME_MAX    12
#define TIME_MIN          10
#define TIME_MAX          60
#define TIME_DEFAULT      20
#define KEY_TIMEOUT_MS    3000UL

#endif
