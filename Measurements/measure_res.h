#ifndef MEASURE_RES_H
#define MEASURE_RES_H

#include <stdbool.h>
#include <stdint.h>

#include "../Drivers/drv_error.h"

typedef enum {
    RES_RANGE_SEL_AUTO = 0,
    RES_RANGE_SEL_200,
    RES_RANGE_SEL_2K,
    RES_RANGE_SEL_20K,
    RES_RANGE_SEL_200K,
    RES_RANGE_SEL_COUNT
} res_range_sel_t;

typedef enum {
    RES_STAT_OK = 0,
    RES_STAT_AFE_BAD,
    RES_STAT_OPEN,
    RES_STAT_SHORT,
    RES_STAT_OVR,
    RES_STAT_ERR
} res_live_stat_t;

typedef struct {
    bool valid;
    uint16_t raw_u16;  /* ADC count */
    uint32_t mv;       /* mV */
    uint32_t vdda_mv;  /* mV */
    app_err_t err;
} res_sample_t;

typedef struct {
    bool enabled;
    bool exp_range;
    float rref_nom_ohm;  /* Ohm */
    float rref_eff_ohm;  /* Ohm */
    float gain_corr;     /* unitless gain correction, default 1.0 */
} res_range_param_t;

typedef enum {
    RES_FMT_NONE = 0,
    RES_FMT_200,
    RES_FMT_2K,
    RES_FMT_20K,
    RES_FMT_200K
} res_formatter_id_t;

typedef struct {
    const char *range_name;
    uint8_t mux_idx;
    res_formatter_id_t formatter_id;
    res_range_param_t param;
} res_range_binding_t;

app_err_t res_acquire_sample(uint8_t range_sel, res_sample_t *s);
app_err_t res_estimate_rx(uint8_t range_sel, const res_sample_t *s, float *r_calc_ohm); /* Ohm */
const char *measure_res_range_name(uint8_t range_sel);
bool measure_res_range_is_exp(uint8_t range_sel);
const res_range_param_t *measure_res_get_range_param(uint8_t range_sel);
bool measure_res_get_binding(uint8_t range_sel, res_range_binding_t *out);
const char *measure_res_stat_name(res_live_stat_t stat);

#endif
