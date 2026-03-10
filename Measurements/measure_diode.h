#ifndef MEASURE_DIODE_H
#define MEASURE_DIODE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../Core/Inc/main.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_error.h"
#include "../Drivers/drv_mux4051.h"

typedef enum {
    DIODE_STAT_PROBE = 0,
    DIODE_STAT_OK,
    DIODE_STAT_OL,
    DIODE_STAT_SHORT,
    DIODE_STAT_ERR
} diode_stat_t;

typedef enum {
    DIODE_SM_ENTER = 0,
    DIODE_SM_STIM_ON,
    DIODE_SM_SAMPLE,
    DIODE_SM_EVAL,
    DIODE_SM_STIM_OFF
} diode_sm_state_t;

typedef struct {
    bool valid;
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;
    uint32_t vf_mv;
    diode_stat_t stat;
    app_err_t err;
} diode_result_t;

typedef struct {
    bool valid;
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;
    uint32_t vf_mv;
    diode_stat_t stat;
    app_err_t err;
    uint32_t last_update_ms;
} diode_latched_result_t;

typedef struct {
    bool active;
    diode_sm_state_t state;
    uint32_t settle_deadline_ms;
    uint16_t sample_raw;
    uint32_t sample_mv;
    uint32_t sample_vdda_mv;
    app_err_t sample_err;
    uint8_t confirm_count;
    diode_stat_t confirm_stat;
    diode_result_t pending_eval;
    uint32_t last_valid_eval_ms;
    diode_latched_result_t latched;
} diode_ctx_t;

#define DIODE_DRV_GPIO_Port GPIOB
#define DIODE_DRV_Pin GPIO_PIN_0
#define DIODE_SETTLE_MS 1u
#define DIODE_SHORT_TH_MV 50u
#define DIODE_OPEN_RATIO_NUM 95u
#define DIODE_OPEN_RATIO_DEN 100u
#define DIODE_CONFIRM_N 2u
#define DIODE_NO_RESULT_TIMEOUT_MS 500u

static inline void diode_drv_on(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    init.Pin = DIODE_DRV_Pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DIODE_DRV_GPIO_Port, &init);
    HAL_GPIO_WritePin(DIODE_DRV_GPIO_Port, DIODE_DRV_Pin, GPIO_PIN_SET);
}

static inline void diode_drv_off(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    init.Pin = DIODE_DRV_Pin;
    init.Mode = GPIO_MODE_ANALOG;
    init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DIODE_DRV_GPIO_Port, &init);
}

static inline void diode_init(diode_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->state = DIODE_SM_ENTER;
    ctx->sample_vdda_mv = 3300u;
    ctx->sample_err = ERR_NOT_IMPL;
    ctx->confirm_stat = DIODE_STAT_PROBE;
    ctx->latched.valid = false;
    ctx->latched.raw_u16 = 0u;
    ctx->latched.mv = 0u;
    ctx->latched.vdda_mv = 3300u;
    ctx->latched.vf_mv = 0u;
    ctx->latched.stat = DIODE_STAT_PROBE;
    ctx->latched.err = ERR_NOT_IMPL;
    ctx->latched.last_update_ms = HAL_GetTick();
}

static inline void diode_enter(diode_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    diode_init(ctx);
    mux_set_mode(MUX_MODE_DIODE);
    adc1_mark_input_path_changed();
    diode_drv_off();
    ctx->active = true;
}

static inline void diode_exit(diode_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    diode_drv_off();
    ctx->active = false;
    ctx->state = DIODE_SM_ENTER;
}

static inline app_err_t diode_get_latched_result(const diode_ctx_t *ctx, diode_latched_result_t *out)
{
    if ((ctx == NULL) || (out == NULL)) {
        return ERR_INVALID_ARG;
    }

    *out = ctx->latched;
    return ERR_OK;
}

static inline const char *diode_stat_name(diode_stat_t s)
{
    switch (s) {
    case DIODE_STAT_OK:
        return "OK";
    case DIODE_STAT_OL:
        return "OL";
    case DIODE_STAT_SHORT:
        return "SHORT";
    case DIODE_STAT_PROBE:
        return "PROBE";
    case DIODE_STAT_ERR:
    default:
        return "ERR";
    }
}

