#ifndef MEASURE_VDC_H
#define MEASURE_VDC_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../App/app_types.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_error.h"
#include "../Drivers/drv_mux4051.h"

typedef enum {
    VDC_STAT_PROBE = 0,
    VDC_STAT_OK,
    VDC_STAT_OL,
    VDC_STAT_MUX_BAD,
    VDC_STAT_ADC_BAD,
    VDC_STAT_ERR
} vdc_status_t;

typedef enum {
    VDC_SM_ENTER = 0,
    VDC_SM_SETTLE,
    VDC_SM_MEASURE
} vdc_sm_state_t;

typedef struct {
    vdc_range_t range;
    uint16_t raw;
    uint32_t mv_sense;
    uint32_t vin_mv;
    uint32_t vdda_mv;
    vdc_status_t status;
    bool valid;
    app_err_t err;
    uint32_t last_update_ms;
} vdc_result_t;

typedef struct {
    bool active;
    vdc_sm_state_t state;
    vdc_range_t range;
    uint32_t settle_deadline_ms;
    uint32_t next_sample_ms;
    vdc_result_t result;
} vdc_ctx_t;

#define VDC_SAMPLE_PERIOD_MS 25u
#define VDC_SETTLE_MS 1u
#define VDC_FALLBACK_VDDA_MV 3300u
#define VDC_2V_OFFSET_MV 0u
#define VDC_20V_OFFSET_MV 0u
#define VDC_2V_OL_MV 2000u
#define VDC_20V_OL_MV 20000u

static inline const char *vdc_status_name(vdc_status_t status)
{
    switch (status) {
    case VDC_STAT_OK:
        return "OK";
    case VDC_STAT_OL:
        return "OL";
    case VDC_STAT_MUX_BAD:
        return "MUX BAD";
    case VDC_STAT_ADC_BAD:
        return "ADC BAD";
    case VDC_STAT_ERR:
        return "ERR";
    case VDC_STAT_PROBE:
    default:
        return "PROBE";
    }
}

static inline uint8_t vdc_expected_volt_ch(vdc_range_t range)
{
    return (range == VDC_RANGE_20V) ? 1u : 0u;
}

static inline mux_volt_range_t vdc_to_mux_range(vdc_range_t range)
{
    return (range == VDC_RANGE_20V) ? MUX_VOLT_20V : MUX_VOLT_2000MV;
}

static inline bool vdc_path_ok(vdc_range_t range)
{
    return (mux_get_volt_phys_ch() == vdc_expected_volt_ch(range));
}

static inline uint32_t vdc_convert_mv(vdc_range_t range, uint32_t mv_sense)
{
    if (range == VDC_RANGE_20V) {
        return (mv_sense * 800u + 60u) / 120u + VDC_20V_OFFSET_MV;
    }
    return mv_sense + VDC_2V_OFFSET_MV;
}

static inline void vdc_set_result(vdc_ctx_t *ctx,
                                  uint32_t now_ms,
                                  vdc_status_t status,
                                  app_err_t err,
                                  bool valid)
{
    if (ctx == NULL) {
        return;
    }

    ctx->result.status = status;
    ctx->result.err = err;
    ctx->result.valid = valid;
    if (!valid) {
        ctx->result.raw = 0u;
        ctx->result.mv_sense = 0u;
        ctx->result.vin_mv = 0u;
        ctx->result.vdda_mv = VDC_FALLBACK_VDDA_MV;
    }
    ctx->result.last_update_ms = now_ms;
}

static inline void vdc_apply_range(vdc_ctx_t *ctx, uint32_t now_ms)
{
    mux_set_volt_range(vdc_to_mux_range(ctx->range));
    adc1_mark_input_path_changed();

    ctx->state = VDC_SM_SETTLE;
    ctx->settle_deadline_ms = now_ms + VDC_SETTLE_MS;
}

