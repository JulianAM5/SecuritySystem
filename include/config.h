#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include "board.h"

typedef struct {
    uint8_t enabled;                    /* 1 = entrada registrada/activa */
    char    name[ENTRY_NAME_MAX + 1];
} entry_t;

typedef struct {
    char     pass[4];                   /* 4 digitos ASCII */
    uint8_t  arm_time;                  /* tiempo de armado (salida), s    */
    uint8_t  disarm_time;               /* tiempo de desarmado (entrada), s*/
    entry_t  entries[MAX_ENTRIES];      /* el indice = bit en el PCF8574   */
} config_t;

extern config_t cfg;                    /* copia en RAM de la EEPROM */

void    config_load(void);              /* carga (o inicializa) desde EEPROM */
void    config_save_pass(void);
void    config_save_arm_time(void);
void    config_save_disarm_time(void);
void    config_save_entry(uint8_t id);
void    config_ee_put(uint16_t addr, uint8_t value);  /* escritura segura en EEPROM */
uint8_t config_active_mask(void);       /* bit i = 1 si la entrada i esta activa */

#endif
