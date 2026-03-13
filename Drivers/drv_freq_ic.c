#include "drv_freq_ic.h"

#include <stddef.h>

#include "../BSP/bsp.h"

#define FREQ_HIST_SIZE 8u
#define FREQ_INVALID_LIMIT_MAX 10u
#define FREQ_AUTO_DEFAULT_RANGE 3u /* 2kHz */
#define FREQ_AUTO_VOTE_NEED 2u
#define FREQ_RANGE_OVER 0xFFu

#define FREQ_RANGE_SEL_AUTO 0u
#define FREQ_RANGE_SEL_20HZ 1u
#define FREQ_RANGE_SEL_200HZ 2u
#define FREQ_RANGE_SEL_2KHZ 3u
#define FREQ_RANGE_SEL_20KHZ 4u
#define FREQ_RANGE_SEL_200KHZ 5u
#define FREQ_RANGE_SEL_COUNT 6u

typedef struct {
    float min_hz;
    float max_hz;
} freq_manual_guard_t;

static float g_hz_hist[FREQ_HIST_SIZE];
static float g_duty_hist[FREQ_HIST_SIZE];
static uint8_t g_hist_w;
static uint8_t g_hist_count;
static uint8_t g_invalid_count;
static bool g_capture_start_ok;
static freq_debug_snapshot_t g_freq_dbg;
static uint8_t g_selected_range_sel = FREQ_RANGE_SEL_AUTO;
static uint8_t g_active_range_sel = FREQ_AUTO_DEFAULT_RANGE;
static uint8_t g_auto_vote;
static uint8_t g_auto_candidate = FREQ_RANGE_OVER;
static bool g_overrange;

static const freq_manual_guard_t k_manual_guard[FREQ_RANGE_SEL_COUNT] = {
    [FREQ_RANGE_SEL_AUTO] = {.min_hz = 0.0f, .max_hz = 0.0f},
    [FREQ_RANGE_SEL_20HZ] = {.min_hz = 0.0f, .max_hz = 22.0f},
    [FREQ_RANGE_SEL_200HZ] = {.min_hz = 18.0f, .max_hz = 220.0f},
    [FREQ_RANGE_SEL_2KHZ] = {.min_hz = 180.0f, .max_hz = 2200.0f},
    [FREQ_RANGE_SEL_20KHZ] = {.min_hz = 1800.0f, .max_hz = 22000.0f},
    [FREQ_RANGE_SEL_200KHZ] = {.min_hz = 18000.0f, .max_hz = 220000.0f},
};

static bsp_freq_profile_t profile_from_active(uint8_t active_range_sel)
{
    switch (active_range_sel) {
    case FREQ_RANGE_SEL_20HZ:
        return BSP_FREQ_PROFILE_20HZ;
    case FREQ_RANGE_SEL_200HZ:
        return BSP_FREQ_PROFILE_200HZ;
    case FREQ_RANGE_SEL_2KHZ:
        return BSP_FREQ_PROFILE_2KHZ;
    case FREQ_RANGE_SEL_20KHZ:
        return BSP_FREQ_PROFILE_20KHZ;
    case FREQ_RANGE_SEL_200KHZ:
        return BSP_FREQ_PROFILE_200KHZ;
    default:
        return BSP_FREQ_PROFILE_2KHZ;
    }
}

static bool is_manual_range(uint8_t range_sel)
{
    return (range_sel >= FREQ_RANGE_SEL_20HZ) && (range_sel <= FREQ_RANGE_SEL_200KHZ);
}

static void clear_history(void)
{
    g_hist_w = 0u;
    g_hist_count = 0u;
    g_invalid_count = 0u;
}

static bool in_manual_range(uint8_t range_sel, float hz)
{
    const freq_manual_guard_t *g;

    if (!is_manual_range(range_sel)) {
        return false;
    }

    g = &k_manual_guard[range_sel];
    if (hz <= g->min_hz) {
        return false;
    }
    return hz <= g->max_hz;
}

