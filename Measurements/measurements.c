#include "measurements.h"

#include <math.h>
#include <stddef.h>

#include "../BSP/bsp.h"
#include "../Drivers/drv_freq_ic.h"
#include "../Drivers/drv_mux4051.h"
#include "../Drivers/drv_adc_internal.h"

#define EPSILON_V 0.02f
#define CONT_HOLD_MS 120u

static const calib_profile_t *g_calib;

static bool g_cont_state;
static uint32_t g_cont_t0;

static app_err_t read_adc_raw_mv_filtered(uint16_t *raw, float *mv)
{
    uint16_t raw_code;
    uint32_t mv_u32;
    app_err_t err;

    if ((raw == NULL) || (mv == NULL)) {
        return ERR_INVALID_ARG;
    }

    err = adc1_read_filtered(&raw_code, &mv_u32);
    if (err != ERR_OK) {
        return err;
    }

    *raw = raw_code;
    *mv = (float)mv_u32;
    return ERR_OK;
}

static app_err_t read_adc_mv_filtered(float *mv)
{
    uint16_t raw_code;
    app_err_t err;

    if (mv == NULL) {
        return ERR_INVALID_ARG;
    }

    err = read_adc_raw_mv_filtered(&raw_code, mv);
    return err;
}

static float apply_linear(calib_linear_t l, float x)
{
    return l.k * x + l.b;
}

void measurements_init(const calib_profile_t *calib)
{
    g_calib = calib;
    g_cont_state = false;
    g_cont_t0 = 0u;
}

app_err_t meas_voltage_dc(vdc_range_t range, meas_result_t *out)
{
    float adc_mv;
    float value;
    app_err_t err;

    if ((out == NULL) || (g_calib == NULL) || (range >= VDC_RANGE_COUNT)) {
        return ERR_INVALID_ARG;
    }

    mux_set_volt_range((range == VDC_RANGE_20V) ? MUX_VOLT_20V : MUX_VOLT_2000MV);
    mux_set_mode(MUX_MODE_VOLTAGE);

    err = read_adc_mv_filtered(&adc_mv);
    if (err != ERR_OK) {
        out->err = err;
        return err;
    }

    if (range == VDC_RANGE_2000MV) {
        value = adc_mv * g_calib->v_div[VDC_RANGE_2000MV];
        value = apply_linear(g_calib->vdc[VDC_RANGE_2000MV], value);
        out->unit = UNIT_MV;
        out->flags = (fabsf(value) > 1999.0f) ? MEAS_FLAG_OVERRANGE : 0u;
    } else {
        value = (adc_mv / 1000.0f) * g_calib->v_div[VDC_RANGE_20V];
        value = apply_linear(g_calib->vdc[VDC_RANGE_20V], value);
        out->unit = UNIT_V;
        out->flags = (fabsf(value) > 20.0f) ? MEAS_FLAG_OVERRANGE : 0u;
    }

    if (value < 0.0f) {
        out->flags |= MEAS_FLAG_NEGATIVE;
    }

    out->value = value;
    out->err = ERR_OK;
    return ERR_OK;
}

app_err_t meas_resistance(res_range_t range, meas_result_t *out)
{
    float adc_mv;
    float vred;
    float rx;
    float denom;
    app_err_t err;

    if ((out == NULL) || (g_calib == NULL) || (range >= RES_RANGE_COUNT)) {
        return ERR_INVALID_ARG;
    }

    mux_set_res_range((mux_res_range_t)range);
    mux_set_mode(MUX_MODE_RES);

    err = read_adc_mv_filtered(&adc_mv);
    if (err != ERR_OK) {
        out->err = err;
        return err;
    }

    vred = adc_mv / 1000.0f;
    denom = g_calib->v_exc - vred;

    out->unit = UNIT_OHM;
    out->flags = 0u;

    if (denom < EPSILON_V) {
        out->value = 0.0f;
        out->flags = MEAS_FLAG_OPEN | MEAS_FLAG_OVERRANGE;
        out->err = ERR_OVERRANGE;
        return out->err;
    }

    rx = g_calib->r_ref_ohm[range] * (vred / denom);
    rx = apply_linear(g_calib->res[range], rx);

    out->value = rx;
    out->err = ERR_OK;
    return ERR_OK;
}

