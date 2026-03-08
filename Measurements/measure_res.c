#include "measure_res.h"

#include <stddef.h>
#include <string.h>

#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_mux4051.h"

enum {
    RES_SEL_AUTO = 0,
    RES_SEL_200,
    RES_SEL_2K,
    RES_SEL_20K,
    RES_SEL_200K,
    RES_SEL_COUNT
};

#define RES_SHORT_TH_MV 15u
#define RES_OPEN_TH_MV 80u
#define RES_OVR_TH_MV 5u

static const char *k_range_name[RES_SEL_COUNT] = {
    "AUTO",
    "200",
    "2K",
    "20K",
    "200K"
};

typedef struct {
    bool enabled;
    mux_res_range_t mux_range;
    float rref_ohm;
} range_cfg_t;

static const range_cfg_t k_range_cfg[RES_SEL_COUNT] = {
    [RES_SEL_AUTO] = {false, MUX_RES_200R, 0.0f},
    [RES_SEL_200] = {true, MUX_RES_200R, 100.0f},
    [RES_SEL_2K] = {true, MUX_RES_2K, 1000.0f},
    [RES_SEL_20K] = {true, MUX_RES_20K, 10000.0f},
    [RES_SEL_200K] = {true, MUX_RES_200K, 100000.0f}
};

static void sample_reset(res_live_sample_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->vdda_mv = 3300u;
    out->stat = RES_STAT_ERR;
    out->err = ERR_NOT_IMPL;
}

const char *measure_res_range_name(uint8_t range_sel)
{
    if (range_sel >= RES_SEL_COUNT) {
        return "UNK";
    }
    return k_range_name[range_sel];
}

bool measure_res_range_is_exp(uint8_t range_sel)
{
    return (range_sel == RES_SEL_200);
}

const char *measure_res_stat_name(res_live_stat_t stat)
{
    switch (stat) {
    case RES_STAT_OK:
        return "OK";
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

app_err_t measure_res_manual_sample(uint8_t range_sel, res_live_sample_t *out)
{
    app_err_t err;
    uint16_t raw = 0u;
    uint32_t mv = 0u;
    uint32_t vdda_mv = 3300u;
    float denom_mv;
    float r_ohm;
    const range_cfg_t *cfg;

    if ((out == NULL) || (range_sel >= RES_SEL_COUNT)) {
        return ERR_INVALID_ARG;
    }

    sample_reset(out);
    cfg = &k_range_cfg[range_sel];

    if (!cfg->enabled) {
        out->err = ERR_NOT_IMPL;
        out->stat = RES_STAT_ERR;
        return out->err;
    }

    mux_set_mode(MUX_MODE_RES);
    mux_set_res_range(cfg->mux_range);

    err = adc1_read_filtered(&raw, &mv);
    if (err != ERR_OK) {
        out->err = err;
        out->stat = RES_STAT_ERR;
        return err;
    }

    err = adc1_read_vdda_mv(&vdda_mv);
    if (err != ERR_OK) {
        vdda_mv = 3300u;
    }

    out->valid = true;
    out->raw_u16 = raw;
    out->mv = mv;
    out->vdda_mv = vdda_mv;

    if (mv <= RES_SHORT_TH_MV) {
        out->stat = RES_STAT_SHORT;
        out->r_ohm = 0.0f;
        out->err = ERR_OK;
        return ERR_OK;
    }

    if (vdda_mv <= (mv + RES_OVR_TH_MV)) {
        out->stat = RES_STAT_OVR;
        out->r_ohm = 0.0f;
        out->err = ERR_OVERRANGE;
        return out->err;
    }

    if (mv >= (vdda_mv - RES_OPEN_TH_MV)) {
        out->stat = RES_STAT_OPEN;
        out->r_ohm = 0.0f;
        out->err = ERR_OVERRANGE;
        return out->err;
    }

    denom_mv = (float)vdda_mv - (float)mv;
    if (denom_mv <= 0.0f) {
        out->stat = RES_STAT_OVR;
        out->r_ohm = 0.0f;
        out->err = ERR_OVERRANGE;
        return out->err;
    }

    r_ohm = cfg->rref_ohm * ((float)mv / denom_mv);

    out->stat = RES_STAT_OK;
    out->r_ohm = r_ohm;
    out->err = ERR_OK;
    return ERR_OK;
}
