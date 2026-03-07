#ifndef DRV_FREQ_IC_H
#define DRV_FREQ_IC_H

#include "drv_error.h"

void freq_start(void);
app_err_t freq_get_hz(float *hz);
app_err_t freq_get_duty(float *duty_pct);
app_err_t freq_get(float *hz, float *duty_pct);

#endif
