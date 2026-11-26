#ifndef CMD_H
#define CMD_H

/* Interprete de comandos por UART (9600 8N1, terminados en \r o \n):
 *   PASS (vieja) (nueva)   ARMADO [s]   DESARMADO [s]
 *   LIST   NEW (nombre)   DEL (id)   MOD (id) (nombre)
 *   LOG alarm              bitacora de alarmas (ID, nombre, hora)
 *   TIME [YYYY-MM-DD HH:MM:SS]   consulta / ajusta fecha y hora        */
void cmd_task(void);

#endif
