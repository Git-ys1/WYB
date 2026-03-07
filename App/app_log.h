#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdint.h>

#define APP_LOG_CAPACITY 32u
#define APP_LOG_MSG_LEN 24u

typedef enum {
    LOG_LVL_I = 0,
    LOG_LVL_W,
    LOG_LVL_E
} log_level_t;

typedef struct {
    uint32_t ms;
    log_level_t level;
    char message[APP_LOG_MSG_LEN + 1u];
} app_log_entry_t;

void app_log_init(void);
void app_log_push(log_level_t level, const char *fmt, ...);
uint8_t app_log_snapshot(app_log_entry_t *out, uint8_t max_entries);

#define LOGI(fmt, ...) app_log_push(LOG_LVL_I, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) app_log_push(LOG_LVL_W, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) app_log_push(LOG_LVL_E, fmt, ##__VA_ARGS__)

#endif
