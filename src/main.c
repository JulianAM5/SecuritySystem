/*
 * Sistema de seguridad para casa (maqueta) - ATmega328P (Arduino NANO)
 *
 * Perifericos: Timer0 (base 1 ms), Timer2 (PWM buzzer), USART, EEPROM,
 *              I2C (PCF8574 de entradas), INT1 (PCF8574), PCINT1 (teclado),
 *              LCD 16x2 por I2C (PCF8574 0x27).
 */
#include "board.h"
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "i2c.h"
#include "UART.h"
#include "lcd.h"
#include "timer.h"
#include "buzzer.h"
#include "keypad.h"
#include "inputs.h"
#include "config.h"
#include "cmd.h"
#include "clock.h"
#include "log.h"

typedef enum {
    ST_DISARMED,    /* NO ARMADO                                   */
    ST_ARMING,      /* cuenta regresiva de salida (tiempo armado)  */
    ST_ARMED,       /* ARMADO                                      */
    ST_ENTRY,       /* entrada abierta: cuenta de desarmado        */
    ST_ALARM        /* alarma intermitente (buzzer + LED)          */
} state_t;

static state_t  state = ST_DISARMED;
static uint32_t state_t0 = 0;
static uint8_t  trigger_id = 0;

/* Secuencia de teclas */
static char     seq[4];
static uint8_t  seq_len = 0;
static uint32_t last_key_ms = 0;

/* Lista de entradas abiertas (intento de armado fallido) */
static uint8_t  show_open = 0;
static uint8_t  open_idx = 0;
static uint32_t open_t0 = 0;

/* Mensaje temporal en pantalla */
static char     msg1[LCD_COLS + 1], msg2[LCD_COLS + 1];
static uint8_t  msg_active = 0;
static uint32_t msg_until = 0;

/* ---------- Utilidades ---------- */

static uint32_t elapsed(uint32_t t0) { return (uint32_t)(millis() - t0); }

static uint8_t open_mask(void)
{
    return inputs_state() & config_active_mask();
}

static uint8_t lowest_bit(uint8_t mask)
{
    for (uint8_t i = 0; i < 8; i++) if (mask & (1 << i)) return i;
    return 0;
}

static uint8_t next_open(uint8_t mask, uint8_t idx)
{
    for (uint8_t i = 1; i <= 8; i++) {
        uint8_t j = (uint8_t)((idx + i) & 7);
        if (mask & (1 << j)) return j;
    }
    return idx;
}

static void show_msg(const char *a, const char *b, uint16_t ms)
{
    snprintf(msg1, sizeof(msg1), "%s", a);
    snprintf(msg2, sizeof(msg2), "%s", b);
    msg_until = millis() + ms;
    msg_active = 1;
}

static void echo_keys(char *dst)
{
    if (seq_len == 0) { dst[0] = '\0'; return; }
    strcpy(dst, "Clave: ");
    uint8_t n = 7;
    for (uint8_t i = 0; i < seq_len; i++) dst[n++] = '*';
    dst[n] = '\0';
}

/* ---------- Logica del sistema ---------- */

static void start_entry(uint8_t id)
{
    trigger_id = id;
    state = ST_ENTRY;
    state_t0 = millis();
    buzzer_continuous_medium();
}

static void try_arm(void)
{
    uint8_t m = open_mask();
    if (m) {
        /* No se arma: mostrar las entradas abiertas (rotando cada 1 s) */
        show_open = 1;
        open_idx = lowest_bit(m);
        open_t0 = millis();
    } else {
        show_open = 0;
        state = ST_ARMING;
        state_t0 = millis();
    }
}

static void on_password_ok(void)
{
    switch (state) {
    case ST_DISARMED:
        try_arm();
        break;
    case ST_ARMING:
    case ST_ARMED:
    case ST_ENTRY:
    case ST_ALARM:
        buzzer_off();
        state = ST_DISARMED;
        break;
    }
}

static void handle_key(char k)
{
    last_key_ms = millis();

    if (k == '*') {                 /* descartar secuencia */
        seq_len = 0;
        show_open = 0;
        return;
    }
    if (seq_len < 4) seq[seq_len++] = k;

    if (seq_len == 4) {
        seq_len = 0;
        if (memcmp(seq, cfg.pass, 4) == 0) on_password_ok();
        else show_msg("CLAVE", "INCORRECTA", 1000);
    }
}

