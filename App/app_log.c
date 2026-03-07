#include "app_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../BSP/bsp.h"

typedef struct {
    app_log_entry_t items[APP_LOG_CAPACITY];
    uint8_t head;
    uint8_t count;
} app_log_ring_t;

static app_log_ring_t g_log;

static char level_char(log_level_t level)
{
    if (level == LOG_LVL_W) {
        return 'W';
    }
    if (level == LOG_LVL_E) {
        return 'E';
    }
    return 'I';
}

void app_log_init(void)
{
    memset(&g_log, 0, sizeof(g_log));
}

void app_log_push(log_level_t level, const char *fmt, ...)
{
    app_log_entry_t *slot;
    va_list args;
    char msg[APP_LOG_MSG_LEN + 1u];
    char line[48];

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    slot = &g_log.items[g_log.head];
    slot->ms = bsp_millis();
    slot->level = level;
    (void)strncpy(slot->message, msg, APP_LOG_MSG_LEN);
    slot->message[APP_LOG_MSG_LEN] = '\0';

    g_log.head = (uint8_t)((g_log.head + 1u) % APP_LOG_CAPACITY);
    if (g_log.count < APP_LOG_CAPACITY) {
        g_log.count++;
    }

    (void)snprintf(
        line,
        sizeof(line),
        "%04lu %c %.24s",
        (unsigned long)(slot->ms % 10000u),
        level_char(level),
        slot->message
    );
    bsp_debug_log(line);
}

uint8_t app_log_snapshot(app_log_entry_t *out, uint8_t max_entries)
{
    uint8_t count;
    uint8_t start;
    uint8_t i;

    if ((out == NULL) || (max_entries == 0u) || (g_log.count == 0u)) {
        return 0u;
    }

    count = max_entries;
    if (count > g_log.count) {
        count = g_log.count;
    }

    start = (uint8_t)((g_log.head + APP_LOG_CAPACITY - count) % APP_LOG_CAPACITY);
    for (i = 0u; i < count; i++) {
        out[i] = g_log.items[(uint8_t)((start + i) % APP_LOG_CAPACITY)];
    }

    return count;
}
