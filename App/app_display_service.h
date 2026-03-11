#ifndef APP_DISPLAY_SERVICE_H
#define APP_DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "../Drivers/drv_error.h"

typedef struct {
    char line[8][22];
} app_ui_frame_t;

app_err_t app_display_init_once(void);
void app_display_poll(void);
bool app_display_ready(void);
app_err_t app_display_last_err(void);
app_err_t app_display_render(const app_ui_frame_t *frame);
app_err_t app_display_render_fault(uint8_t fault_code, uint8_t stage);
app_err_t app_display_show_boot_splash(void);

#endif