static inline void vdc_init(vdc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->range = VDC_RANGE_2000MV;
    ctx->state = VDC_SM_ENTER;
    ctx->result.range = VDC_RANGE_2000MV;
    ctx->result.vdda_mv = VDC_FALLBACK_VDDA_MV;
    ctx->result.status = VDC_STAT_PROBE;
    ctx->result.err = ERR_NOT_IMPL;
}

static inline void vdc_enter(vdc_ctx_t *ctx, vdc_range_t range)
{
    if (ctx == NULL) {
        return;
    }

    vdc_init(ctx);
    if (range < VDC_RANGE_COUNT) {
        ctx->range = range;
        ctx->result.range = range;
    }
    ctx->active = true;
}

static inline void vdc_exit(vdc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->active = false;
    ctx->state = VDC_SM_ENTER;
}

static inline void vdc_set_range(vdc_ctx_t *ctx, vdc_range_t range)
{
    if ((ctx == NULL) || (range >= VDC_RANGE_COUNT)) {
        return;
    }

    if (ctx->range == range) {
        return;
    }

    ctx->range = range;
    ctx->result.range = range;

    if (ctx->active) {
        ctx->state = VDC_SM_ENTER;
        ctx->next_sample_ms = 0u;
    }
}

static inline app_err_t vdc_tick(vdc_ctx_t *ctx, uint32_t now_ms)
{
    app_err_t err;
    uint32_t vdda_mv;
    uint32_t vin_mv;
    uint16_t raw = 0u;
    uint32_t mv_sense = 0u;

    if (ctx == NULL) {
        return ERR_INVALID_ARG;
    }
    if (!ctx->active) {
        return ERR_OK;
    }

    switch (ctx->state) {
    case VDC_SM_ENTER:
        vdc_apply_range(ctx, now_ms);
        return ERR_OK;

    case VDC_SM_SETTLE:
        if ((int32_t)(now_ms - ctx->settle_deadline_ms) < 0) {
            return ERR_OK;
        }
        ctx->state = VDC_SM_MEASURE;
        ctx->next_sample_ms = now_ms;
        return ERR_OK;

    case VDC_SM_MEASURE:
    default:
        break;
    }

    if ((int32_t)(now_ms - ctx->next_sample_ms) < 0) {
        return ERR_OK;
    }
    ctx->next_sample_ms = now_ms + VDC_SAMPLE_PERIOD_MS;

    if (!vdc_path_ok(ctx->range)) {
        vdc_set_result(ctx, now_ms, VDC_STAT_MUX_BAD, ERR_HW_FAIL, false);
        return ERR_OK;
    }

    err = adc1_read_filtered(&raw, &mv_sense);
    if (err != ERR_OK) {
        vdc_set_result(ctx, now_ms, VDC_STAT_ADC_BAD, err, false);
        return ERR_OK;
    }

    vdda_mv = VDC_FALLBACK_VDDA_MV;
    err = adc1_read_vdda_mv(&vdda_mv);
    if (err != ERR_OK) {
        vdda_mv = VDC_FALLBACK_VDDA_MV;
    }

    vin_mv = vdc_convert_mv(ctx->range, mv_sense);
    ctx->result.valid = true;
    ctx->result.raw = raw;
    ctx->result.mv_sense = mv_sense;
    ctx->result.vin_mv = vin_mv;
    ctx->result.vdda_mv = vdda_mv;
    ctx->result.err = ERR_OK;
    ctx->result.last_update_ms = now_ms;

    if (((ctx->range == VDC_RANGE_2000MV) && (vin_mv >= VDC_2V_OL_MV)) ||
        ((ctx->range == VDC_RANGE_20V) && (vin_mv >= VDC_20V_OL_MV))) {
        ctx->result.status = VDC_STAT_OL;
    } else {
        ctx->result.status = VDC_STAT_OK;
    }

    return ERR_OK;
}

static inline app_err_t vdc_get_result(const vdc_ctx_t *ctx, vdc_result_t *out)
{
    if ((ctx == NULL) || (out == NULL)) {
        return ERR_INVALID_ARG;
    }

    *out = ctx->result;
    return ERR_OK;
}

#endif
