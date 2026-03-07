#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include <stdbool.h>

#include "../App/app_types.h"
#include "../Calib/calib.h"
#include "../Drivers/drv_adc_internal.h"

void measurements_init(const calib_profile_t *calib);

app_err_t meas_voltage_dc(vdc_range_t range, meas_result_t *out);
app_err_t meas_resistance(res_range_t range, meas_result_t *out);
app_err_t meas_continuity(meas_result_t *out, bool *beep_on);
app_err_t meas_diode(meas_result_t *out);
app_err_t meas_freq_duty(freq_range_t range, meas_result_t *freq_out, meas_result_t *duty_out);

typedef struct {
    float vred_v;
    float rx_ohm;
    int16_t raw;
    app_err_t err;
    uint32_t flags;
} res_manual_result_t;

app_err_t meas_resistance_manual(res_range_t range, float rref_eff_ohm, float vref_v, res_manual_result_t *out);

#endif
