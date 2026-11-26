#include "clock.h"
#include "timer.h"

static uint32_t last_tick = 0;
static uint8_t  synced = 0;

void clock_init(void)
{
    set_system_time(0);
    last_tick = millis();
    synced = 0;
}

/* Un system_tick() por cada segundo completo; seguro ante el desbordamiento
 * de millis(). Solo se llama desde el contexto principal (no desde ISR). */
void clock_task(void)
{
    while ((uint32_t)(millis() - last_tick) >= 1000UL) {
        last_tick += 1000UL;
        system_tick();
    }
}

void clock_set(time_t t)
{
    clock_task();
    set_system_time(t);
    last_tick = millis();          /* reiniciar fraccion de segundo */
    synced = 1;
}

uint8_t clock_synced(void)
{
    return synced;
}

time_t clock_now(void)
{
    clock_task();
    return time(NULL);
}
