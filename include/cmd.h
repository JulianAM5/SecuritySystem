#ifndef CMD_H
#define CMD_H

/* Interprete de comandos por UART (9600 8N1, terminados en \r o \n):
 *   PASS (vieja) (nueva)   ARMADO [s]   DESARMADO [s]
 *   LIST   NEW (nombre)   DEL (id)   MOD (id) (nombre)          */
void cmd_task(void);

#endif
