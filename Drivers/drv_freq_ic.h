#ifndef DRV_FREQ_IC_H
#define DRV_FREQ_IC_H

#include <stdbool.h>

#include "drv_error.h"

typedef struct {
    float inst_hz;
    float inst_duty;
    uint8_t hist_count;
    uint8_t invalid_count;
    bool capture_start_ok;
    app_err_t last_err;
} freq_debug_snapshot_t;

void freq_start(void);
app_err_t freq_get_hz(float *hz);
app_err_t freq_get_duty(float *duty_pct);
app_err_t freq_get(float *hz, float *duty_pct);
void freq_get_debug_snapshot(freq_debug_snapshot_t *out);

#endif
