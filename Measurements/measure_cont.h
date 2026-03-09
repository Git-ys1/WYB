#ifndef MEASURE_CONT_H
#define MEASURE_CONT_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../Drivers/drv_error.h"
#include "measure_res.h"

typedef enum {
    CONT_STATE_OPEN = 0,
    CONT_STATE_BEEP_ON,
    CONT_STATE_BEEP_OFF_WAIT,
    CONT_STATE_ERR
} cont_state_t;

typedef struct {
    cont_state_t state;
    uint8_t vote_enter;
    uint8_t vote_exit;
    uint32_t settle_deadline_ms;
} cont_ctx_t;

typedef struct {
    bool sample_valid;
    bool calc_ok;
    bool beep_on;
    float r_est_ohm;
    app_err_t err;
    cont_state_t state;
    uint8_t vote_enter;
    uint8_t vote_exit;
    res_sample_t sample;
} cont_result_t;

#define CONT_ENTER_EST_OHM 10.0f
#define CONT_EXIT_EST_OHM 13.0f
#define CONT_VOTE_N 2u
#define CONT_SETTLE_MS 40u

static inline void cont_reset(cont_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = CONT_STATE_OPEN;
}

static inline void cont_init(cont_ctx_t *ctx)
{
    cont_reset(ctx);
}

static inline const char *cont_get_state_name(cont_state_t state)
{
    switch (state) {
    case CONT_STATE_OPEN:
        return "OPEN";
    case CONT_STATE_BEEP_ON:
        return "BEEP";
    case CONT_STATE_BEEP_OFF_WAIT:
        return "WAIT";
    case CONT_STATE_ERR:
    default:
        return "ERR";
    }
}

static inline app_err_t cont_step(cont_ctx_t *ctx, uint32_t now_ms, cont_result_t *out)
{
    app_err_t err;
    float r_est = 0.0f;
    bool calc_ok = false;
    bool below_enter = false;
    bool above_exit = false;
    bool beep_on = false;

    if ((ctx == NULL) || (out == NULL)) {
        return ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->state = ctx->state;
    out->err = ERR_HW_FAIL;

    err = res_acquire_sample(RES_RANGE_SEL_200, &out->sample);
    if (err != ERR_OK) {
        ctx->state = CONT_STATE_ERR;
        ctx->vote_enter = 0u;
        ctx->vote_exit = 0u;
        out->sample_valid = false;
        out->calc_ok = false;
        out->beep_on = false;
        out->r_est_ohm = 0.0f;
        out->err = err;
        out->state = ctx->state;
        out->vote_enter = ctx->vote_enter;
        out->vote_exit = ctx->vote_exit;
        return err;
    }

    out->sample_valid = out->sample.valid;
    err = res_estimate_rx(RES_RANGE_SEL_200, &out->sample, &r_est);
    calc_ok = (err == ERR_OK);
    out->calc_ok = calc_ok;
    out->r_est_ohm = calc_ok ? r_est : 0.0f;
    out->err = calc_ok ? ERR_OK : err;

    if (ctx->state == CONT_STATE_ERR) {
        ctx->state = CONT_STATE_OPEN;
    }

    below_enter = calc_ok && (r_est <= CONT_ENTER_EST_OHM);
    above_exit = (!calc_ok) || (r_est >= CONT_EXIT_EST_OHM);

    switch (ctx->state) {
    case CONT_STATE_OPEN:
        ctx->vote_exit = 0u;
        if (below_enter) {
            if (ctx->vote_enter < 255u) {
                ctx->vote_enter++;
            }
            if (ctx->vote_enter >= CONT_VOTE_N) {
                ctx->state = CONT_STATE_BEEP_ON;
                ctx->vote_enter = 0u;
            }
        } else {
            ctx->vote_enter = 0u;
        }
        break;

    case CONT_STATE_BEEP_ON:
        if (above_exit) {
            if (ctx->vote_exit < 255u) {
                ctx->vote_exit++;
            }
            if (ctx->vote_exit >= CONT_VOTE_N) {
                ctx->state = CONT_STATE_BEEP_OFF_WAIT;
                ctx->settle_deadline_ms = now_ms + CONT_SETTLE_MS;
                ctx->vote_exit = 0u;
            }
        } else {
            ctx->vote_exit = 0u;
        }
        break;

    case CONT_STATE_BEEP_OFF_WAIT:
        ctx->vote_enter = 0u;
        ctx->vote_exit = 0u;
        if ((int32_t)(now_ms - ctx->settle_deadline_ms) >= 0) {
            ctx->state = CONT_STATE_OPEN;
        }
        break;

    case CONT_STATE_ERR:
    default:
        ctx->state = CONT_STATE_OPEN;
        ctx->vote_enter = 0u;
        ctx->vote_exit = 0u;
        break;
    }

    beep_on = (ctx->state == CONT_STATE_BEEP_ON);
    out->beep_on = beep_on;
    out->state = ctx->state;
    out->vote_enter = ctx->vote_enter;
    out->vote_exit = ctx->vote_exit;
    return ERR_OK;
}

#endif