static uint8_t invalid_limit_for_active(uint8_t active_range_sel)
{
    switch (active_range_sel) {
    case FREQ_RANGE_SEL_20HZ:
        return 8u;
    case FREQ_RANGE_SEL_200HZ:
        return 7u;
    case FREQ_RANGE_SEL_2KHZ:
        return 5u;
    case FREQ_RANGE_SEL_20KHZ:
        return 4u;
    case FREQ_RANGE_SEL_200KHZ:
        return 3u;
    default:
        return 5u;
    }
}

static uint8_t auto_target_from_hz(float hz)
{
    if (hz <= 0.0f) {
        return FREQ_RANGE_OVER;
    }
    if (hz <= 20.0f) {
        return FREQ_RANGE_SEL_20HZ;
    }
    if (hz <= 200.0f) {
        return FREQ_RANGE_SEL_200HZ;
    }
    if (hz <= 2000.0f) {
        return FREQ_RANGE_SEL_2KHZ;
    }
    if (hz <= 20000.0f) {
        return FREQ_RANGE_SEL_20KHZ;
    }
    if (hz <= 200000.0f) {
        return FREQ_RANGE_SEL_200KHZ;
    }
    return FREQ_RANGE_OVER;
}

static bool should_fast_upshift(uint8_t active_range_sel, uint8_t target_range_sel, float hz)
{
    float active_max;

    if (!is_manual_range(active_range_sel) || !is_manual_range(target_range_sel)) {
        return false;
    }
    if (target_range_sel <= active_range_sel) {
        return false;
    }
    if (target_range_sel > (uint8_t)(active_range_sel + 1u)) {
        return true;
    }

    active_max = k_manual_guard[active_range_sel].max_hz;
    if (active_max <= 0.0f) {
        return false;
    }

    /* Large jumps should escape current profile quickly to reduce apparent freeze. */
    return hz > (active_max * 1.60f);
}

static bool auto_track(float hz)
{
    uint8_t target = auto_target_from_hz(hz);

    if (target == FREQ_RANGE_OVER) {
        g_auto_vote = 0u;
        g_auto_candidate = FREQ_RANGE_OVER;
        g_overrange = true;
        return false;
    }

    g_overrange = false;
    if (target == g_active_range_sel) {
        g_auto_vote = 0u;
        g_auto_candidate = FREQ_RANGE_OVER;
        return false;
    }

    if (should_fast_upshift(g_active_range_sel, target, hz)) {
        g_active_range_sel = target;
        g_auto_vote = 0u;
        g_auto_candidate = FREQ_RANGE_OVER;
        clear_history();
        bsp_freq_capture_set_profile(profile_from_active(g_active_range_sel));
        return true;
    }

    if (g_auto_candidate == target) {
        if (g_auto_vote < 0xFFu) {
            g_auto_vote++;
        }
    } else {
        g_auto_candidate = target;
        g_auto_vote = 1u;
    }

    if (g_auto_vote < FREQ_AUTO_VOTE_NEED) {
        return false;
    }

    g_active_range_sel = target;
    g_auto_vote = 0u;
    g_auto_candidate = FREQ_RANGE_OVER;
    clear_history();
    bsp_freq_capture_set_profile(profile_from_active(g_active_range_sel));
    return true;
}

static void hist_push(float hz, float duty)
{
    g_hz_hist[g_hist_w] = hz;
    g_duty_hist[g_hist_w] = duty;
    g_hist_w = (uint8_t)((g_hist_w + 1u) % FREQ_HIST_SIZE);
    if (g_hist_count < FREQ_HIST_SIZE) {
        g_hist_count++;
    }
}

static uint8_t selected_window(float hz)
{
    if (hz < 100.0f) {
        return 8u;
    }
    if (hz < 1000.0f) {
        return 6u;
    }
    return 4u;
}

