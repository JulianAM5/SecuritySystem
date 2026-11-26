#include "cmd.h"
#include "UART.h"
#include "config.h"
#include "inputs.h"
#include "log.h"
#include "clock.h"
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


/* Formatea fecha/hora: "YYYY-MM-DD HH:MM:SS" o "T+H:MM:SS" (desde arranque) */
static void format_time(time_t ts, uint8_t synced, char *buf, uint8_t size)
{
    if (synced) {
        struct tm tm;
        gmtime_r(&ts, &tm);
        snprintf(buf, size, "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)(tm.tm_year + 1900), (unsigned)(tm.tm_mon + 1),
                 (unsigned)tm.tm_mday, (unsigned)tm.tm_hour,
                 (unsigned)tm.tm_min, (unsigned)tm.tm_sec);
    } else {
        uint32_t s = (uint32_t)ts;
        snprintf(buf, size, "T+%u:%02u:%02u", (unsigned)(s / 3600UL),
                 (unsigned)((s % 3600UL) / 60UL), (unsigned)(s % 60UL));
    }
}

/* LOG alarm: bitacora de alarmas (mas reciente primero) */
static void cmd_log(char *args)
{
    char *t = next_token(&args);
    char buf[72], when[24];
    log_entry_t e;

    if (!t || next_token(&args)) { resp_err(); return; }
    for (char *c = t; *c; c++) *c = (char)toupper((unsigned char)*c);
    if (strcmp(t, "ALARM") != 0) { resp_err(); return; }

    UART_string("| # | ID | NOMBRE | FECHA Y HORA |\r\n");
    for (uint8_t n = 0; n < log_count(); n++) {
        if (!log_get(n, &e)) break;
        format_time(e.ts, e.synced, when, sizeof(when));
        snprintf(buf, sizeof(buf), "| %u | %u | %s | %s |\r\n",
                 (unsigned)(n + 1), (unsigned)e.id, e.name, when);
        UART_string(buf);
    }
    resp_ok();
}

/* Lee exactamente n digitos ASCII */
static uint8_t read_digits(const char *s, uint8_t n, uint16_t *out)
{
    uint16_t v = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = (uint16_t)(v * 10 + (s[i] - '0'));
    }
    *out = v;
    return 1;
}

/* TIME                          -> consulta
 * TIME YYYY-MM-DD HH:MM:SS      -> ajusta fecha y hora */
static void cmd_clock(char *args)
{
    char *d = next_token(&args);
    char *h = next_token(&args);
    char buf[32];

    if (!d) {
        format_time(clock_now(), clock_synced(), buf, sizeof(buf));
        UART_string(buf);
        UART_string("\r\n");
        return;
    }

    uint16_t y, mo, dd, hh, mi, ss;
    if (!h || next_token(&args) ||
        strlen(d) != 10 || d[4] != '-' || d[7] != '-' ||
        strlen(h) != 8  || h[2] != ':' || h[5] != ':' ||
        !read_digits(d, 4, &y) || !read_digits(d + 5, 2, &mo) || !read_digits(d + 8, 2, &dd) ||
        !read_digits(h, 2, &hh) || !read_digits(h + 3, 2, &mi) || !read_digits(h + 6, 2, &ss) ||
        y < 2000 || y > 2099 || mo < 1 || mo > 12 || dd < 1 ||
        hh > 23 || mi > 59 || ss > 59) {
        resp_err();
        return;
    }

    struct tm tm, chk;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = (int)y - 1900;
    tm.tm_mon  = (int)mo - 1;
    tm.tm_mday = (int)dd;
    tm.tm_hour = (int)hh;
    tm.tm_min  = (int)mi;
    tm.tm_sec  = (int)ss;
    time_t ts = mktime(&tm);

    gmtime_r(&ts, &chk);                  /* detectar fechas inexistentes (30 feb, etc.) */
    if (chk.tm_mday != (int)dd || chk.tm_mon != (int)mo - 1) { resp_err(); return; }

    clock_set(ts);
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
    else if (!strcmp(cmd, "LOG"))       cmd_log(args);
    else if (!strcmp(cmd, "TIME"))      cmd_clock(args);
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
