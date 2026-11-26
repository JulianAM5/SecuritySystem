#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

/* Buzzer por PWM (Timer2/OC2A). El LED de alarma va en la misma terminal. */
void buzzer_init(void);
void buzzer_off(void);
void buzzer_beep(uint16_t ms);          /* pitido breve, volumen medio     */
void buzzer_continuous_medium(void);    /* sonido continuo, volumen medio  */
void buzzer_intermittent_high(void);    /* intermitente volumen alto (buzzer + LED) */
void buzzer_task(void);                 /* llamar en cada vuelta del loop  */

#endif
