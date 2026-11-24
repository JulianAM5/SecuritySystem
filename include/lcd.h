#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#define LCD_COLS 16
#define LCD_ROWS 2

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_putc(char c);
void lcd_print(const char *s);
/* Escribe una linea completa (rellena con espacios). Solo envia por I2C
 * si el texto cambio respecto a lo que ya esta en pantalla. */
void lcd_line(uint8_t row, const char *s);

#endif
