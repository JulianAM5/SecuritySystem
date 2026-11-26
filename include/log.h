#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <time.h>
#include "board.h"

#define LOG_MAX 16      /* registros (bitacora circular: se sobreescribe el mas viejo) */

typedef struct {
    uint8_t id;                         /* ID de la entrada (bit en el PCF8574) */
    char    name[ENTRY_NAME_MAX + 1];   /* nombre al momento de la alarma       */
    time_t  ts;                         /* fecha/hora de la alarma (epoch 2000) */
    uint8_t synced;                     /* 1 = fecha/hora real, 0 = seg. desde arranque */
} log_entry_t;

void    log_init(void);
void    log_alarm_add(uint8_t id, const char *name);   /* registra con la hora actual */
uint8_t log_count(void);
/* n = 0 es el registro mas reciente. Retorna 1 si existe */
uint8_t log_get(uint8_t n, log_entry_t *e);

#endif
