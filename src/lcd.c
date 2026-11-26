#include "lcd.h"
#include "pcf8574.h"
#include "board.h"
#include <util/delay.h>
#include <string.h>

/* LCD HD44780 con modulo I2C (PCF8574 en LCD_I2C_ADDR).
 * Mapeo tipico del modulo (backpack):
 *   P0=RS  P1=RW  P2=EN  P3=Backlight  P4..P7=D4..D7            */
#define LCD_RS  0x01
#define LCD_EN  0x04
#define LCD_BL  0x08

static char shadow[LCD_ROWS][LCD_COLS];

static void lcd_nibble(uint8_t nib, uint8_t rs)
{
    uint8_t d = (uint8_t)((nib & 0xF0) | rs | LCD_BL);
    pcf8574_write(LCD_I2C_ADDR, d | LCD_EN);
    _delay_us(2);
    pcf8574_write(LCD_I2C_ADDR, d & (uint8_t)~LCD_EN);
    _delay_us(50);
}

static void lcd_send(uint8_t v, uint8_t rs)
{
    lcd_nibble(v & 0xF0, rs);
    lcd_nibble((uint8_t)(v << 4), rs);
}

static void lcd_command(uint8_t c)
{
    lcd_send(c, 0);
    if (c <= 0x03) _delay_ms(2);           /* clear / home */
}

void lcd_init(void)
{
    _delay_ms(50);
    lcd_nibble(0x30, 0); _delay_ms(5);
    lcd_nibble(0x30, 0); _delay_us(150);
    lcd_nibble(0x30, 0); _delay_us(150);
    lcd_nibble(0x20, 0); _delay_us(150);   /* modo 4 bits */

    lcd_command(0x28);                     /* 4 bits, 2 lineas, 5x8 */
    lcd_command(0x08);                     /* display off */
    lcd_command(0x01);                     /* clear */
    lcd_command(0x06);                     /* incrementar cursor */
    lcd_command(0x0C);                     /* display on, sin cursor */
    memset(shadow, ' ', sizeof(shadow));
}

void lcd_clear(void)
{
    lcd_command(0x01);
    memset(shadow, ' ', sizeof(shadow));
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    static const uint8_t base[LCD_ROWS] = {0x00, 0x40};
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    lcd_command((uint8_t)(0x80 | (base[row] + col)));
}

void lcd_putc(char c)
{
    lcd_send((uint8_t)c, LCD_RS);
}

void lcd_print(const char *s)
{
    while (*s) lcd_putc(*s++);
}

void lcd_line(uint8_t row, const char *s)
{
    char buf[LCD_COLS];
    uint8_t i = 0;

    if (row >= LCD_ROWS) return;
    while (i < LCD_COLS && s[i]) { buf[i] = s[i]; i++; }
    while (i < LCD_COLS) buf[i++] = ' ';

    if (memcmp(buf, shadow[row], LCD_COLS) == 0) return;   /* sin cambios */
    memcpy(shadow[row], buf, LCD_COLS);

    lcd_set_cursor(0, row);
    for (i = 0; i < LCD_COLS; i++) lcd_putc(buf[i]);
}