static void process_inputs(void)
{
    uint8_t changed;
    if (!inputs_update(&changed)) return;

    uint8_t active = config_active_mask();
    uint8_t ch = changed & active;
    if (!ch) return;

    uint8_t now = inputs_state();
    uint8_t id = lowest_bit(ch);

    switch (state) {
    case ST_DISARMED:
        buzzer_beep(150);
        show_msg(cfg.entries[id].name,
                 (now & (1 << id)) ? "ABIERTA" : "CERRADA", 2000);
        break;
    case ST_ARMED: {
        uint8_t opened = ch & now;          /* solo aperturas disparan */
        if (opened) start_entry(lowest_bit(opened));
        break;
    }
    default:                                /* ARMING / ENTRY / ALARM: sin accion */
        break;
    }
}

static void update_state(void)
{
    switch (state) {
    case ST_ARMING:
        if (elapsed(state_t0) >= (uint32_t)cfg.arm_time * 1000UL) {
            uint8_t m = open_mask();
            if (m) start_entry(lowest_bit(m));   /* algo quedo abierto */
            else   state = ST_ARMED;
        }
        break;
    case ST_ENTRY:
        if (elapsed(state_t0) >= (uint32_t)cfg.disarm_time * 1000UL) {
            state = ST_ALARM;
            buzzer_intermittent_high();
            log_alarm_add(trigger_id, cfg.entries[trigger_id].name);
        }
        break;
    default:
        break;
    }
}

static void update_display(void)
{
    char l1[LCD_COLS + 1], l2[LCD_COLS + 1];
    uint16_t rem;

    if (msg_active) {
        if ((int32_t)(millis() - msg_until) < 0) {
            lcd_line(0, msg1);
            lcd_line(1, msg2);
            return;
        }
        msg_active = 0;
    }

    switch (state) {
    case ST_DISARMED:
        if (show_open) {
            uint8_t m = open_mask();
            if (!m) {
                show_open = 0;
            } else {
                if (!(m & (1 << open_idx))) {
                    open_idx = next_open(m, open_idx);
                    open_t0 = millis();
                } else if (elapsed(open_t0) >= 1000UL) {
                    open_idx = next_open(m, open_idx);
                    open_t0 = millis();
                }
                lcd_line(0, "ENTRADA ABIERTA");
                lcd_line(1, cfg.entries[open_idx].name);
                return;
            }
        }
        echo_keys(l2);
        lcd_line(0, "NO ARMADO");
        lcd_line(1, l2);
        break;

    case ST_ARMING:
        rem = (uint16_t)(elapsed(state_t0) / 1000UL);
        rem = (rem >= cfg.arm_time) ? 0 : (uint16_t)(cfg.arm_time - rem);
        snprintf(l2, sizeof(l2), "Salida: %u s", (unsigned)rem);
        lcd_line(0, "ARMANDO...");
        lcd_line(1, l2);
        break;

    case ST_ARMED:
        echo_keys(l2);
        lcd_line(0, "ARMADO");
        lcd_line(1, l2);
        break;

    case ST_ENTRY:
        rem = (uint16_t)(elapsed(state_t0) / 1000UL);
        rem = (rem >= cfg.disarm_time) ? 0 : (uint16_t)(cfg.disarm_time - rem);
        snprintf(l1, sizeof(l1), "ALERTA! %u s", (unsigned)rem);
        lcd_line(0, l1);
        if (seq_len) { echo_keys(l2); lcd_line(1, l2); }
        else         lcd_line(1, cfg.entries[trigger_id].name);
        break;

    case ST_ALARM:
        lcd_line(0, "!! ALARMA !!");
        if (seq_len) { echo_keys(l2); lcd_line(1, l2); }
        else         lcd_line(1, cfg.entries[trigger_id].name);
        break;
    }
}

/* ---------- Programa principal ---------- */

int main(void)
{
    timer_init();
    buzzer_init();
    UART_init(UART_UBRR_9600, 3, 0, 0);   /* 9600, 8 bits, sin paridad, 1 stop */
    PORTC |= (1 << PC4) | (1 << PC5);     /* pull-ups internos de SDA/SCL (respaldo) */
    i2c_init();
    config_load();
    log_init();
    clock_init();
    lcd_init();
    keypad_init();
    inputs_init();

    sei();

    lcd_line(0, "Seguridad Casa");
    lcd_line(1, "Iniciando...");
    _delay_ms(1000);

    for (;;) {
        clock_task();
        cmd_task();

        char k = keypad_get();
        if (k) handle_key(k);

        /* La secuencia se descarta si pasan 3 s sin presionar teclas */
        if (seq_len && elapsed(last_key_ms) >= KEY_TIMEOUT_MS) seq_len = 0;

        process_inputs();
        update_state();
        buzzer_task();
        update_display();
    }
}
