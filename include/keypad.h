#ifndef KEYPAD_H
#define KEYPAD_H

/* Teclado 4x4 conectado a un PCF8574 (0x21):
 *   P0-P3 = filas, P4-P7 = columnas.
 * La pulsacion se detecta con la interrupcion INT0 (pin INT del PCF). */
void keypad_init(void);
/* Retorna la tecla recien presionada o 0 si no hay ninguna nueva */
char keypad_get(void);

#endif
