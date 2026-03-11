#include "drv_freq_ic.h"

#include <stddef.h>

#include "../BSP/bsp.h"

#define FREQ_HIST_SIZE 8u
#define FREQ_INVALID_LIMIT 5u

static float g_hz_hist[FREQ_HIST_SIZE];
static float g_duty_hist[FREQ_HIST_SIZE];
static uint8_t g_hist_w;
static uint8_t g_hist_count;
static uint8_t g_invalid_count;
static bool g_capture_start_ok;
static freq_debug_snapshot_t g_freq_dbg;

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
    g_hist_w = 0u;
    g_hist_count = 0u;
    g_invalid_count = 0u;
    g_capture_start_ok = bsp_freq_capture_start();
    g_freq_dbg.inst_hz = 0.0f;
    g_freq_dbg.inst_duty = 0.0f;
    g_freq_dbg.hist_count = 0u;
    g_freq_dbg.invalid_count = 0u;
    g_freq_dbg.capture_start_ok = g_capture_start_ok;
    g_freq_dbg.last_err = g_capture_start_ok ? ERR_OK : ERR_HW_FAIL;
}

app_err_t freq_get(float *hz, float *duty_pct)
{
    bsp_capture_t cap;
    float inst_hz;
    float inst_duty;
    uint8_t window;

    if ((hz == NULL) || (duty_pct == NULL)) {
        return ERR_INVALID_ARG;
    }

    if (!bsp_freq_get_capture(&cap) || !cap.valid || (cap.period_ticks == 0u) ||
        (cap.tim_clk_hz == 0u) || (cap.high_ticks > cap.period_ticks)) {
        if (g_invalid_count < 0xFFu) {
            g_invalid_count++;
        }
        g_freq_dbg.invalid_count = g_invalid_count;
        g_freq_dbg.hist_count = g_hist_count;
        g_freq_dbg.capture_start_ok = g_capture_start_ok;
        if ((g_invalid_count < FREQ_INVALID_LIMIT) && (g_hist_count > 0u)) {
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
    if (inst_duty < 0.0f) {
        inst_duty = 0.0f;
    }
    if (inst_duty > 100.0f) {
        inst_duty = 100.0f;
    }

    hist_push(inst_hz, inst_duty);
    window = selected_window(inst_hz);
    hist_average(window, hz, duty_pct);
    g_freq_dbg.inst_hz = inst_hz;
    g_freq_dbg.inst_duty = inst_duty;
    g_freq_dbg.hist_count = g_hist_count;
    g_freq_dbg.invalid_count = g_invalid_count;
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

