#include "measure_res.h"

#include <stddef.h>
#include <string.h>

#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_mux4051.h"
#include "../Drivers/drv_opamp_internal.h"

typedef struct {
    mux_res_range_t mux_range;
    res_range_param_t param;
} range_cfg_t;

static const char *k_range_name[RES_RANGE_SEL_COUNT] = {
    "AUTO",
    "200",
    "2K",
    "20K",
    "200K"
};

static const range_cfg_t k_range_cfg[RES_RANGE_SEL_COUNT] = {
    [RES_RANGE_SEL_AUTO] = {MUX_RES_200R, {.enabled = false, .exp_range = false, .rref_nom_ohm = 0.0f, .rref_eff_ohm = 0.0f}},
    [RES_RANGE_SEL_200] = {MUX_RES_200R, {.enabled = true, .exp_range = true, .rref_nom_ohm = 100.0f, .rref_eff_ohm = 100.0f}},
    [RES_RANGE_SEL_2K] = {MUX_RES_2K, {.enabled = true, .exp_range = false, .rref_nom_ohm = 1000.0f, .rref_eff_ohm = 1000.0f}},
    [RES_RANGE_SEL_20K] = {MUX_RES_20K, {.enabled = true, .exp_range = false, .rref_nom_ohm = 10000.0f, .rref_eff_ohm = 10000.0f}},
    [RES_RANGE_SEL_200K] = {MUX_RES_200K, {.enabled = true, .exp_range = false, .rref_nom_ohm = 100000.0f, .rref_eff_ohm = 100000.0f}}
};

static void sample_reset(res_sample_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->vdda_mv = 3300u;
    out->err = ERR_NOT_IMPL;
}

const char *measure_res_range_name(uint8_t range_sel)
{
    if (range_sel >= RES_RANGE_SEL_COUNT) {
        return "UNK";
    }
    return k_range_name[range_sel];
}

bool measure_res_range_is_exp(uint8_t range_sel)
{
    if (range_sel >= RES_RANGE_SEL_COUNT) {
        return false;
    }
    return k_range_cfg[range_sel].param.exp_range;
}

const res_range_param_t *measure_res_get_range_param(uint8_t range_sel)
{
    if (range_sel >= RES_RANGE_SEL_COUNT) {
        return NULL;
    }
    return &k_range_cfg[range_sel].param;
}

const char *measure_res_stat_name(res_live_stat_t stat)
{
    switch (stat) {
    case RES_STAT_OK:
        return "OK";
    case RES_STAT_AFE_BAD:
        return "AFE BAD";
    case RES_STAT_OPEN:
        return "OPEN";
    case RES_STAT_SHORT:
        return "SHORT";
    case RES_STAT_OVR:
        return "OVR";
    default:
        return "ERR";
    }
}

app_err_t res_acquire_sample(uint8_t range_sel, res_sample_t *s)
{
    app_err_t err;
    uint32_t vdda_mv = 3300u;
    const range_cfg_t *cfg;

    if ((s == NULL) || (range_sel >= RES_RANGE_SEL_COUNT)) {
        return ERR_INVALID_ARG;
    }

    sample_reset(s);
    cfg = &k_range_cfg[range_sel];

    if (!cfg->param.enabled) {
        s->err = ERR_NOT_IMPL;
        return s->err;
    }
    if (!opamp1_ready()) {
        s->err = ERR_HW_FAIL;
        return s->err;
    }

    mux_set_mode(MUX_MODE_RES);
    mux_set_res_range(cfg->mux_range);

    err = adc1_read_opamp1_filtered(&s->raw_u16, &s->mv);
    if (err != ERR_OK) {
        s->err = err;
        return err;
    }

    err = adc1_read_vdda_mv(&vdda_mv);
    if (err != ERR_OK) {
        vdda_mv = 3300u;
    }

    s->vdda_mv = vdda_mv;
    s->valid = true;
    s->err = ERR_OK;
    return ERR_OK;
}

app_err_t res_estimate_rx(uint8_t range_sel, const res_sample_t *s, float *r_ohm)
{
    float denom_mv;
    const range_cfg_t *cfg;

    if ((s == NULL) || (r_ohm == NULL) || (range_sel >= RES_RANGE_SEL_COUNT)) {
        return ERR_INVALID_ARG;
    }

    cfg = &k_range_cfg[range_sel];
    if (!cfg->param.enabled) {
        return ERR_NOT_IMPL;
    }
    if (!s->valid) {
        return s->err;
    }

    denom_mv = (float)s->vdda_mv - (float)s->mv;
    if (denom_mv <= 1.0f) {
        return ERR_OVERRANGE;
    }

    *r_ohm = cfg->param.rref_eff_ohm * ((float)s->mv / denom_mv);
    return ERR_OK;
}
