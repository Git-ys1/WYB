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
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;
    app_err_t err;
} res_sample_t;

typedef struct {
    bool enabled;
    bool exp_range;
    float rref_nom_ohm;
    float rref_eff_ohm;
} res_range_param_t;

app_err_t res_acquire_sample(uint8_t range_sel, res_sample_t *s);
app_err_t res_estimate_rx(uint8_t range_sel, const res_sample_t *s, float *r_ohm);
const char *measure_res_range_name(uint8_t range_sel);
bool measure_res_range_is_exp(uint8_t range_sel);
const res_range_param_t *measure_res_get_range_param(uint8_t range_sel);
const char *measure_res_stat_name(res_live_stat_t stat);

#endif
