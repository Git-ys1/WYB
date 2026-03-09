#include "measure_res.h"

#include <stddef.h>
#include <string.h>

#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_mux4051.h"
#include "../Drivers/drv_opamp_internal.h"

typedef struct {
    const char *range_name;
    mux_res_range_t mux_range;
    uint8_t mux_idx;
    res_formatter_id_t formatter_id;
    res_range_param_t param;
} range_cfg_t;

static const range_cfg_t k_range_cfg[RES_RANGE_SEL_COUNT] = {
    [RES_RANGE_SEL_AUTO] = {
        .range_name = "AUTO",
        .mux_range = MUX_RES_200R,
        .mux_idx = 0u,
        .formatter_id = RES_FMT_NONE,
        .param = {.enabled = false, .exp_range = false, .rref_nom_ohm = 0.0f, .rref_eff_ohm = 0.0f, .gain_corr = 1.0f}
    },
    [RES_RANGE_SEL_200] = {
        .range_name = "200",
        .mux_range = MUX_RES_200R,
        .mux_idx = 0u,
        .formatter_id = RES_FMT_200,
        .param = {.enabled = true, .exp_range = true, .rref_nom_ohm = 1000.0f, .rref_eff_ohm = 1000.0f, .gain_corr = 1.0f}
    },
    [RES_RANGE_SEL_2K] = {
        .range_name = "2K",
        .mux_range = MUX_RES_2K,
        .mux_idx = 1u,
        .formatter_id = RES_FMT_2K,
        .param = {.enabled = true, .exp_range = false, .rref_nom_ohm = 10000.0f, .rref_eff_ohm = 10000.0f, .gain_corr = 1.0f}
    },
    [RES_RANGE_SEL_20K] = {
        .range_name = "20K",
        .mux_range = MUX_RES_20K,
        .mux_idx = 2u,
        .formatter_id = RES_FMT_20K,
        .param = {.enabled = true, .exp_range = false, .rref_nom_ohm = 100000.0f, .rref_eff_ohm = 100000.0f, .gain_corr = 1.0f}
    },
    [RES_RANGE_SEL_200K] = {
        .range_name = "200K",
        .mux_range = MUX_RES_200K,
        .mux_idx = 3u,
        .formatter_id = RES_FMT_200K,
        .param = {.enabled = true, .exp_range = false, .rref_nom_ohm = 1000000.0f, .rref_eff_ohm = 1000000.0f, .gain_corr = 1.0f}
    }
};
static uint8_t s_last_range_sel = 0xFFu;

#define RES_SHORT_OHM_TH 3.0f
#define RES_OPEN_MARGIN_MV 80u

static float res_range_max_ohm(uint8_t range_sel)
{
    switch (range_sel) {
    case RES_RANGE_SEL_200:
        return 199.9f;
    case RES_RANGE_SEL_2K:
        return 1999.0f;
    case RES_RANGE_SEL_20K:
        return 19990.0f;
    case RES_RANGE_SEL_200K:
        return 199900.0f;
    default:
        return 0.0f;
    }
}

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
    return k_range_cfg[range_sel].range_name;
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

bool measure_res_get_binding(uint8_t range_sel, res_range_binding_t *out)
{
    const range_cfg_t *cfg;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (out == NULL)) {
        return false;
    }

    cfg = &k_range_cfg[range_sel];
    out->range_name = cfg->range_name;
    out->mux_idx = cfg->mux_idx;
    out->formatter_id = cfg->formatter_id;
    out->param = cfg->param;
    return true;
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

res_live_stat_t measure_res_classify_display_state(uint8_t range_sel,
                                                   const res_sample_t *s,
                                                   bool calc_ok,
                                                   float r_calc_ohm)
{
    float max_ohm;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (range_sel == RES_RANGE_SEL_AUTO) || (s == NULL)) {
        return RES_STAT_ERR;
    }
    if (!s->valid) {
        return RES_STAT_ERR;
    }

    if ((s->vdda_mv > RES_OPEN_MARGIN_MV) && (s->mv >= (s->vdda_mv - RES_OPEN_MARGIN_MV))) {
        return RES_STAT_OPEN;
    }

    if (!calc_ok) {
        return RES_STAT_OVR;
    }

    if (r_calc_ohm <= RES_SHORT_OHM_TH) {
        return RES_STAT_SHORT;
    }

    max_ohm = res_range_max_ohm(range_sel);
    if ((max_ohm > 0.0f) && (r_calc_ohm > max_ohm)) {
        return RES_STAT_OVR;
    }

    return RES_STAT_OK;
}

app_err_t res_acquire_sample(uint8_t range_sel, res_sample_t *s)
{
    app_err_t err;
    uint32_t vdda_mv = 3300u;
    const range_cfg_t *cfg;
    bool path_changed;

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

    path_changed = (s_last_range_sel != range_sel);
    mux_set_mode(MUX_MODE_RES);
    mux_set_res_range(cfg->mux_range);
    if (path_changed) {
        adc1_mark_input_path_changed();
        s_last_range_sel = range_sel;
    }

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

app_err_t res_estimate_rx(uint8_t range_sel, const res_sample_t *s, float *r_calc_ohm)
{
    float denom_mv;
    const range_cfg_t *cfg;

    if ((s == NULL) || (r_calc_ohm == NULL) || (range_sel >= RES_RANGE_SEL_COUNT)) {
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

    *r_calc_ohm = cfg->param.rref_eff_ohm * cfg->param.gain_corr * ((float)s->mv / denom_mv);
    return ERR_OK;
}
