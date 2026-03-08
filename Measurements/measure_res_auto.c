#include "measure_res_auto.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    float up_th_ohm;
    float down_th_ohm;
} auto_bound_t;

typedef struct {
    res_auto_state_t state;
    uint8_t locked_range_sel;
    uint8_t last_locked_range_sel;
    uint8_t vote_up;
    uint8_t vote_down;
    uint32_t settle_deadline_ms;
    res_auto_result_t hold;
    bool have_hold;
} res_auto_ctx_t;

#define AUTO_VOTE_NEED 3u
#define AUTO_SETTLE_MS 120u

static const auto_bound_t k_auto_bounds[RES_RANGE_SEL_COUNT] = {
    [RES_RANGE_SEL_AUTO] = {.up_th_ohm = FLT_MAX, .down_th_ohm = 0.0f},
    [RES_RANGE_SEL_200] = {.up_th_ohm = 195.0f, .down_th_ohm = 0.0f},
    [RES_RANGE_SEL_2K] = {.up_th_ohm = 1950.0f, .down_th_ohm = 180.0f},
    [RES_RANGE_SEL_20K] = {.up_th_ohm = 19500.0f, .down_th_ohm = 1800.0f},
    [RES_RANGE_SEL_200K] = {.up_th_ohm = FLT_MAX, .down_th_ohm = 18000.0f},
};

static res_auto_ctx_t g_auto;

static uint8_t auto_default_range(void)
{
    return RES_RANGE_SEL_20K;
}

static bool auto_is_manual_range(uint8_t range_sel)
{
    return (range_sel == RES_RANGE_SEL_200) || (range_sel == RES_RANGE_SEL_2K) ||
           (range_sel == RES_RANGE_SEL_20K) || (range_sel == RES_RANGE_SEL_200K);
}

static uint8_t auto_next_up(uint8_t range_sel)
{
    switch (range_sel) {
    case RES_RANGE_SEL_200:
        return RES_RANGE_SEL_2K;
    case RES_RANGE_SEL_2K:
        return RES_RANGE_SEL_20K;
    case RES_RANGE_SEL_20K:
        return RES_RANGE_SEL_200K;
    case RES_RANGE_SEL_200K:
    default:
        return RES_RANGE_SEL_200K;
    }
}

static uint8_t auto_next_down(uint8_t range_sel)
{
    switch (range_sel) {
    case RES_RANGE_SEL_200K:
        return RES_RANGE_SEL_20K;
    case RES_RANGE_SEL_20K:
        return RES_RANGE_SEL_2K;
    case RES_RANGE_SEL_2K:
        return RES_RANGE_SEL_200;
    case RES_RANGE_SEL_200:
    default:
        return RES_RANGE_SEL_200;
    }
}

static int auto_decide_direction(uint8_t range_sel, res_afe_window_t window, bool calc_ok, float r_calc_ohm)
{
    const auto_bound_t *b;

    if (!auto_is_manual_range(range_sel) || (range_sel >= RES_RANGE_SEL_COUNT)) {
        return 0;
    }

    if (window == RES_AFE_WIN_SHORT) {
        return -1;
    }
    if (window == RES_AFE_WIN_OPEN) {
        return 1;
    }
    if (!calc_ok) {
        return 0;
    }

    b = &k_auto_bounds[range_sel];
    if (r_calc_ohm > b->up_th_ohm) {
        return 1;
    }
    if ((b->down_th_ohm > 0.0f) && (r_calc_ohm < b->down_th_ohm)) {
        return -1;
    }
    return 0;
}

static void auto_fill_hold(res_auto_result_t *out)
{
    if (out == NULL) {
        return;
    }

    if (g_auto.have_hold) {
        *out = g_auto.hold;
    } else {
        memset(out, 0, sizeof(*out));
        out->locked_range_sel = g_auto.locked_range_sel;
        (void)measure_res_get_binding(out->locked_range_sel, &out->binding);
        (void)snprintf(out->disp.r_disp_str, sizeof(out->disp.r_disp_str), "----");
        (void)snprintf(out->disp.stat_str, sizeof(out->disp.stat_str), "AUTO");
        (void)snprintf(out->disp.line_value, sizeof(out->disp.line_value), "R: %s", out->disp.r_disp_str);
        (void)snprintf(out->disp.line_stat, sizeof(out->disp.line_stat), "STAT: %s", out->disp.stat_str);
    }

    out->in_settle = true;
    out->locked_range_sel = g_auto.locked_range_sel;
    (void)measure_res_get_binding(out->locked_range_sel, &out->binding);
    out->vote_up = g_auto.vote_up;
    out->vote_down = g_auto.vote_down;
}