static void hist_average(uint8_t window, float *hz, float *duty)
{
    uint8_t i;
    uint8_t count = window;
    uint8_t idx;
    float hz_sum = 0.0f;
    float duty_sum = 0.0f;

    if (count > g_hist_count) {
        count = g_hist_count;
    }

    if (count == 0u) {
        *hz = 0.0f;
        *duty = 0.0f;
        return;
    }

    idx = (uint8_t)((g_hist_w + FREQ_HIST_SIZE - 1u) % FREQ_HIST_SIZE);
    for (i = 0u; i < count; i++) {
        hz_sum += g_hz_hist[idx];
        duty_sum += g_duty_hist[idx];
        idx = (uint8_t)((idx + FREQ_HIST_SIZE - 1u) % FREQ_HIST_SIZE);
    }

    *hz = hz_sum / (float)count;
    *duty = duty_sum / (float)count;
}

void freq_start(void)
{
    clear_history();
    bsp_freq_capture_set_profile(profile_from_active(g_active_range_sel));
    g_capture_start_ok = bsp_freq_capture_start();
    g_freq_dbg.inst_hz = 0.0f;
    g_freq_dbg.inst_duty = 0.0f;
    g_freq_dbg.hist_count = 0u;
    g_freq_dbg.invalid_count = 0u;
    g_freq_dbg.selected_range_sel = g_selected_range_sel;
    g_freq_dbg.active_range_sel = g_active_range_sel;
    g_freq_dbg.overrange = g_overrange;
    g_freq_dbg.capture_start_ok = g_capture_start_ok;
    g_freq_dbg.last_err = g_capture_start_ok ? ERR_OK : ERR_HW_FAIL;
}

