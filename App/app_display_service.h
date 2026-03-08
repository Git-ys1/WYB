#ifndef APP_DISPLAY_SERVICE_H
#define APP_DISPLAY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "../Drivers/drv_error.h"

typedef struct {
    char line[8][22];
} app_ui_frame_t;

app_err_t app_display_init(void);
bool app_display_ready(void);
app_err_t app_display_last_err(void);

app_err_t app_display_show_boot(void);
app_err_t app_display_show_adc_debug(uint16_t raw, bool raw_valid,
                                     uint32_t mv, bool mv_valid,
                                     uint32_t vdda_mv, bool vdda_valid,
                                     app_err_t stat);
app_err_t app_display_show_menu_frame(const app_ui_frame_t *frame);
app_err_t app_display_show_fallback(const char *err_tag);

#endif
