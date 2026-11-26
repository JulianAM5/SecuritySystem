#ifndef BOARD_H
#define BOARD_H

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

/* ============================ PINOUT ============================
 *  Arduino NANO | ATmega328P | Funcion
 *  -------------+------------+-----------------------------------
 *  D0  (RX)     | PD0        | UART RX
 *  D1  (TX)     | PD1        | UART TX
 *  D2           | PD2        | Teclado: fila F4
 *  D3           | PD3 (INT1) | INT del PCF8574 de entradas (0x20)
 *  D4..D9       | PD4..PD7, PB0, PB1 | LIBRES (el LCD va por I2C)
 *  D10          | PB2        | Teclado: fila F1
 *  D11          | PB3 (OC2A) | Buzzer + LED (comparten terminal, PWM Timer2)
 *  D12          | PB4        | Teclado: fila F2
 *  D13          | PB5        | Teclado: fila F3
 *  A0..A3       | PC0..PC3   | Teclado: columnas C1..C4 (PCINT8..11)
 *  A4 / A5      | PC4 / PC5  | I2C (SDA / SCL): PCF8574 de entradas (0x20) + modulo LCD (0x27)
 * ================================================================ */

/* ---------- Direccion I2C (7 bits) ---------- */
#define PCF_ADDR_INPUTS   0x20   /* PCF8574 de entradas (puertas/ventanas) */

/* ---------- LCD 16x2 con modulo I2C (PCF8574) ---------- */
#define LCD_I2C_ADDR      0x27   /* A0=A1=A2=1 en PCF8574; 0x3F si es PCF8574A */

/* ---------- Buzzer y LED (misma terminal) ---------- */
#define BUZZER_PIN        PB3

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