app_err_t freq_get(float *hz, float *duty_pct)
{
    bsp_capture_t cap;
    float inst_hz;
    float inst_duty;
    uint8_t window;
    uint8_t invalid_limit;

    if ((hz == NULL) || (duty_pct == NULL)) {
        return ERR_INVALID_ARG;
    }

    if (!bsp_freq_get_capture(&cap) || !cap.valid || (cap.period_ticks == 0u) ||
        (cap.tim_clk_hz == 0u) || (cap.high_ticks > cap.period_ticks)) {
        if (g_invalid_count < 0xFFu) {
            g_invalid_count++;
        }
        invalid_limit = invalid_limit_for_active(g_active_range_sel);
        if (invalid_limit > FREQ_INVALID_LIMIT_MAX) {
            invalid_limit = FREQ_INVALID_LIMIT_MAX;
        }
        g_freq_dbg.invalid_count = g_invalid_count;
        g_freq_dbg.hist_count = g_hist_count;
        g_freq_dbg.selected_range_sel = g_selected_range_sel;
        g_freq_dbg.active_range_sel = g_active_range_sel;
        g_freq_dbg.overrange = g_overrange;
        g_freq_dbg.capture_start_ok = g_capture_start_ok;
        if (!g_capture_start_ok && (g_hist_count == 0u)) {
            g_freq_dbg.last_err = ERR_HW_FAIL;
            return ERR_HW_FAIL;
        }
        if ((g_active_range_sel >= FREQ_RANGE_SEL_20KHZ) && (g_invalid_count >= 2u)) {
            /* High-range invalid capture must not keep stale history for long. */
            clear_history();
        }
        if ((g_invalid_count < invalid_limit) && (g_hist_count > 0u)) {
            hist_average(FREQ_HIST_SIZE, hz, duty_pct);
            g_freq_dbg.last_err = ERR_OK;
            return ERR_OK;
        }
        g_freq_dbg.last_err = ERR_NO_SIGNAL;
        return ERR_NO_SIGNAL;
    }

    g_invalid_count = 0u;

    inst_hz = (float)cap.tim_clk_hz / (float)cap.period_ticks;
    inst_duty = 100.0f * ((float)cap.high_ticks / (float)cap.period_ticks);
    g_freq_dbg.inst_hz = inst_hz;
    g_freq_dbg.inst_duty = inst_duty;
    if (inst_duty < 0.0f) {
        inst_duty = 0.0f;
    }
    if (inst_duty > 100.0f) {
        inst_duty = 100.0f;
    }

    if (g_selected_range_sel == FREQ_RANGE_SEL_AUTO) {
        if (auto_track(inst_hz)) {
            g_freq_dbg.hist_count = g_hist_count;
            g_freq_dbg.invalid_count = g_invalid_count;
            g_freq_dbg.selected_range_sel = g_selected_range_sel;
            g_freq_dbg.active_range_sel = g_active_range_sel;
            g_freq_dbg.overrange = g_overrange;
            g_freq_dbg.capture_start_ok = g_capture_start_ok;
            g_freq_dbg.last_err = ERR_NO_SIGNAL;
            return ERR_NO_SIGNAL;
        }
        if (g_overrange) {
            g_freq_dbg.hist_count = g_hist_count;
            g_freq_dbg.invalid_count = g_invalid_count;
            g_freq_dbg.selected_range_sel = g_selected_range_sel;
            g_freq_dbg.active_range_sel = g_active_range_sel;
            g_freq_dbg.overrange = g_overrange;
            g_freq_dbg.capture_start_ok = g_capture_start_ok;
            g_freq_dbg.last_err = ERR_OVERRANGE;
            return ERR_OVERRANGE;
        }
    } else if (!in_manual_range(g_selected_range_sel, inst_hz)) {
        g_overrange = true;
        clear_history();
        g_freq_dbg.hist_count = g_hist_count;
        g_freq_dbg.invalid_count = g_invalid_count;
        g_freq_dbg.selected_range_sel = g_selected_range_sel;
        g_freq_dbg.active_range_sel = g_active_range_sel;
        g_freq_dbg.overrange = g_overrange;
        g_freq_dbg.capture_start_ok = g_capture_start_ok;
        g_freq_dbg.last_err = ERR_OVERRANGE;
        return ERR_OVERRANGE;
    } else {
        g_overrange = false;
    }

    hist_push(inst_hz, inst_duty);
    window = selected_window(inst_hz);
    hist_average(window, hz, duty_pct);
    g_freq_dbg.hist_count = g_hist_count;
    g_freq_dbg.invalid_count = g_invalid_count;
    g_freq_dbg.selected_range_sel = g_selected_range_sel;
    g_freq_dbg.active_range_sel = g_active_range_sel;
    g_freq_dbg.overrange = g_overrange;
    g_freq_dbg.capture_start_ok = g_capture_start_ok;
    g_freq_dbg.last_err = ERR_OK;

    return ERR_OK;
}

app_err_t freq_get_hz(float *hz)
{
    float dummy_duty;
    return freq_get(hz, &dummy_duty);
}

app_err_t freq_get_duty(float *duty_pct)
{
    float dummy_hz;
    return freq_get(&dummy_hz, duty_pct);
}

void freq_get_debug_snapshot(freq_debug_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_freq_dbg;
}

void freq_set_range_sel(uint8_t sel)
{
    if (sel >= FREQ_RANGE_SEL_COUNT) {
        sel = FREQ_RANGE_SEL_AUTO;
    }

    g_selected_range_sel = sel;
    g_overrange = false;
    g_auto_vote = 0u;
    g_auto_candidate = FREQ_RANGE_OVER;

    if (g_selected_range_sel == FREQ_RANGE_SEL_AUTO) {
        if (!is_manual_range(g_active_range_sel)) {
            g_active_range_sel = FREQ_AUTO_DEFAULT_RANGE;
        }
    } else {
        g_active_range_sel = g_selected_range_sel;
    }

    clear_history();
    bsp_freq_capture_set_profile(profile_from_active(g_active_range_sel));
    g_freq_dbg.selected_range_sel = g_selected_range_sel;
    g_freq_dbg.active_range_sel = g_active_range_sel;
    g_freq_dbg.overrange = g_overrange;
}

uint8_t freq_get_active_range_sel(void)
{
    return g_active_range_sel;
}

bool freq_is_overrange(void)
{
    return g_overrange;
}

