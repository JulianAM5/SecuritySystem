#include "config.h"
#include "EEPROM.h"
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

/* ---------- Mapa de la EEPROM ----------
 * 0        : marca de inicializacion
 * 1..4     : clave (4 digitos ASCII)
 * 5        : tiempo de armado
 * 6        : tiempo de desarmado
 * 8 + 13*i : entrada i -> [enabled][nombre x12]            */
#define CFG_MAGIC        0xA5
#define ADDR_MAGIC       0
#define ADDR_PASS        1
#define ADDR_ARM         5
#define ADDR_DISARM      6
#define ADDR_ENTRIES     8
#define ENTRY_SIZE       (1 + ENTRY_NAME_MAX)

config_t cfg;

/* Escritura segura: la secuencia EEMPE->EEPE debe ocurrir en 4 ciclos,
 * por eso se hace sin interrupciones (esperando antes a que termine
 * cualquier escritura previa para no bloquear el tick de 1 ms). */
static void ee_put(uint16_t addr, uint8_t value)
{
    while (EECR & (1 << EEPE));
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        EEPROM_update(addr, value);
    }
}

static void factory_defaults(void)
{
    memset(&cfg, 0, sizeof(cfg));
    memcpy(cfg.pass, "1234", 4);
    cfg.arm_time = TIME_DEFAULT;
    cfg.disarm_time = TIME_DEFAULT;

    /* Maqueta: una puerta y dos ventanas */
    cfg.entries[0].enabled = 1; strcpy(cfg.entries[0].name, "Puerta");
    cfg.entries[1].enabled = 1; strcpy(cfg.entries[1].name, "Ventana 1");
    cfg.entries[2].enabled = 1; strcpy(cfg.entries[2].name, "Ventana 2");
}

void config_save_pass(void)
{
    for (uint8_t i = 0; i < 4; i++) ee_put(ADDR_PASS + i, (uint8_t)cfg.pass[i]);
}

void config_save_arm_time(void)    { ee_put(ADDR_ARM, cfg.arm_time); }
void config_save_disarm_time(void) { ee_put(ADDR_DISARM, cfg.disarm_time); }

void config_save_entry(uint8_t id)
{
    if (id >= MAX_ENTRIES) return;
    uint16_t base = ADDR_ENTRIES + (uint16_t)id * ENTRY_SIZE;
    ee_put(base, cfg.entries[id].enabled ? 1 : 0);
    for (uint8_t i = 0; i < ENTRY_NAME_MAX; i++) {
        ee_put(base + 1 + i, (uint8_t)cfg.entries[id].name[i]);  /* relleno con 0 */
    }
}

void config_load(void)
{
    if (EEPROM_read(ADDR_MAGIC) != CFG_MAGIC) {
        /* Primer arranque: escribir valores por defecto */
        factory_defaults();
        config_save_pass();
        config_save_arm_time();
        config_save_disarm_time();
        for (uint8_t i = 0; i < MAX_ENTRIES; i++) config_save_entry(i);
        ee_put(ADDR_MAGIC, CFG_MAGIC);
        return;
    }

    for (uint8_t i = 0; i < 4; i++) cfg.pass[i] = (char)EEPROM_read(ADDR_PASS + i);

    cfg.arm_time = EEPROM_read(ADDR_ARM);
    if (cfg.arm_time < TIME_MIN || cfg.arm_time > TIME_MAX) cfg.arm_time = TIME_DEFAULT;
    cfg.disarm_time = EEPROM_read(ADDR_DISARM);
    if (cfg.disarm_time < TIME_MIN || cfg.disarm_time > TIME_MAX) cfg.disarm_time = TIME_DEFAULT;

    for (uint8_t id = 0; id < MAX_ENTRIES; id++) {
        uint16_t base = ADDR_ENTRIES + (uint16_t)id * ENTRY_SIZE;
        cfg.entries[id].enabled = (EEPROM_read(base) == 1);
        for (uint8_t i = 0; i < ENTRY_NAME_MAX; i++) {
            cfg.entries[id].name[i] = (char)EEPROM_read(base + 1 + i);
        }
        cfg.entries[id].name[ENTRY_NAME_MAX] = '\0';
    }
}

uint8_t config_active_mask(void)
{
    uint8_t m = 0;
    for (uint8_t i = 0; i < MAX_ENTRIES; i++) {
        if (cfg.entries[i].enabled) m |= (uint8_t)(1 << i);
    }
    return m;
}
