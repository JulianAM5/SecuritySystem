#include "buzzer.h"
#include "timer.h"
#include "board.h"

/* PWM rapido en Timer2: 16 MHz / 64 / 256 = ~976 Hz.
 * El volumen se ajusta con el ciclo de trabajo. */
#define DUTY_MEDIUM      64      /* ~25 % */
#define DUTY_HIGH        128     /* ~50 % */
#define INTERMITTENT_MS  250UL

typedef enum { BUZ_OFF, BUZ_BEEP, BUZ_CONT, BUZ_INTERMITTENT } buz_mode_t;

static buz_mode_t mode = BUZ_OFF;
static uint32_t   t0 = 0;
static uint16_t   beep_ms = 0;
static uint8_t    phase_on = 0;

static void out_on(uint8_t duty)
{
    OCR2A = duty;
    TCCR2A |= (1 << COM2A1);
}

static void out_off(void)
{
    TCCR2A &= (uint8_t)~(1 << COM2A1);
    PORTB  &= (uint8_t)~(1 << BUZZER_PIN);
}

void buzzer_init(void)
{
    DDRB  |= (1 << BUZZER_PIN);              /* buzzer y LED comparten esta terminal */
    TCCR2A = (1 << WGM21) | (1 << WGM20);   /* Fast PWM, OC2A desconectado */
    TCCR2B = (1 << CS22);                   /* prescaler 64 */
    buzzer_off();
}

void buzzer_off(void)
{
    mode = BUZ_OFF;
    out_off();
}

void buzzer_beep(uint16_t ms)
{
    if (mode == BUZ_CONT || mode == BUZ_INTERMITTENT) return; /* no interrumpir alarma */
    mode = BUZ_BEEP;
    beep_ms = ms;
    t0 = millis();
    out_on(DUTY_MEDIUM);
}

void buzzer_continuous_medium(void)
{
    mode = BUZ_CONT;
    out_on(DUTY_MEDIUM);
}

void buzzer_intermittent_high(void)
{
    mode = BUZ_INTERMITTENT;
    phase_on = 1;
    t0 = millis();
    out_on(DUTY_HIGH);
}

void buzzer_task(void)
{
    switch (mode) {
    case BUZ_BEEP:
        if ((uint32_t)(millis() - t0) >= beep_ms) buzzer_off();
        break;
    case BUZ_INTERMITTENT:
        if ((uint32_t)(millis() - t0) >= INTERMITTENT_MS) {
            t0 = millis();
            phase_on = !phase_on;
            if (phase_on) out_on(DUTY_HIGH); else out_off();
        }
        break;
    default:
        break;
    }
}