static void auto_result_from_measure(uint8_t range_sel, res_auto_result_t *out)
{
    app_err_t err;
    bool display_calc_ok;

    memset(out, 0, sizeof(*out));
    out->locked_range_sel = range_sel;
    out->in_settle = false;
    out->calc_err = ERR_NOT_IMPL;
    out->window = RES_AFE_WIN_INVALID;
    out->afe_ok = false;

    (void)measure_res_get_binding(range_sel, &out->binding);

    err = res_acquire_sample(range_sel, &out->sample);
    if (err != ERR_OK) {
        out->calc_err = err;
        out->calc_ok = false;
        res_format_display(&out->binding, &out->sample, false, false, 0.0f, &out->disp);
        out->valid = false;
        return;
    }

    out->valid = true;
    res_afe_diag_update(range_sel, &out->sample);
    out->health_hist = res_afe_diag_get(range_sel);
    out->window = res_afe_get_window(range_sel, &out->sample);
    out->afe_ok = res_check_afe_health(range_sel, &out->sample);

    out->calc_err = res_estimate_rx(range_sel, &out->sample, &out->r_calc_ohm);
    out->calc_ok = (out->calc_err == ERR_OK);
    if (!out->calc_ok) {
        out->r_calc_ohm = 0.0f;
    }

    display_calc_ok = out->afe_ok && out->calc_ok;
    res_format_display(&out->binding,
                       &out->sample,
                       out->afe_ok,
                       display_calc_ok,
                       out->r_calc_ohm,
                       &out->disp);

    if (out->window == RES_AFE_WIN_SHORT) {
        (void)snprintf(out->disp.r_disp_str, sizeof(out->disp.r_disp_str), "----");
        (void)snprintf(out->disp.stat_str, sizeof(out->disp.stat_str), "SHORT");
        (void)snprintf(out->disp.line_value, sizeof(out->disp.line_value), "R: %s", out->disp.r_disp_str);
        (void)snprintf(out->disp.line_stat, sizeof(out->disp.line_stat), "STAT: %s", out->disp.stat_str);
    } else if (out->window == RES_AFE_WIN_OPEN) {
        (void)snprintf(out->disp.r_disp_str, sizeof(out->disp.r_disp_str), "OL");
        (void)snprintf(out->disp.stat_str, sizeof(out->disp.stat_str), "OPEN");
        (void)snprintf(out->disp.line_value, sizeof(out->disp.line_value), "R: %s", out->disp.r_disp_str);
        (void)snprintf(out->disp.line_stat, sizeof(out->disp.line_stat), "STAT: %s", out->disp.stat_str);
    }
}

static void auto_apply_votes(uint32_t now_ms, res_auto_result_t *out)
{
    int dir;
    uint8_t target_range;

    dir = auto_decide_direction(g_auto.locked_range_sel, out->window, out->calc_ok, out->r_calc_ohm);

    if (dir > 0) {
        if (g_auto.locked_range_sel >= RES_RANGE_SEL_200K) {
            g_auto.vote_up = 0u;
            g_auto.vote_down = 0u;
            return;
        }
        g_auto.vote_up++;
        g_auto.vote_down = 0u;
        if (g_auto.vote_up < AUTO_VOTE_NEED) {
            return;
        }
        target_range = auto_next_up(g_auto.locked_range_sel);
    } else if (dir < 0) {
        if (g_auto.locked_range_sel <= RES_RANGE_SEL_200) {
            g_auto.vote_up = 0u;
            g_auto.vote_down = 0u;
            return;
        }
        g_auto.vote_down++;
        g_auto.vote_up = 0u;
        if (g_auto.vote_down < AUTO_VOTE_NEED) {
            return;
        }
        target_range = auto_next_down(g_auto.locked_range_sel);
    } else {
        g_auto.vote_up = 0u;
        g_auto.vote_down = 0u;
        return;
    }

    g_auto.hold = *out;
    g_auto.hold.in_settle = true;
    (void)snprintf(g_auto.hold.disp.stat_str, sizeof(g_auto.hold.disp.stat_str), "AUTO");
    (void)snprintf(g_auto.hold.disp.line_stat, sizeof(g_auto.hold.disp.line_stat), "STAT: %s", g_auto.hold.disp.stat_str);
    g_auto.have_hold = true;

    g_auto.locked_range_sel = target_range;
    g_auto.last_locked_range_sel = target_range;
    g_auto.vote_up = 0u;
    g_auto.vote_down = 0u;
    g_auto.settle_deadline_ms = now_ms + AUTO_SETTLE_MS;
    g_auto.state = AUTO_SWITCH_WAIT;
}

void measure_res_auto_reset(void)
{
    memset(&g_auto, 0, sizeof(g_auto));
    g_auto.state = AUTO_IDLE;
    g_auto.locked_range_sel = auto_default_range();
    g_auto.last_locked_range_sel = auto_default_range();
}

void measure_res_auto_enter(void)
{
    if (!auto_is_manual_range(g_auto.last_locked_range_sel)) {
        g_auto.last_locked_range_sel = auto_default_range();
    }
    g_auto.locked_range_sel = g_auto.last_locked_range_sel;
    g_auto.vote_up = 0u;
    g_auto.vote_down = 0u;
    g_auto.have_hold = false;
    g_auto.settle_deadline_ms = 0u;
    g_auto.state = AUTO_TRACK;
}

app_err_t measure_res_auto_step(uint32_t now_ms, res_auto_result_t *out)
{
    if (out == NULL) {
        return ERR_INVALID_ARG;
    }

    if (g_auto.state == AUTO_IDLE) {
        measure_res_auto_enter();
    }

    if (g_auto.state == AUTO_SWITCH_WAIT) {
        if ((int32_t)(now_ms - g_auto.settle_deadline_ms) < 0) {
            auto_fill_hold(out);
            return ERR_OK;
        }
        g_auto.state = AUTO_TRACK;
        g_auto.have_hold = false;
    }

    auto_result_from_measure(g_auto.locked_range_sel, out);
    out->locked_range_sel = g_auto.locked_range_sel;
    out->vote_up = g_auto.vote_up;
    out->vote_down = g_auto.vote_down;

    if (out->valid) {
        auto_apply_votes(now_ms, out);
    }

    out->locked_range_sel = g_auto.locked_range_sel;
    out->vote_up = g_auto.vote_up;
    out->vote_down = g_auto.vote_down;
    return ERR_OK;
}

uint8_t measure_res_auto_locked_range(void)
{
    return g_auto.locked_range_sel;
}

void measure_res_auto_get_vote(uint8_t *up, uint8_t *down)
{
    if (up != NULL) {
        *up = g_auto.vote_up;
    }
    if (down != NULL) {
        *down = g_auto.vote_down;
    }
}

res_auto_state_t measure_res_auto_state(void)
{
    return g_auto.state;
}
