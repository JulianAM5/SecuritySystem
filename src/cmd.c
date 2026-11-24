#include "cmd.h"
#include "UART.h"
#include "config.h"
#include "inputs.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define CMD_LINE_MAX 40

static char    line[CMD_LINE_MAX];
static uint8_t len = 0;
static uint8_t overflow = 0;

static void resp_ok(void)  { UART_string("OK\r\n"); }
static void resp_err(void) { UART_string("ERROR\r\n"); }

/* ---------- Utilidades de parseo ---------- */

/* Salta espacios y regresa el siguiente token (modifica la cadena) */
static char *next_token(char **p)
{
    while (**p == ' ' || **p == '\t') (*p)++;
    if (**p == '\0') return NULL;
    char *start = *p;
    while (**p && **p != ' ' && **p != '\t') (*p)++;
    if (**p) { **p = '\0'; (*p)++; }
    return start;
}

/* Resto de la linea como nombre (sin espacios al inicio/fin). NULL si invalido */
static char *get_name(char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t')) p[--n] = '\0';
    if (n == 0 || n > ENTRY_NAME_MAX) return NULL;
    return p;
}

static uint8_t parse_uint(const char *s, uint8_t *out)
{
    uint16_t v = 0;
    uint8_t n = 0;
    if (!s || !*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = (uint16_t)(v * 10 + (*s - '0'));
        if (++n > 3) return 0;
        s++;
    }
    if (v > 255) return 0;
    *out = (uint8_t)v;
    return 1;
}

static uint8_t is_pass(const char *s)
{
    if (!s || strlen(s) != 4) return 0;
    for (uint8_t i = 0; i < 4; i++) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* ---------- Comandos ---------- */

static void cmd_pass(char *args)
{
    char *old_p = next_token(&args);
    char *new_p = next_token(&args);

    if (!is_pass(old_p) || !is_pass(new_p) || next_token(&args) ||
        memcmp(old_p, cfg.pass, 4) != 0) {
        resp_err();
        return;
    }
    memcpy(cfg.pass, new_p, 4);
    config_save_pass();
    resp_ok();
}

static void cmd_time(char *args, uint8_t *field, void (*save)(void))
{
    char *t = next_token(&args);
    uint8_t v;

    if (!t) {                                   /* sin parametros: regresar valor actual */
        char buf[8];
        snprintf(buf, sizeof(buf), "%u\r\n", (unsigned)*field);
        UART_string(buf);
        return;
    }
    if (!parse_uint(t, &v) || v < TIME_MIN || v > TIME_MAX || next_token(&args)) {
        resp_err();
        return;
    }
    *field = v;
    save();
    resp_ok();
}

static void cmd_list(void)
{
    char buf[48];
    uint8_t st = inputs_state();

    UART_string("| ID | NOMBRE | ESTADO |\r\n");
    for (uint8_t i = 0; i < MAX_ENTRIES; i++) {
        if (!cfg.entries[i].enabled) continue;
        snprintf(buf, sizeof(buf), "| %u | %s | %s |\r\n",
                 (unsigned)i, cfg.entries[i].name,
                 (st & (1 << i)) ? "ABIERTO" : "CERRADO");
        UART_string(buf);
    }
    resp_ok();
}

static void cmd_new(char *args)
{
    char *name = get_name(args);
    if (!name) { resp_err(); return; }

    for (uint8_t i = 0; i < MAX_ENTRIES; i++) {
        if (!cfg.entries[i].enabled) {          /* primer ID libre */
            memset(cfg.entries[i].name, 0, sizeof(cfg.entries[i].name));
            strcpy(cfg.entries[i].name, name);
            cfg.entries[i].enabled = 1;
            config_save_entry(i);
            resp_ok();
            return;
        }
    }
    resp_err();                                  /* ya hay 8 entradas */
}

static void cmd_del(char *args)
{
    uint8_t id;
    char *t = next_token(&args);

    if (!parse_uint(t, &id) || next_token(&args) ||
        id >= MAX_ENTRIES || !cfg.entries[id].enabled) {
        resp_err();
        return;
    }
    cfg.entries[id].enabled = 0;
    memset(cfg.entries[id].name, 0, sizeof(cfg.entries[id].name));
    config_save_entry(id);
    resp_ok();
}

static void cmd_mod(char *args)
{
    uint8_t id;
    char *t = next_token(&args);
    char *name;

    if (!parse_uint(t, &id) || id >= MAX_ENTRIES || !cfg.entries[id].enabled) {
        resp_err();
        return;
    }
    name = get_name(args);
    if (!name) { resp_err(); return; }

    memset(cfg.entries[id].name, 0, sizeof(cfg.entries[id].name));
    strcpy(cfg.entries[id].name, name);
    config_save_entry(id);
    resp_ok();
}

static void process_line(char *s)
{
    char *args = s;
    char *cmd = next_token(&args);
    if (!cmd) return;

    for (char *c = cmd; *c; c++) *c = (char)toupper((unsigned char)*c);

    if      (!strcmp(cmd, "PASS"))      cmd_pass(args);
    else if (!strcmp(cmd, "ARMADO"))    cmd_time(args, &cfg.arm_time, config_save_arm_time);
    else if (!strcmp(cmd, "DESARMADO")) cmd_time(args, &cfg.disarm_time, config_save_disarm_time);
    else if (!strcmp(cmd, "LIST"))      cmd_list();
    else if (!strcmp(cmd, "NEW"))       cmd_new(args);
    else if (!strcmp(cmd, "DEL"))       cmd_del(args);
    else if (!strcmp(cmd, "MOD"))       cmd_mod(args);
    else                                resp_err();
}

void cmd_task(void)
{
    unsigned char c;

    while ((c = UART_read()) != 0) {
        if (c == '\r' || c == '\n') {
            if (overflow)  resp_err();
            else if (len)  { line[len] = '\0'; process_line(line); }
            len = 0;
            overflow = 0;
            continue;
        }
        if (len < CMD_LINE_MAX - 1) line[len++] = (char)c;
        else                        overflow = 1;
    }
}
