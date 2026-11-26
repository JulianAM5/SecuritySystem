#include "log.h"
#include "clock.h"
#include "config.h"
#include "EEPROM.h"

/* ---------- Mapa de la EEPROM (a continuacion de la configuracion) ----------
 * 128 : numero de registros
 * 129 : siguiente indice a escribir
 * 130 + 18*i : registro i -> [id][nombre x12][time_t x4, LSB primero][synced] */
#define LOG_ADDR_COUNT   128
#define LOG_ADDR_HEAD    129
#define LOG_ADDR_RECORDS 130
#define LOG_REC_SIZE     (1 + ENTRY_NAME_MAX + 4 + 1)
#define LOG_REC_ADDR(i)  (LOG_ADDR_RECORDS + (uint16_t)(i) * LOG_REC_SIZE)

static uint8_t count = 0;
static uint8_t head  = 0;

void log_init(void)
{
    count = EEPROM_read(LOG_ADDR_COUNT);
    head  = EEPROM_read(LOG_ADDR_HEAD);
    if (count > LOG_MAX || head >= LOG_MAX) {     /* EEPROM virgen o corrupta */
        count = 0;
        head = 0;
        config_ee_put(LOG_ADDR_COUNT, 0);
        config_ee_put(LOG_ADDR_HEAD, 0);
    }
}

void log_alarm_add(uint8_t id, const char *name)
{
    uint8_t synced = clock_synced();
    uint32_t ts = (uint32_t)clock_now();
    uint16_t a = LOG_REC_ADDR(head);
    uint8_t ended = 0;

    config_ee_put(a, id);
    for (uint8_t i = 0; i < ENTRY_NAME_MAX; i++) {
        if (!ended && name[i] == '\0') ended = 1;
        config_ee_put(a + 1 + i, ended ? 0 : (uint8_t)name[i]);
    }
    for (uint8_t i = 0; i < 4; i++) {
        config_ee_put(a + 1 + ENTRY_NAME_MAX + i, (uint8_t)(ts >> (8 * i)));
    }
    config_ee_put(a + 1 + ENTRY_NAME_MAX + 4, synced);

    /* El encabezado se actualiza al final: un corte a mitad no corrompe el log */
    head = (uint8_t)((head + 1) % LOG_MAX);
    if (count < LOG_MAX) count++;
    config_ee_put(LOG_ADDR_HEAD, head);
    config_ee_put(LOG_ADDR_COUNT, count);
}

uint8_t log_count(void)
{
    return count;
}

uint8_t log_get(uint8_t n, log_entry_t *e)
{
    if (n >= count) return 0;

    uint8_t idx = (uint8_t)((head + LOG_MAX - 1 - n) % LOG_MAX);
    uint16_t a = LOG_REC_ADDR(idx);

    e->id = EEPROM_read(a);
    for (uint8_t i = 0; i < ENTRY_NAME_MAX; i++) e->name[i] = (char)EEPROM_read(a + 1 + i);
    e->name[ENTRY_NAME_MAX] = '\0';
    uint32_t ts = 0;
    for (uint8_t i = 0; i < 4; i++) {
        ts |= (uint32_t)EEPROM_read(a + 1 + ENTRY_NAME_MAX + i) << (8 * i);
    }
    e->ts     = (time_t)ts;
    e->synced = EEPROM_read(a + 1 + ENTRY_NAME_MAX + 4);
    return 1;
}