static inline void diode_commit_eval(diode_ctx_t *ctx, uint32_t now_ms, const diode_result_t *eval)
{
    if ((ctx == NULL) || (eval == NULL)) {
        return;
    }

    if (eval->valid) {
        ctx->last_valid_eval_ms = now_ms;
    }

    if ((ctx->confirm_count == 0u) || (ctx->confirm_stat != eval->stat)) {
        ctx->confirm_stat = eval->stat;
        ctx->confirm_count = 1u;
        ctx->pending_eval = *eval;
        return;
    }

    ctx->confirm_count++;
    ctx->pending_eval = *eval;

    if (ctx->confirm_count < DIODE_CONFIRM_N) {
        return;
    }

    ctx->latched.valid = eval->valid;
    ctx->latched.raw_u16 = eval->raw_u16;
    ctx->latched.mv = eval->mv;
    ctx->latched.vdda_mv = eval->vdda_mv;
    ctx->latched.vf_mv = eval->vf_mv;
    ctx->latched.stat = eval->stat;
    ctx->latched.err = eval->err;
    ctx->latched.last_update_ms = now_ms;
    ctx->confirm_count = 0u;
}

static inline app_err_t diode_step(diode_ctx_t *ctx, uint32_t now_ms, diode_result_t *out)
{
    uint8_t guard;
    uint32_t open_th_mv;
    diode_result_t eval;
    app_err_t err;

    if ((ctx == NULL) || (out == NULL)) {
        return ERR_INVALID_ARG;
    }

    if (!ctx->active) {
        diode_enter(ctx);
    }

    if ((ctx->latched.stat != DIODE_STAT_PROBE) &&
        (ctx->last_valid_eval_ms != 0u) &&
        ((int32_t)(now_ms - (ctx->last_valid_eval_ms + DIODE_NO_RESULT_TIMEOUT_MS)) >= 0)) {
        ctx->latched.valid = false;
        ctx->latched.raw_u16 = 0u;
        ctx->latched.mv = 0u;
        ctx->latched.vdda_mv = 3300u;
        ctx->latched.vf_mv = 0u;
        ctx->latched.stat = DIODE_STAT_PROBE;
        ctx->latched.err = ERR_NO_SIGNAL;
        ctx->latched.last_update_ms = now_ms;
        ctx->confirm_count = 0u;
    }

    for (guard = 0u; guard < 8u; guard++) {
        switch (ctx->state) {
        case DIODE_SM_ENTER:
            mux_set_mode(MUX_MODE_DIODE);
            adc1_mark_input_path_changed();
            diode_drv_off();
            ctx->state = DIODE_SM_STIM_ON;
            break;

        case DIODE_SM_STIM_ON:
            diode_drv_on();
            ctx->settle_deadline_ms = now_ms + DIODE_SETTLE_MS;
            ctx->state = DIODE_SM_SAMPLE;
            break;

        case DIODE_SM_SAMPLE:
            if ((int32_t)(now_ms - ctx->settle_deadline_ms) < 0) {
                goto step_done;
            }

            err = adc1_read_opamp1_filtered(&ctx->sample_raw, &ctx->sample_mv);
            if (err != ERR_OK) {
                ctx->sample_err = err;
                ctx->sample_vdda_mv = 3300u;
                ctx->state = DIODE_SM_EVAL;
                break;
            }

            err = adc1_read_vdda_mv(&ctx->sample_vdda_mv);
            if (err != ERR_OK) {
                ctx->sample_vdda_mv = 3300u;
            }
            ctx->sample_err = ERR_OK;
            ctx->state = DIODE_SM_EVAL;
            break;

        case DIODE_SM_EVAL:
            eval.valid = (ctx->sample_err == ERR_OK);
            eval.raw_u16 = ctx->sample_raw;
            eval.mv = ctx->sample_mv;
            eval.vdda_mv = ctx->sample_vdda_mv;
            eval.vf_mv = 0u;
            eval.err = ctx->sample_err;

            if (ctx->sample_err != ERR_OK) {
                eval.stat = DIODE_STAT_ERR;
            } else if (ctx->sample_mv < DIODE_SHORT_TH_MV) {
                eval.stat = DIODE_STAT_SHORT;
            } else {
                open_th_mv = (ctx->sample_vdda_mv * DIODE_OPEN_RATIO_NUM) / DIODE_OPEN_RATIO_DEN;
                if (ctx->sample_mv > open_th_mv) {
                    eval.stat = DIODE_STAT_OL;
                } else {
                    eval.stat = DIODE_STAT_OK;
                    eval.vf_mv = ctx->sample_mv;
                }
            }

            diode_commit_eval(ctx, now_ms, &eval);
            ctx->state = DIODE_SM_STIM_OFF;
            break;

        case DIODE_SM_STIM_OFF:
            diode_drv_off();
            ctx->state = DIODE_SM_STIM_ON;
            goto step_done;

        default:
            ctx->state = DIODE_SM_ENTER;
            break;
        }
    }

step_done:
    out->valid = ctx->latched.valid;
    out->raw_u16 = ctx->latched.raw_u16;
    out->mv = ctx->latched.mv;
    out->vdda_mv = ctx->latched.vdda_mv;
    out->vf_mv = ctx->latched.vf_mv;
    out->stat = ctx->latched.stat;
    out->err = ctx->latched.err;

    return ERR_OK;
}

#endif