app_err_t meas_continuity(meas_result_t *out, bool *beep_on)
{
    meas_result_t r;
    app_err_t err;
    uint32_t now;

    if ((out == NULL) || (beep_on == NULL) || (g_calib == NULL)) {
        return ERR_INVALID_ARG;
    }

    err = meas_resistance(RES_RANGE_200, &r);
    *out = r;
    if ((err != ERR_OK) && (err != ERR_OVERRANGE)) {
        *beep_on = false;
        return err;
    }

    now = bsp_millis();

    if (!g_cont_state) {
        if (r.value < g_calib->continuity_on_ohm) {
            if (g_cont_t0 == 0u) {
                g_cont_t0 = now;
            } else if ((now - g_cont_t0) > CONT_HOLD_MS) {
                g_cont_state = true;
                g_cont_t0 = 0u;
            }
        } else {
            g_cont_t0 = 0u;
        }
    } else {
        if (r.value > g_calib->continuity_off_ohm) {
            if (g_cont_t0 == 0u) {
                g_cont_t0 = now;
            } else if ((now - g_cont_t0) > CONT_HOLD_MS) {
                g_cont_state = false;
                g_cont_t0 = 0u;
            }
        } else {
            g_cont_t0 = 0u;
        }
    }

    *beep_on = g_cont_state;
    return ERR_OK;
}

app_err_t meas_diode(meas_result_t *out)
{
    float adc_mv;
    float vf;
    app_err_t err;

    if ((out == NULL) || (g_calib == NULL)) {
        return ERR_INVALID_ARG;
    }

    mux_set_mode(MUX_MODE_DIODE);
    err = read_adc_mv_filtered(&adc_mv);
    if (err != ERR_OK) {
        out->err = err;
        return err;
    }

    vf = apply_linear(g_calib->diode, adc_mv / 1000.0f);
    out->value = vf;
    out->unit = UNIT_V;
    out->flags = 0u;
    if ((vf < 0.05f) || (vf > 1.5f)) {
        out->flags = MEAS_FLAG_NO_CONDUCTION;
    }
    out->err = ERR_OK;
    return ERR_OK;
}

app_err_t meas_freq_duty(freq_range_t range, meas_result_t *freq_out, meas_result_t *duty_out)
{
    float hz;
    float duty;
    app_err_t err;

    (void)range;

    if ((freq_out == NULL) || (duty_out == NULL)) {
        return ERR_INVALID_ARG;
    }

    err = freq_get(&hz, &duty);

    freq_out->value = hz;
    freq_out->unit = UNIT_HZ;
    freq_out->flags = 0u;
    freq_out->err = err;

    duty_out->value = duty;
    duty_out->unit = UNIT_PERCENT;
    duty_out->flags = 0u;
    duty_out->err = err;

    return err;
}

app_err_t meas_resistance_manual(res_range_t range, float rref_eff_ohm, float vref_v, res_manual_result_t *out)
{
    uint16_t raw_u16;
    float adc_mv;
    float vred_v;
    float denom;
    app_err_t err;

    if ((out == NULL) || (range >= RES_RANGE_COUNT) || (rref_eff_ohm <= 0.0f) || (vref_v <= 0.1f)) {
        return ERR_INVALID_ARG;
    }

    out->vred_v = 0.0f;
    out->rx_ohm = 0.0f;
    out->raw = 0;
    out->flags = 0u;
    out->err = ERR_OK;

    mux_set_mode(MUX_MODE_RES);
    mux_set_res_range((mux_res_range_t)range);

    err = read_adc_raw_mv_filtered(&raw_u16, &adc_mv);
    if (err != ERR_OK) {
        out->err = err;
        return err;
    }

    vred_v = adc_mv / 1000.0f;
    denom = vref_v - vred_v;

    out->raw = (int16_t)raw_u16;
    out->vred_v = vred_v;

    if (denom <= EPSILON_V) {
        out->flags = MEAS_FLAG_OPEN | MEAS_FLAG_OVERRANGE;
        out->err = ERR_OVERRANGE;
        return out->err;
    }

    out->rx_ohm = rref_eff_ohm * (vred_v / denom);
    out->err = ERR_OK;
    return ERR_OK;
}
