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
    bool active;
    diode_sm_state_t state;
    uint32_t settle_deadline_ms;
    uint16_t sample_raw;
    uint32_t sample_mv;
    uint32_t sample_vdda_mv;
    app_err_t sample_err;
} diode_ctx_t;

typedef struct {
    bool valid;
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;
    uint32_t vf_mv;
    diode_stat_t stat;
    app_err_t err;
} diode_result_t;

#define DIODE_DRV_GPIO_Port GPIOB
#define DIODE_DRV_Pin GPIO_PIN_0
#define DIODE_SETTLE_MS 1u
#define DIODE_SHORT_TH_MV 50u
#define DIODE_OPEN_RATIO_NUM 95u
#define DIODE_OPEN_RATIO_DEN 100u

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

static inline app_err_t diode_step(diode_ctx_t *ctx, uint32_t now_ms, diode_result_t *out)
{
    uint8_t guard;
    uint32_t open_th_mv;
    app_err_t err;

    if ((ctx == NULL) || (out == NULL)) {
        return ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->stat = DIODE_STAT_PROBE;
    out->err = ERR_NOT_IMPL;
    out->vdda_mv = 3300u;

    if (!ctx->active) {
        diode_enter(ctx);
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
                out->stat = DIODE_STAT_PROBE;
                out->err = ERR_OK;
                return ERR_OK;
            }

            err = adc1_read_opamp1_filtered(&ctx->sample_raw, &ctx->sample_mv);
            if (err != ERR_OK) {
                ctx->sample_err = err;
                ctx->sample_vdda_mv = 3300u;
                ctx->state = DIODE_SM_STIM_OFF;
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
            out->raw_u16 = ctx->sample_raw;
            out->mv = ctx->sample_mv;
            out->vdda_mv = ctx->sample_vdda_mv;
            out->vf_mv = 0u;
            out->valid = (ctx->sample_err == ERR_OK);
            out->err = ctx->sample_err;

            if (ctx->sample_err != ERR_OK) {
                out->stat = DIODE_STAT_ERR;
            } else if (ctx->sample_mv < DIODE_SHORT_TH_MV) {
                out->stat = DIODE_STAT_SHORT;
            } else {
                open_th_mv = (ctx->sample_vdda_mv * DIODE_OPEN_RATIO_NUM) / DIODE_OPEN_RATIO_DEN;
                if (ctx->sample_mv > open_th_mv) {
                    out->stat = DIODE_STAT_OL;
                } else {
                    out->stat = DIODE_STAT_OK;
                    out->vf_mv = ctx->sample_mv;
                }
            }

            ctx->state = DIODE_SM_STIM_OFF;
            break;

        case DIODE_SM_STIM_OFF:
            diode_drv_off();
            ctx->state = DIODE_SM_STIM_ON;
            return out->err == ERR_NOT_IMPL ? ERR_OK : out->err;

        default:
            ctx->state = DIODE_SM_ENTER;
            break;
        }
    }

    out->stat = DIODE_STAT_ERR;
    out->err = ERR_HW_FAIL;
    return ERR_HW_FAIL;
}

#endif
