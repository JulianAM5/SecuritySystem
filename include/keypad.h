#ifndef KEYPAD_H
#define KEYPAD_H

/* Teclado matricial 4x4 conectado directamente al ATmega328P.
 *   Filas    (salidas) : F1=D10 (PB2), F2=D12 (PB4), F3=D13 (PB5), F4=D2 (PD2)
 *   Columnas (entradas): C1..C4 = A0..A3 (PC0..PC3) con pull-up interno
 * Reposo: todas las filas en 0; al presionar una tecla su columna baja y se
 * dispara la interrupcion por cambio de pin PCINT1 (PCINT8..11). */
void keypad_init(void);
/* Retorna la tecla recien presionada o 0 si no hay ninguna nueva */
char keypad_get(void);

#endif
