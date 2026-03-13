#include "app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"
#include "../Drivers/drv_freq_ic.h"
#include "../Drivers/drv_opamp_internal.h"
#include "../Measurements/measure_res.h"
#include "../Measurements/measure_res_auto.h"
#include "../Measurements/measure_cont.h"
#include "../Measurements/measure_diode.h"
#include "../Measurements/measure_vdc.h"
#include "../Measurements/measure_cap.h"
#include "../Measurements/res_afe_diag.h"
#include "../Measurements/res_display_fmt.h"
#include "app_bootdiag.h"
#include "app_display_service.h"
#include "app_types.h"
#include "app_ui_presenter.h"
#include "stm32g4xx_hal.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define MEAS_PERIOD_MS 40u
#define CONT_MEAS_PERIOD_MS 25u
#define BEEP_FREQ_HZ 2700u
#define DIODE_VF_DIRTY_DELTA_MV 8u
#define DIODE_RAW_DIRTY_DELTA 16u
#define APP_DEBUG_LEFT_KEY_ENABLE 1u
#define VDC_UI_SEL_AUTO 0u
#define VDC_UI_SEL_2000MV 1u
#define VDC_UI_SEL_20V 2u
#define VDC_UI_SEL_COUNT 3u
#define VDC_AUTO_UP_MV 1800u
#define VDC_AUTO_DOWN_MV 1500u
#define VDC_AUTO_VOTE_NEED 2u
#define FREQ_UI_STALL_MS 1200u
#define FREQ_MEAS_ALIVE_MS 300u
#define FREQ_RECOVERY_CONFIRM_MS 800u
#define FREQ_MAIN_DEADBAND_PCT 0.002f
#define FREQ_MAIN_DUTY_DEADBAND 1.0f
#define RES_MAIN_DEADBAND_PCT 0.002f
#define RES_MAIN_MIN_LSD_OHM 1.0f

#define KEY_SHORT_MIN_MS 15u

typedef enum {
    VIEW_RUN_MAIN = 0,
    VIEW_RUN_DEBUG
} app_view_t;

typedef enum {
    FREQ_RECOVER_IDLE = 0,
    FREQ_RECOVER_WAIT_CONFIRM
} freq_recover_state_t;

typedef struct app_ctx_s app_ctx_t;
typedef const char *(*mode_range_name_fn_t)(const app_ctx_t *ctx);
typedef void (*mode_range_next_fn_t)(app_ctx_t *ctx);
typedef void (*mode_measure_fn_t)(app_ctx_t *ctx, uint32_t now_ms);

typedef struct {
    const char *title;
    mode_range_name_fn_t range_name_fn;
    mode_range_next_fn_t range_next_fn;
    mode_measure_fn_t measure_fn;
} mode_desc_t;

struct app_ctx_s {
    app_mode_t mode;
    app_view_t view;

    uint8_t res_range_sel;
    uint8_t vdc_range_sel;
    uint8_t vdc_ui_sel;
    uint8_t vdc_auto_vote_up;
    uint8_t vdc_auto_vote_down;
    uint8_t freq_range_sel;
    uint8_t cap_range_sel;
    float freq_hz;
    float freq_duty;
    bool freq_have_valid;
    app_err_t freq_err;
    float freq_shown_hz;
    float freq_shown_duty;
    app_err_t freq_shown_err;
    uint8_t freq_shown_active_range;
    bool freq_shown_inited;
    uint32_t freq_enter_ms;
    uint32_t freq_last_measure_ms;
    uint32_t freq_last_flush_ok_ms;
    uint32_t freq_recover_deadline_ms;
    freq_recover_state_t freq_recover_state;
    bool freq_reset_used_this_entry;

    bool right_long_fired;
    bool left_long_fired;

    app_err_t presenter_err;
    app_err_t adc_init_err;
    app_err_t opamp_init_err;
    app_err_t adc_last_err;

    bool dbg_raw_valid;
    bool dbg_mv_valid;
    bool dbg_vdda_valid;
    uint16_t dbg_raw_u16;
    uint32_t dbg_mv;
    uint32_t dbg_vdda_mv;

    res_sample_t res_sample;
    res_range_binding_t res_binding;
    res_afe_health_t res_health;
    res_afe_window_t res_window;
    bool res_afe_ok;
    bool res_calc_ok;
    app_err_t res_calc_err;
    float res_r_calc_ohm;
    res_display_text_t res_disp;
    res_auto_result_t res_auto;
    bool res_auto_active;

    cont_ctx_t cont_ctx;
    cont_result_t cont;
    vdc_ctx_t vdc_ctx;
    vdc_result_t vdc;
    uint32_t vdc_shown_vin_mv;
    vdc_status_t vdc_shown_status;
    uint8_t vdc_shown_range;
    app_err_t vdc_shown_err;
    bool vdc_shown_inited;
    diode_ctx_t diode_ctx;
    diode_latched_result_t diode;
    cap_result_t cap;
    float res_shown_r_ohm;
    uint8_t res_shown_range_sel;
    uint8_t res_shown_locked_sel;
    app_err_t res_shown_calc_err;
    char res_shown_line_value[22];
    char res_shown_line_stat[22];
    bool res_shown_inited;

    uint32_t next_meas_ms;
    uint32_t next_ui_ms;
    uint32_t next_debug_adc_ms;
    bool ui_dirty;
};

static app_ctx_t g_app;

static const char *k_vdc_name[VDC_RANGE_COUNT] = {"2000mV", "20V"};
static const char *k_vdc_active_short[VDC_RANGE_COUNT] = {"2V", "20V"};
static const char *k_freq_name[FREQ_RANGE_COUNT] = {"AUTO", "20Hz", "200Hz", "2kHz", "20kHz", "200kHz"};
static const char *k_cap_name[CAP_RANGE_COUNT] = {"20nF", "2uF", "200uF"};

static const char *range_name_res(const app_ctx_t *ctx)
{
    return measure_res_range_name(ctx->res_range_sel);
}

static const char *range_name_vdc(const app_ctx_t *ctx)
{
    return k_vdc_name[ctx->vdc_range_sel % VDC_RANGE_COUNT];
}

static const char *vdc_active_short_name(const app_ctx_t *ctx)
{
    return k_vdc_active_short[ctx->vdc_range_sel % VDC_RANGE_COUNT];
}

static const char *range_name_freq(const app_ctx_t *ctx)
{
    return k_freq_name[ctx->freq_range_sel % FREQ_RANGE_COUNT];
}

static const char *range_name_cap(const app_ctx_t *ctx)
{
    return k_cap_name[ctx->cap_range_sel % CAP_RANGE_COUNT];
}

static void format_freq_main_value(float hz, char *out, size_t out_sz, const char **unit_out)
{
    uint32_t hz_i;
    uint32_t whole;
    uint32_t frac;
    uint32_t khz_x100;
    uint32_t khz_x10;

    if ((out == NULL) || (out_sz == 0u) || (unit_out == NULL)) {
        return;
    }

    if (hz <= 0.0f) {
        hz_i = 0u;
    } else {
        hz_i = (uint32_t)(hz + 0.5f);
    }

    if (hz_i < 1000u) {
        (void)snprintf(out, out_sz, "%lu", (unsigned long)hz_i);
        *unit_out = "Hz";
        return;
    }

    if (hz_i < 10000u) {
        whole = hz_i / 1000u;
        frac = hz_i % 1000u;
        (void)snprintf(out, out_sz, "%lu.%03lu",
                       (unsigned long)whole,
                       (unsigned long)frac);
    } else if (hz_i < 100000u) {
        khz_x100 = (hz_i + 5u) / 10u; /* round to 0.01kHz (10Hz) */
        whole = khz_x100 / 100u;
        frac = khz_x100 % 100u;
        (void)snprintf(out, out_sz, "%lu.%02lu",
                       (unsigned long)whole,
                       (unsigned long)frac);
    } else {
        khz_x10 = (hz_i + 50u) / 100u; /* round to 0.1kHz (100Hz) */
        whole = khz_x10 / 10u;
        frac = khz_x10 % 10u;
        (void)snprintf(out, out_sz, "%lu.%01lu",
                       (unsigned long)whole,
                       (unsigned long)frac);
    }
    *unit_out = "kHz";
}

static const char *range_name_cont(const app_ctx_t *ctx)
{
    (void)ctx;
    return "BEEP";
}

static const char *range_name_diode(const app_ctx_t *ctx)
{
    (void)ctx;
    return "VF";
}

static void range_next_res(app_ctx_t *ctx)
{
    ctx->res_range_sel = (uint8_t)((ctx->res_range_sel + 1u) % RES_RANGE_SEL_COUNT);
}

static void range_next_vdc(app_ctx_t *ctx)
{
    if (ctx->vdc_ui_sel == VDC_UI_SEL_AUTO) {
        ctx->vdc_ui_sel = VDC_UI_SEL_2000MV;
        ctx->vdc_range_sel = VDC_RANGE_2000MV;
    } else if (ctx->vdc_ui_sel == VDC_UI_SEL_2000MV) {
        ctx->vdc_ui_sel = VDC_UI_SEL_20V;
        ctx->vdc_range_sel = VDC_RANGE_20V;
    } else {
        ctx->vdc_ui_sel = VDC_UI_SEL_AUTO;
        ctx->vdc_range_sel = VDC_RANGE_2000MV;
    }
    ctx->vdc_auto_vote_up = 0u;
    ctx->vdc_auto_vote_down = 0u;
    vdc_set_range(&ctx->vdc_ctx, (vdc_range_t)ctx->vdc_range_sel);
}

static void range_next_freq(app_ctx_t *ctx)
{
    ctx->freq_range_sel = (uint8_t)((ctx->freq_range_sel + 1u) % FREQ_RANGE_COUNT);
    freq_set_range_sel(ctx->freq_range_sel);
    freq_start();
    ctx->freq_err = ERR_NO_SIGNAL;
}

static void range_next_noop(app_ctx_t *ctx)
{
    (void)ctx;
}

static void range_next_cap(app_ctx_t *ctx)
{
    ctx->cap_range_sel = (uint8_t)((ctx->cap_range_sel + 1u) % CAP_RANGE_COUNT);
    cap_set_range((cap_range_t)ctx->cap_range_sel);
}

static float app_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static void res_mark_dirty(app_ctx_t *ctx)
{
    bool changed = (ctx->view == VIEW_RUN_DEBUG);
    uint8_t locked_sel;

    if (ctx == NULL) {
        return;
    }

    locked_sel = (ctx->res_range_sel == RES_RANGE_SEL_AUTO) ? ctx->res_auto.locked_range_sel : ctx->res_range_sel;

    if (!ctx->res_shown_inited) {
        changed = true;
    }

    if ((ctx->res_shown_range_sel != ctx->res_range_sel) ||
        (ctx->res_shown_locked_sel != locked_sel) ||
        (ctx->res_shown_calc_err != ctx->res_calc_err)) {
        changed = true;
    }

    if (strcmp(ctx->res_shown_line_stat, ctx->res_disp.line_stat) != 0) {
        changed = true;
    }

    if (ctx->res_calc_ok && (ctx->res_calc_err == ERR_OK) &&
        ctx->res_shown_inited && (ctx->res_shown_calc_err == ERR_OK)) {
        float threshold = app_absf(ctx->res_shown_r_ohm) * RES_MAIN_DEADBAND_PCT;
        if (threshold < RES_MAIN_MIN_LSD_OHM) {
            threshold = RES_MAIN_MIN_LSD_OHM;
        }
        if (app_absf(ctx->res_r_calc_ohm - ctx->res_shown_r_ohm) >= threshold) {
            changed = true;
        }
    } else if (strcmp(ctx->res_shown_line_value, ctx->res_disp.line_value) != 0) {
        changed = true;
    }

    if (!changed) {
        return;
    }

    (void)snprintf(ctx->res_shown_line_value, sizeof(ctx->res_shown_line_value), "%s", ctx->res_disp.line_value);
    (void)snprintf(ctx->res_shown_line_stat, sizeof(ctx->res_shown_line_stat), "%s", ctx->res_disp.line_stat);
    ctx->res_shown_r_ohm = ctx->res_r_calc_ohm;
    ctx->res_shown_range_sel = ctx->res_range_sel;
    ctx->res_shown_locked_sel = locked_sel;
    ctx->res_shown_calc_err = ctx->res_calc_err;
    ctx->res_shown_inited = true;
    ctx->ui_dirty = true;
}

static void vdc_mark_dirty(app_ctx_t *ctx)
{
    bool changed = (ctx->view == VIEW_RUN_DEBUG);
    uint32_t delta;
    uint32_t lsd;

    if (ctx == NULL) {
        return;
    }

    if (!ctx->vdc_shown_inited) {
        changed = true;
    }

    if ((ctx->vdc_shown_status != ctx->vdc.status) ||
        (ctx->vdc_shown_range != (uint8_t)ctx->vdc.range) ||
        (ctx->vdc_shown_err != ctx->vdc.err)) {
        changed = true;
    }

    if (ctx->vdc.valid && (ctx->vdc.status == VDC_STAT_OK) && ctx->vdc_shown_inited &&
        (ctx->vdc_shown_status == VDC_STAT_OK)) {
        lsd = (ctx->vdc.range == VDC_RANGE_2000MV) ? 1u : 10u;
        delta = (ctx->vdc.vin_mv >= ctx->vdc_shown_vin_mv)
            ? (ctx->vdc.vin_mv - ctx->vdc_shown_vin_mv)
            : (ctx->vdc_shown_vin_mv - ctx->vdc.vin_mv);
        if (delta >= lsd) {
            changed = true;
        }
    }

    if (!changed) {
        return;
    }

    ctx->vdc_shown_vin_mv = ctx->vdc.vin_mv;
    ctx->vdc_shown_status = ctx->vdc.status;
    ctx->vdc_shown_range = (uint8_t)ctx->vdc.range;
    ctx->vdc_shown_err = ctx->vdc.err;
    ctx->vdc_shown_inited = true;
    ctx->ui_dirty = true;
}

static void ui_update_debug_adc_sample(void)
{
    app_err_t err;
    uint16_t raw = 0u;
    uint32_t mv = 0u;
    uint32_t vdda_mv = 3300u;

    g_app.dbg_raw_valid = false;
    g_app.dbg_mv_valid = false;
    g_app.dbg_vdda_valid = false;
    g_app.dbg_raw_u16 = 0u;
    g_app.dbg_mv = 0u;
    g_app.dbg_vdda_mv = 3300u;

    if (g_app.adc_init_err != ERR_OK) {
        g_app.adc_last_err = g_app.adc_init_err;
        return;
    }

    err = adc1_read_raw_u16(&raw);
    if (err != ERR_OK) {
        g_app.adc_last_err = err;
        return;
    }
    g_app.dbg_raw_valid = true;
    g_app.dbg_raw_u16 = raw;

    err = adc1_read_mv(&mv);
    if (err != ERR_OK) {
        g_app.adc_last_err = err;
        return;
    }
    g_app.dbg_mv_valid = true;
    g_app.dbg_mv = mv;

    err = adc1_read_vdda_mv(&vdda_mv);
    if (err == ERR_OK) {
        g_app.dbg_vdda_valid = true;
        g_app.dbg_vdda_mv = vdda_mv;
        g_app.adc_last_err = ERR_OK;
    } else {
        g_app.dbg_vdda_valid = false;
        g_app.dbg_vdda_mv = 3300u;
        g_app.adc_last_err = err;
    }
}

static void measure_tick_res(app_ctx_t *ctx, uint32_t now_ms)
{
    app_err_t err;
    bool have_binding;
    bool display_calc_ok;

    if (ctx->res_range_sel == RES_RANGE_SEL_AUTO) {
        if (!ctx->res_auto_active) {
            measure_res_auto_enter();
            ctx->res_auto_active = true;
        }

        err = measure_res_auto_step(now_ms, &ctx->res_auto);
        if (err != ERR_OK) {
            ctx->res_window = RES_AFE_WIN_INVALID;
            ctx->res_afe_ok = false;
            ctx->res_calc_ok = false;
            ctx->res_calc_err = err;
            ctx->res_r_calc_ohm = 0.0f;
            res_format_display(&ctx->res_binding, &ctx->res_sample, false, false, 0.0f, &ctx->res_disp);
            res_mark_dirty(ctx);
            return;
        }

        ctx->res_binding = ctx->res_auto.binding;
        ctx->res_sample = ctx->res_auto.sample;
        ctx->res_health = ctx->res_auto.health_hist;
        ctx->res_window = ctx->res_auto.window;
        ctx->res_afe_ok = ctx->res_auto.afe_ok;
        ctx->res_calc_ok = ctx->res_auto.calc_ok;
        ctx->res_calc_err = ctx->res_auto.calc_err;
        ctx->res_r_calc_ohm = ctx->res_auto.r_calc_ohm;
        ctx->res_disp = ctx->res_auto.disp;
        res_mark_dirty(ctx);
        return;
    }

    ctx->res_auto_active = false;

    have_binding = measure_res_get_binding(ctx->res_range_sel, &ctx->res_binding);
    if (!have_binding) {
        ctx->res_window = RES_AFE_WIN_INVALID;
        ctx->res_afe_ok = false;
        ctx->res_calc_ok = false;
        ctx->res_calc_err = ERR_INVALID_ARG;
        ctx->res_r_calc_ohm = 0.0f;
        res_format_display(NULL, &ctx->res_sample, false, false, 0.0f, &ctx->res_disp);
        res_mark_dirty(ctx);
        return;
    }

    err = res_acquire_sample(ctx->res_range_sel, &ctx->res_sample);
    if (err != ERR_OK) {
        ctx->res_window = RES_AFE_WIN_INVALID;
        ctx->res_afe_ok = false;
        ctx->res_calc_ok = false;
        ctx->res_calc_err = err;
        ctx->res_r_calc_ohm = 0.0f;
        res_format_display(&ctx->res_binding, &ctx->res_sample, false, false, 0.0f, &ctx->res_disp);
        res_mark_dirty(ctx);
        return;
    }

    res_afe_diag_update(ctx->res_range_sel, &ctx->res_sample);
    ctx->res_health = res_afe_diag_get(ctx->res_range_sel);
    ctx->res_window = res_afe_get_window(ctx->res_range_sel, &ctx->res_sample);
    ctx->res_afe_ok = res_check_afe_health(ctx->res_range_sel, &ctx->res_sample);

    ctx->res_calc_err = res_estimate_rx(ctx->res_range_sel, &ctx->res_sample, &ctx->res_r_calc_ohm);
    ctx->res_calc_ok = (ctx->res_calc_err == ERR_OK);
    if (!ctx->res_calc_ok) {
        ctx->res_r_calc_ohm = 0.0f;
    }

    display_calc_ok = ctx->res_afe_ok && ctx->res_calc_ok;
    res_format_display(&ctx->res_binding,
                       &ctx->res_sample,
                       ctx->res_afe_ok,
                       display_calc_ok,
                       ctx->res_r_calc_ohm,
                       &ctx->res_disp);
    if (ctx->res_sample.valid) {
        if (ctx->res_window == RES_AFE_WIN_SHORT) {
            (void)snprintf(ctx->res_disp.r_disp_str, sizeof(ctx->res_disp.r_disp_str), "----");
            (void)snprintf(ctx->res_disp.stat_str, sizeof(ctx->res_disp.stat_str), "SHORT");
            (void)snprintf(ctx->res_disp.line_value, sizeof(ctx->res_disp.line_value), "R: %s", ctx->res_disp.r_disp_str);
            (void)snprintf(ctx->res_disp.line_stat, sizeof(ctx->res_disp.line_stat), "STAT: %s", ctx->res_disp.stat_str);
        } else if (ctx->res_window == RES_AFE_WIN_OPEN) {
            (void)snprintf(ctx->res_disp.r_disp_str, sizeof(ctx->res_disp.r_disp_str), "----");
            (void)snprintf(ctx->res_disp.stat_str, sizeof(ctx->res_disp.stat_str), "OPEN");
            (void)snprintf(ctx->res_disp.line_value, sizeof(ctx->res_disp.line_value), "R: %s", ctx->res_disp.r_disp_str);
            (void)snprintf(ctx->res_disp.line_stat, sizeof(ctx->res_disp.line_stat), "STAT: %s", ctx->res_disp.stat_str);
        }
    }
    res_mark_dirty(ctx);
}

static void vdc_auto_track(app_ctx_t *ctx)
{
    if ((ctx->vdc_ui_sel != VDC_UI_SEL_AUTO) || !ctx->vdc.valid) {
        ctx->vdc_auto_vote_up = 0u;
        ctx->vdc_auto_vote_down = 0u;
        return;
    }

    if (ctx->vdc_range_sel == VDC_RANGE_2000MV) {
        if ((ctx->vdc.status == VDC_STAT_OL) || (ctx->vdc.vin_mv > VDC_AUTO_UP_MV)) {
            if (ctx->vdc_auto_vote_up < 0xFFu) {
                ctx->vdc_auto_vote_up++;
            }
        } else {
            ctx->vdc_auto_vote_up = 0u;
        }
        ctx->vdc_auto_vote_down = 0u;

        if (ctx->vdc_auto_vote_up >= VDC_AUTO_VOTE_NEED) {
            ctx->vdc_range_sel = VDC_RANGE_20V;
            ctx->vdc_auto_vote_up = 0u;
            ctx->vdc_auto_vote_down = 0u;
            vdc_set_range(&ctx->vdc_ctx, (vdc_range_t)ctx->vdc_range_sel);
            ctx->ui_dirty = true;
        }
    } else {
        if ((ctx->vdc.status == VDC_STAT_OK) && (ctx->vdc.vin_mv < VDC_AUTO_DOWN_MV)) {
            if (ctx->vdc_auto_vote_down < 0xFFu) {
                ctx->vdc_auto_vote_down++;
            }
        } else {
            ctx->vdc_auto_vote_down = 0u;
        }
        ctx->vdc_auto_vote_up = 0u;

        if (ctx->vdc_auto_vote_down >= VDC_AUTO_VOTE_NEED) {
            ctx->vdc_range_sel = VDC_RANGE_2000MV;
            ctx->vdc_auto_vote_up = 0u;
            ctx->vdc_auto_vote_down = 0u;
            vdc_set_range(&ctx->vdc_ctx, (vdc_range_t)ctx->vdc_range_sel);
            ctx->ui_dirty = true;
        }
    }
}

static void freq_recovery_tick(uint32_t now_ms)
{
    if (g_app.mode != MODE_FREQ) {
        g_app.freq_recover_state = FREQ_RECOVER_IDLE;
        return;
    }

    if ((bootdiag_get_stage() != BOOT_RUN) || (bootdiag_get_fault() != BOOT_FAULT_NONE)) {
        return;
    }

    if ((now_ms - g_app.freq_last_measure_ms) > FREQ_MEAS_ALIVE_MS) {
        return;
    }

    if (g_app.freq_recover_state == FREQ_RECOVER_WAIT_CONFIRM) {
        if ((now_ms - g_app.freq_last_flush_ok_ms) <= UI_REFRESH_MS) {
            g_app.freq_recover_state = FREQ_RECOVER_IDLE;
            return;
        }

        if ((int32_t)(now_ms - g_app.freq_recover_deadline_ms) < 0) {
            return;
        }

        if (!g_app.freq_reset_used_this_entry) {
            g_app.freq_reset_used_this_entry = true;
            NVIC_SystemReset();
        }
        return;
    }

    if ((now_ms - g_app.freq_last_flush_ok_ms) <= FREQ_UI_STALL_MS) {
        return;
    }

    g_app.presenter_err = app_ui_presenter_init();
    g_app.ui_dirty = true;
    g_app.next_ui_ms = now_ms;
    g_app.freq_recover_state = FREQ_RECOVER_WAIT_CONFIRM;
    g_app.freq_recover_deadline_ms = now_ms + FREQ_RECOVERY_CONFIRM_MS;
}

static void measure_tick_freq(app_ctx_t *ctx, uint32_t now_ms)
{
    float hz = 0.0f;
    float duty = 0.0f;
    app_err_t err;
    uint8_t active_sel;
    bool changed = (ctx->view == VIEW_RUN_DEBUG);

    err = freq_get(&hz, &duty);
    ctx->freq_err = err;
    ctx->freq_last_measure_ms = now_ms;
    active_sel = freq_get_active_range_sel();
    if (err == ERR_OK) {
        ctx->freq_hz = hz;
        ctx->freq_duty = duty;
        ctx->freq_have_valid = true;
    }

    if (!ctx->freq_shown_inited) {
        changed = true;
    }

    if ((ctx->freq_shown_err != ctx->freq_err) ||
        (ctx->freq_shown_active_range != active_sel)) {
        changed = true;
    }

    if ((ctx->freq_err == ERR_OK) && ctx->freq_shown_inited && (ctx->freq_shown_err == ERR_OK)) {
        float hz_threshold = app_absf(ctx->freq_shown_hz) * FREQ_MAIN_DEADBAND_PCT;
        if (hz_threshold < 1.0f) {
            hz_threshold = 1.0f;
        }
        if (app_absf(ctx->freq_hz - ctx->freq_shown_hz) >= hz_threshold) {
            changed = true;
        }
        if (app_absf(ctx->freq_duty - ctx->freq_shown_duty) >= FREQ_MAIN_DUTY_DEADBAND) {
            changed = true;
        }
    } else if (ctx->freq_err == ERR_OK) {
        changed = true;
    }

    if (!changed) {
        /* Main-view deadband: no flush needed this cycle, keep recovery watchdog calm. */
        ctx->freq_last_flush_ok_ms = now_ms;
        return;
    }

    ctx->freq_shown_hz = ctx->freq_hz;
    ctx->freq_shown_duty = ctx->freq_duty;
    ctx->freq_shown_err = ctx->freq_err;
    ctx->freq_shown_active_range = active_sel;
    ctx->freq_shown_inited = true;
    ctx->ui_dirty = true;
}

static void measure_tick_vdc(app_ctx_t *ctx, uint32_t now_ms)
{
    app_err_t err;

    err = vdc_tick(&ctx->vdc_ctx, now_ms);
    if (err != ERR_OK) {
        ctx->vdc.valid = false;
        ctx->vdc.status = VDC_STAT_ERR;
        ctx->vdc.err = err;
        vdc_mark_dirty(ctx);
        return;
    }

    err = vdc_get_result(&ctx->vdc_ctx, &ctx->vdc);
    if (err != ERR_OK) {
        ctx->vdc.valid = false;
        ctx->vdc.status = VDC_STAT_ERR;
        ctx->vdc.err = err;
    }

    vdc_auto_track(ctx);
    vdc_mark_dirty(ctx);
}

static void measure_tick_diode(app_ctx_t *ctx, uint32_t now_ms)
{
    diode_result_t step_out;
    diode_latched_result_t prev;
    uint32_t vf_delta;
    uint16_t raw_delta;
    app_err_t err;

    prev = ctx->diode;

    err = diode_step(&ctx->diode_ctx, now_ms, &step_out);
    if (err != ERR_OK) {
        ctx->diode.valid = false;
        ctx->diode.err = err;
        ctx->diode.stat = DIODE_STAT_ERR;
        ctx->ui_dirty = true;
        return;
    }

    err = diode_get_latched_result(&ctx->diode_ctx, &ctx->diode);
    if (err != ERR_OK) {
        return;
    }

    if ((prev.stat != ctx->diode.stat) ||
        (prev.valid != ctx->diode.valid) ||
        (prev.err != ctx->diode.err)) {
        ctx->ui_dirty = true;
        return;
    }

    if (ctx->diode.stat == DIODE_STAT_OK) {
        vf_delta = (ctx->diode.vf_mv >= prev.vf_mv) ? (ctx->diode.vf_mv - prev.vf_mv) : (prev.vf_mv - ctx->diode.vf_mv);
        if (vf_delta >= DIODE_VF_DIRTY_DELTA_MV) {
            ctx->ui_dirty = true;
            return;
        }
    }

    if (ctx->diode.valid) {
        raw_delta = (ctx->diode.raw_u16 >= prev.raw_u16)
            ? (uint16_t)(ctx->diode.raw_u16 - prev.raw_u16)
            : (uint16_t)(prev.raw_u16 - ctx->diode.raw_u16);
        if (raw_delta >= DIODE_RAW_DIRTY_DELTA) {
            ctx->ui_dirty = true;
        }
    }
}

static void measure_tick_cont(app_ctx_t *ctx, uint32_t now_ms)
{
    app_err_t err;

    err = cont_step(&ctx->cont_ctx, now_ms, &ctx->cont);
    if (err != ERR_OK) {
        ctx->cont.beep_on = false;
    }

    if (ctx->view == VIEW_RUN_DEBUG) {
        /* Hard rule: debug view must silence continuous buzzer in CONT mode. */
        beep_continuous(false);
    } else {
        beep_continuous(ctx->cont.beep_on);
    }
    ctx->ui_dirty = true;
}

static void measure_tick_cap(app_ctx_t *ctx, uint32_t now_ms)
{
    cap_result_t prev;

    (void)now_ms;
    prev = ctx->cap;
    cap_measure_once(&ctx->cap);

    if ((ctx->view == VIEW_RUN_DEBUG) ||
        (ctx->cap.stat != prev.stat) ||
        (ctx->cap.over != prev.over) ||
        (ctx->cap.valid != prev.valid) ||
        (ctx->cap.range != prev.range) ||
        (ctx->cap.err != prev.err) ||
        (ctx->cap.adc_raw_last != prev.adc_raw_last)) {
        ctx->ui_dirty = true;
        return;
    }

    if (ctx->cap.valid) {
        float threshold_pf = app_absf(prev.value_pf) * 0.01f;
        if (threshold_pf < 1.0f) {
            threshold_pf = 1.0f;
        }
        if (app_absf(ctx->cap.value_pf - prev.value_pf) >= threshold_pf) {
            ctx->ui_dirty = true;
        }
    }
}

static const mode_desc_t k_mode_desc[MODE_COUNT] = {
    [MODE_VDC] = {
        .title = "VDC",
        .range_name_fn = range_name_vdc,
        .range_next_fn = range_next_vdc,
        .measure_fn = measure_tick_vdc
    },
    [MODE_RES] = {
        .title = "RES",
        .range_name_fn = range_name_res,
        .range_next_fn = range_next_res,
        .measure_fn = measure_tick_res
    },
    [MODE_FREQ] = {
        .title = "FREQ",
        .range_name_fn = range_name_freq,
        .range_next_fn = range_next_freq,
        .measure_fn = measure_tick_freq
    },
    [MODE_CONT] = {
        .title = "CONT",
        .range_name_fn = range_name_cont,
        .range_next_fn = range_next_noop,
        .measure_fn = measure_tick_cont
    },
    [MODE_DIODE] = {
        .title = "DIODE",
        .range_name_fn = range_name_diode,
        .range_next_fn = range_next_noop,
        .measure_fn = measure_tick_diode
    },
    [MODE_CAP] = {
        .title = "CAP",
        .range_name_fn = range_name_cap,
        .range_next_fn = range_next_cap,
        .measure_fn = measure_tick_cap
    }
};

static const mode_desc_t *active_mode_desc(void)
{
    if ((uint32_t)g_app.mode >= MODE_COUNT) {
        return &k_mode_desc[MODE_RES];
    }
    return &k_mode_desc[g_app.mode];
}

static void mode_next(void)
{
    app_mode_t prev = g_app.mode;
    uint32_t now = bsp_millis();

    switch (g_app.mode) {
    case MODE_RES:
        g_app.mode = MODE_VDC;
        break;
    case MODE_VDC:
        g_app.mode = MODE_FREQ;
        break;
    case MODE_FREQ:
        g_app.mode = MODE_CONT;
        break;
    case MODE_CONT:
        g_app.mode = MODE_DIODE;
        break;
    case MODE_DIODE:
        g_app.mode = MODE_CAP;
        break;
    case MODE_CAP:
    default:
        g_app.mode = MODE_RES;
        break;
    }

    if ((prev == MODE_CONT) && (g_app.mode != MODE_CONT)) {
        beep_continuous(false);
    }
    if ((prev != MODE_CONT) && (g_app.mode == MODE_CONT)) {
        cont_reset(&g_app.cont_ctx);
    }
    if ((prev == MODE_VDC) && (g_app.mode != MODE_VDC)) {
        vdc_exit(&g_app.vdc_ctx);
    }
    if ((prev != MODE_VDC) && (g_app.mode == MODE_VDC)) {
        vdc_enter(&g_app.vdc_ctx, (vdc_range_t)g_app.vdc_range_sel);
    }
    if ((prev != MODE_FREQ) && (g_app.mode == MODE_FREQ)) {
        diode_drv_off();
        beep_continuous(false);
        mux_set_mode(MUX_MODE_FREQ);
        freq_set_range_sel(g_app.freq_range_sel);
        freq_start();
        g_app.freq_hz = 0.0f;
        g_app.freq_duty = 0.0f;
        g_app.freq_have_valid = false;
        g_app.freq_err = ERR_NO_SIGNAL;
        g_app.freq_enter_ms = now;
        g_app.freq_last_measure_ms = now;
        g_app.freq_last_flush_ok_ms = now;
        g_app.freq_recover_deadline_ms = 0u;
        g_app.freq_recover_state = FREQ_RECOVER_IDLE;
        g_app.freq_reset_used_this_entry = false;
    } else if ((prev == MODE_FREQ) && (g_app.mode != MODE_FREQ)) {
        g_app.freq_recover_state = FREQ_RECOVER_IDLE;
        g_app.freq_recover_deadline_ms = 0u;
        g_app.freq_reset_used_this_entry = false;
    }
    if ((prev == MODE_DIODE) && (g_app.mode != MODE_DIODE)) {
        diode_exit(&g_app.diode_ctx);
    }
    if ((prev != MODE_DIODE) && (g_app.mode == MODE_DIODE)) {
        diode_enter(&g_app.diode_ctx);
    }
    if ((prev == MODE_CAP) && (g_app.mode != MODE_CAP)) {
        cap_leave();
    }
    if ((prev != MODE_CAP) && (g_app.mode == MODE_CAP)) {
        mux_set_mode(MUX_MODE_CAP);
        adc1_mark_input_path_changed();
        cap_enter();
        cap_set_range((cap_range_t)g_app.cap_range_sel);
    }
}

static void toggle_debug_view(void)
{
    app_view_t next_view = (g_app.view == VIEW_RUN_MAIN) ? VIEW_RUN_DEBUG : VIEW_RUN_MAIN;

    if ((g_app.mode == MODE_CONT) && (next_view == VIEW_RUN_DEBUG)) {
        /* Hard rule: entering debug from CONT should mute immediately. */
        beep_continuous(false);
    }
    g_app.view = next_view;
}

static bool is_short_up_event(const key_event_t *evt, bool long_fired)
{
    if ((evt == NULL) || (evt->type != KEY_EVT_UP) || long_fired) {
        return false;
    }
    return (evt->duration_ms >= KEY_SHORT_MIN_MS);
}

static void format_rcalc_line(char *out, size_t out_sz, bool valid, float r_calc_ohm)
{
    uint32_t scaled;

    if ((out == NULL) || (out_sz == 0u)) {
        return;
    }

    if (!valid) {
        (void)snprintf(out, out_sz, "RCALC:----");
        return;
    }

    if (r_calc_ohm < 0.0f) {
        (void)snprintf(out, out_sz, "RCALC:<0");
        return;
    }

    scaled = (uint32_t)(r_calc_ohm * 10.0f + 0.5f);
    (void)snprintf(out, out_sz, "RCALC:%lu.%01lu",
                   (unsigned long)(scaled / 10u),
                   (unsigned long)(scaled % 10u));
}

static void format_cap_value_line(const cap_result_t *cap, char *out, size_t out_sz)
{
    uint32_t pf_x10;
    uint32_t nf_x100;
    uint32_t uf_x100;

    if ((out == NULL) || (out_sz == 0u) || (cap == NULL)) {
        return;
    }

    if (cap->stat == CAP_STAT_OL) {
        (void)snprintf(out, out_sz, "C: OL");
        return;
    }
    if ((cap->stat != CAP_STAT_OK) || (cap->valid == 0u)) {
        (void)snprintf(out, out_sz, "C: ----");
        return;
    }

    if (cap->value_pf < 1000.0f) {
        pf_x10 = (uint32_t)(cap->value_pf * 10.0f + 0.5f);
        (void)snprintf(out, out_sz, "C:%lu.%01lu pF",
                       (unsigned long)(pf_x10 / 10u),
                       (unsigned long)(pf_x10 % 10u));
        return;
    }

    if (cap->value_pf < 1000000.0f) {
        nf_x100 = (uint32_t)(cap->value_pf / 10.0f + 0.5f); /* nF * 100 */
        (void)snprintf(out, out_sz, "C:%lu.%02lu nF",
                       (unsigned long)(nf_x100 / 100u),
                       (unsigned long)(nf_x100 % 100u));
        return;
    }

    uf_x100 = (uint32_t)(cap->value_pf / 10000.0f + 0.5f); /* uF * 100 */
    (void)snprintf(out, out_sz, "C:%lu.%02lu uF",
                   (unsigned long)(uf_x100 / 100u),
                   (unsigned long)(uf_x100 % 100u));
}

static void build_main_frame(app_ui_frame_t *frame)
{
    const mode_desc_t *md = active_mode_desc();
    const char *range = md->range_name_fn(&g_app);

    memset(frame, 0, sizeof(*frame));

    (void)snprintf(frame->line[0], sizeof(frame->line[0]), "FUNC: %s", md->title);
    if (g_app.mode == MODE_DIODE) {
        if (g_app.diode.stat == DIODE_STAT_OK) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "A=RED K=BLK");
        } else if (g_app.diode.stat == DIODE_STAT_OL) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "REV/OPEN");
        } else if (g_app.diode.stat == DIODE_STAT_SHORT) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "SHORT");
        } else if (g_app.diode.stat == DIODE_STAT_ERR) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MEAS ERR");
        } else {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "PROBE...");
        }
    } else if (g_app.mode == MODE_CONT) {
        if (!g_app.cont.sample_valid) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "CONT: PROBE...");
        } else {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "CONT: %s",
                           g_app.cont.beep_on ? "BEEP" : "OPEN");
        }
    } else if (g_app.mode == MODE_VDC) {
        if (g_app.vdc_ui_sel == VDC_UI_SEL_AUTO) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG: AUTO %s", vdc_active_short_name(&g_app));
        } else {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG: %s", range);
        }
    } else if (g_app.mode == MODE_FREQ) {
        if (g_app.freq_range_sel == FREQ_RANGE_AUTO) {
            uint8_t active_sel = freq_get_active_range_sel();
            const char *active = k_freq_name[(active_sel < FREQ_RANGE_COUNT) ? active_sel : FREQ_RANGE_2KHZ];
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG: AUTO %s", active);
        } else {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG: %s", range);
        }
    } else if ((g_app.mode == MODE_RES) && (g_app.res_range_sel == RES_RANGE_SEL_AUTO)) {
        const char *locked = measure_res_range_name(g_app.res_auto.locked_range_sel);
        if (measure_res_range_is_exp(g_app.res_auto.locked_range_sel)) {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: AUTO %s EXP", locked);
        } else {
            (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: AUTO %s", locked);
        }
    } else if ((g_app.mode == MODE_RES) && measure_res_range_is_exp(g_app.res_range_sel)) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: %s EXP", range);
    } else {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: %s", range);
    }

    if (g_app.mode == MODE_RES) {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "%s", g_app.res_disp.line_value);
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "%s", g_app.res_disp.line_stat);
    } else if (g_app.mode == MODE_CONT) {
        const char *state = cont_get_state_name(g_app.cont.state);
        if (!g_app.cont.sample_valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "STAT: PROBE");
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "STAT: %s", state);
        }
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "BEEP: %s", g_app.cont.beep_on ? "ON" : "OFF");
        (void)snprintf(frame->line[4], sizeof(frame->line[4]), "CONT MODE");
    } else if (g_app.mode == MODE_DIODE) {
        if (g_app.diode.stat == DIODE_STAT_OK) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "Vf=%lu.%03luV",
                           (unsigned long)(g_app.diode.vf_mv / 1000u),
                           (unsigned long)(g_app.diode.vf_mv % 1000u));
        } else if (g_app.diode.stat == DIODE_STAT_OL) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "OL");
        } else if (g_app.diode.stat == DIODE_STAT_SHORT) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "0.000V");
        } else if (g_app.diode.stat == DIODE_STAT_ERR) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "ERR");
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "Vf=----");
        }

        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT: %s", diode_stat_name(g_app.diode.stat));
    } else if (g_app.mode == MODE_VDC) {
        if (g_app.vdc.status == VDC_STAT_OK) {
            if (g_app.vdc.range == VDC_RANGE_2000MV) {
                (void)snprintf(frame->line[2], sizeof(frame->line[2]), "VAL: %lumV",
                               (unsigned long)g_app.vdc.vin_mv);
            } else {
                (void)snprintf(frame->line[2], sizeof(frame->line[2]), "VAL: %lu.%02luV",
                               (unsigned long)(g_app.vdc.vin_mv / 1000u),
                               (unsigned long)((g_app.vdc.vin_mv % 1000u) / 10u));
            }
        } else if (g_app.vdc.status == VDC_STAT_OL) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "OL");
        } else if (g_app.vdc.status == VDC_STAT_MUX_BAD) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "MUX BAD");
        } else if (g_app.vdc.status == VDC_STAT_ADC_BAD) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "ADC BAD");
        } else if (g_app.vdc.status == VDC_STAT_ERR) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "ERR");
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "PROBE...");
        }
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT: %s",
                       vdc_status_name(g_app.vdc.status));
    } else if (g_app.mode == MODE_FREQ) {
        if (g_app.freq_err == ERR_OK) {
            char fbuf[16];
            const char *funit = "Hz";
            uint32_t duty_i = (uint32_t)(g_app.freq_duty + 0.5f);
            if (duty_i > 100u) {
                duty_i = 100u;
            }
            format_freq_main_value(g_app.freq_hz, fbuf, sizeof(fbuf), &funit);
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "F:%s %s", fbuf, funit);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:%lu%%", (unsigned long)duty_i);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: OK");
        } else if (g_app.freq_err == ERR_OVERRANGE) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "OL");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:--%%");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: OVER");
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "NO SIG");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:--%%");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: NO SIG");
        }
    } else if (g_app.mode == MODE_CAP) {
        format_cap_value_line(&g_app.cap, frame->line[2], sizeof(frame->line[2]));
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT: %s",
                       cap_stat_name(g_app.cap.stat));
    } else {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "VALUE: READY");
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT : READY");
    }

    frame->line[7][0] = '\0';
}

static void build_debug_frame(app_ui_frame_t *frame)
{
    const mode_desc_t *md = active_mode_desc();
    const res_sample_t *s = &g_app.res_sample;
    char rc_line[22];
    bool rcalc_valid;
    uint8_t mode_mux_idx;
    uint8_t res_mux_idx;

    memset(frame, 0, sizeof(*frame));

    (void)snprintf(frame->line[0], sizeof(frame->line[0]), "DEBUG %s OP1", md->title);
    mode_mux_idx = (uint8_t)(mux_get_mode_phys_ch() & 0x07u);
    res_mux_idx = (uint8_t)(mux_get_res_range() & 0x07u);
    if (g_app.mode == MODE_DIODE) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MODEMUX:%c",
                       (char)('0' + mode_mux_idx));
    } else {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MODEMUX:%c RESMUX:%c",
                       (char)('0' + mode_mux_idx),
                       (char)('0' + res_mux_idx));
    }

    rcalc_valid = (s->valid && (g_app.res_calc_err == ERR_OK));
    format_rcalc_line(rc_line, sizeof(rc_line), rcalc_valid, g_app.res_r_calc_ohm);

    if ((g_app.mode == MODE_RES) && s->valid) {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)s->raw_u16);
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)s->mv);
        (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:%lu", (unsigned long)s->vdda_mv);
        (void)snprintf(frame->line[5], sizeof(frame->line[5]), "RN:%lu RE:%lu",
                       (unsigned long)g_app.res_binding.param.rref_nom_ohm,
                       (unsigned long)g_app.res_binding.param.rref_eff_ohm);
        if (g_app.res_range_sel == RES_RANGE_SEL_AUTO) {
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "A:%s U%uD%u",
                           measure_res_range_name(g_app.res_auto.locked_range_sel),
                           (unsigned)g_app.res_auto.vote_up,
                           (unsigned)g_app.res_auto.vote_down);
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "RC:%.6s RD:%.6s",
                           rc_line + 6,
                           g_app.res_disp.r_disp_str);
        } else {
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "%s", rc_line);
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "RD:%.7s ST:%.5s",
                           g_app.res_disp.r_disp_str,
                           g_app.res_disp.stat_str);
        }
    } else if (g_app.mode == MODE_CONT) {
        if (g_app.cont.sample_valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)g_app.cont.sample.raw_u16);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)g_app.cont.sample.mv);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:%lu", (unsigned long)g_app.cont.sample.vdda_mv);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:----");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :----");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:----");
        }

        if (g_app.cont.calc_ok) {
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "CONT_EST:%.1f", g_app.cont.r_est_ohm);
        } else {
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "CONT_EST:----");
        }
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "BEEP:%s V%u/%u",
                       g_app.cont.beep_on ? "ON" : "OFF",
                       (unsigned)g_app.cont.vote_enter,
                       (unsigned)g_app.cont.vote_exit);
        (void)snprintf(frame->line[7], sizeof(frame->line[7]), "STAT:%s",
                       cont_get_state_name(g_app.cont.state));
    } else if (g_app.mode == MODE_DIODE) {
        if (g_app.diode.valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)g_app.diode.raw_u16);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)g_app.diode.mv);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:%lu", (unsigned long)g_app.diode.vdda_mv);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:----");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :----");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:----");
        }
        (void)snprintf(frame->line[5], sizeof(frame->line[5]), "DSTAT:%s", diode_stat_name(g_app.diode.stat));
        if (g_app.diode.stat == DIODE_STAT_OK) {
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "VF:%lu.%03luV",
                           (unsigned long)(g_app.diode.vf_mv / 1000u),
                           (unsigned long)(g_app.diode.vf_mv % 1000u));
        } else {
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "VF:----");
        }
        (void)snprintf(frame->line[7], sizeof(frame->line[7]), "ERR:%u",
                       (unsigned)g_app.diode.err);
    } else if (g_app.mode == MODE_CAP) {
        if (g_app.cap.valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "ADC:%u THR:%u",
                           (unsigned)g_app.cap.adc_raw_last,
                           (unsigned)g_app.cap.adc_threshold);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "CYC:%lu",
                           (unsigned long)g_app.cap.elapsed_cycles);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "PF:%lu NF:%lu",
                           (unsigned long)(g_app.cap.value_pf + 0.5f),
                           (unsigned long)(g_app.cap.value_nf + 0.5f));
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "UF:%lu.%02lu",
                           (unsigned long)g_app.cap.value_uf,
                           (unsigned long)((uint32_t)(g_app.cap.value_uf * 100.0f + 0.5f) % 100u));
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "ADC:%u THR:%u",
                           (unsigned)g_app.cap.adc_raw_last,
                           (unsigned)g_app.cap.adc_threshold);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "CYC:%lu",
                           (unsigned long)g_app.cap.elapsed_cycles);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "CAP:----");
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "RNG:%s",
                           cap_range_name((cap_range_t)g_app.cap_range_sel));
        }
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "STAT:%s",
                       cap_stat_name(g_app.cap.stat));
        (void)snprintf(frame->line[7], sizeof(frame->line[7]), "ERR:%u O:%u",
                       (unsigned)g_app.cap.err,
                       (unsigned)g_app.cap.over);
    } else if (g_app.mode == MODE_VDC) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MODE_CH:%u VOLT:%u %s",
                       (unsigned)mux_get_mode_phys_ch(),
                       (unsigned)mux_get_volt_phys_ch(),
                       vdc_active_short_name(&g_app));
        if (g_app.vdc.valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)g_app.vdc.raw);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)g_app.vdc.mv_sense);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VR20:%lu VC20:%lu",
                           (unsigned long)g_app.vdc.vin_raw_mv,
                           (unsigned long)g_app.vdc.vin_corr_mv);
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "VIN:%lu", (unsigned long)g_app.vdc.vin_mv);
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "VDDA:%lu", (unsigned long)g_app.vdc.vdda_mv);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:----");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :----");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VR20:---- VC20:----");
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "VIN:----");
            (void)snprintf(frame->line[6], sizeof(frame->line[6]), "VDDA:----");
        }
        (void)snprintf(frame->line[7], sizeof(frame->line[7]), "STAT:%s", vdc_status_name(g_app.vdc.status));
    } else if (g_app.mode == MODE_FREQ) {
        bsp_capture_t cap = {0};
        bsp_freq_diag_t diag = {0};
        freq_debug_snapshot_t fdbg = {0};
        bool cap_ok = bsp_freq_get_capture(&cap) && cap.valid;
        (void)bsp_freq_get_diag(&diag);
        freq_get_debug_snapshot(&fdbg);

        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MODE:%u S:%s A:%s",
                       (unsigned)mux_get_mode_phys_ch(),
                       k_freq_name[g_app.freq_range_sel % FREQ_RANGE_COUNT],
                       k_freq_name[freq_get_active_range_sel() % FREQ_RANGE_COUNT]);
        if (cap_ok) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "P:%lu H:%lu",
                           (unsigned long)cap.period_ticks,
                           (unsigned long)cap.high_ticks);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "CLK:%lu",
                           (unsigned long)cap.tim_clk_hz);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "P:---- H:----");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "CLK:----");
        }
        (void)snprintf(frame->line[4], sizeof(frame->line[4]), "IRQ:%lu C:%lu/%lu",
                       (unsigned long)diag.tim2_irq_count,
                       (unsigned long)diag.cap_ch1_count,
                       (unsigned long)diag.cap_ch2_count);
        (void)snprintf(frame->line[5], sizeof(frame->line[5]), "C1:%lu C2:%lu",
                       (unsigned long)diag.last_ccr1,
                       (unsigned long)diag.last_ccr2);
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "INV:%lu ST:%u E:%u",
                       (unsigned long)diag.invalid_h_gt_p_count,
                       (unsigned)diag.capture_start_ok,
                       (unsigned)g_app.freq_err);
        if (g_app.freq_err == ERR_OK) {
            uint32_t hz_i = (uint32_t)(g_app.freq_hz + 0.5f);
            uint32_t duty_i = (uint32_t)(g_app.freq_duty + 0.5f);
            if (duty_i > 100u) {
                duty_i = 100u;
            }
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "F:%lu D:%lu I:%u",
                           (unsigned long)hz_i,
                           (unsigned long)duty_i,
                           (unsigned)fdbg.invalid_count);
        } else {
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "NO SIG H:%u",
                           (unsigned)fdbg.hist_count);
        }
    } else {
        if (g_app.dbg_raw_valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)g_app.dbg_raw_u16);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:----");
        }
        if (g_app.dbg_mv_valid) {
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)g_app.dbg_mv);
        } else {
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :----");
        }
        if (g_app.dbg_vdda_valid) {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:%lu", (unsigned long)g_app.dbg_vdda_mv);
        } else {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:----");
        }
        (void)snprintf(frame->line[5], sizeof(frame->line[5]), "RREF:----/----");
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "RCALC:----");
        if (g_app.adc_last_err == ERR_OK) {
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "S:OK D:----");
        } else {
            (void)snprintf(frame->line[7], sizeof(frame->line[7]), "S:ERR%u D:----",
                           (unsigned)g_app.adc_last_err);
        }
    }
}

void app_init(void)
{
    app_err_t err;
    uint32_t now = bsp_millis();
    uint8_t i;

    memset(&g_app, 0, sizeof(g_app));

    bsp_keys_init();
    beep_init(BEEP_FREQ_HZ);

    g_app.mode = MODE_RES;
    g_app.view = VIEW_RUN_MAIN;
    g_app.res_range_sel = RES_RANGE_SEL_AUTO;
    g_app.vdc_range_sel = VDC_RANGE_2000MV;
    g_app.vdc_ui_sel = VDC_UI_SEL_AUTO;
    g_app.vdc_auto_vote_up = 0u;
    g_app.vdc_auto_vote_down = 0u;
    g_app.freq_range_sel = FREQ_RANGE_AUTO;
    g_app.cap_range_sel = CAP_RANGE_2U;
    g_app.freq_hz = 0.0f;
    g_app.freq_duty = 0.0f;
    g_app.freq_have_valid = false;
    g_app.freq_err = ERR_NO_SIGNAL;
    g_app.freq_enter_ms = now;
    g_app.freq_last_measure_ms = now;
    g_app.freq_last_flush_ok_ms = now;
    g_app.freq_recover_deadline_ms = 0u;
    g_app.freq_recover_state = FREQ_RECOVER_IDLE;
    g_app.freq_reset_used_this_entry = false;

    g_app.adc_init_err = adc1_init();
    g_app.opamp_init_err = opamp1_init();
    g_app.adc_last_err = (g_app.adc_init_err != ERR_OK) ? g_app.adc_init_err : g_app.opamp_init_err;
    ui_update_debug_adc_sample();

    for (i = 0u; i < RES_RANGE_SEL_COUNT; i++) {
        res_afe_diag_reset(i);
    }
    measure_res_auto_reset();
    cont_init(&g_app.cont_ctx);
    memset(&g_app.cont, 0, sizeof(g_app.cont));
    g_app.cont.state = CONT_STATE_OPEN;
    g_app.cont.err = ERR_OK;
    vdc_init(&g_app.vdc_ctx);
    memset(&g_app.vdc, 0, sizeof(g_app.vdc));
    g_app.vdc.range = (vdc_range_t)g_app.vdc_range_sel;
    g_app.vdc.status = VDC_STAT_PROBE;
    g_app.vdc.err = ERR_NOT_IMPL;
    diode_init(&g_app.diode_ctx);
    diode_exit(&g_app.diode_ctx);
    memset(&g_app.diode, 0, sizeof(g_app.diode));
    g_app.diode.stat = DIODE_STAT_PROBE;
    g_app.diode.err = ERR_NOT_IMPL;
    cap_result_reset(&g_app.cap);
    g_app.cap.range = (cap_range_t)g_app.cap_range_sel;
    g_app.cap.stat = CAP_STAT_PROBE;
    g_app.cap.err = ERR_NOT_IMPL;
    cap_set_range((cap_range_t)g_app.cap_range_sel);
    g_app.res_auto_active = false;
    freq_set_range_sel(g_app.freq_range_sel);
    (void)measure_res_get_binding(g_app.res_range_sel, &g_app.res_binding);
    res_format_display(&g_app.res_binding, &g_app.res_sample, false, false, 0.0f, &g_app.res_disp);

    bootdiag_set_stage(BOOT_DISPLAY_INIT);
    err = app_ui_presenter_init();
    g_app.presenter_err = err;
    if (err != ERR_OK) {
        bootdiag_set_fault(BOOT_FAULT_DISPLAY_INIT);
        bootdiag_set_stage(BOOT_FAULT);
    } else {
        (void)app_display_show_boot_splash();
        bootdiag_set_fault(BOOT_FAULT_NONE);
        bootdiag_set_stage(BOOT_RUN);
    }

    g_app.next_ui_ms = now;
    g_app.next_debug_adc_ms = now;
    g_app.next_meas_ms = now;
    g_app.ui_dirty = true;
}

void app_poll_button(void)
{
    key_event_t evt;
    uint32_t now = bsp_millis();
    bool changed = false;

    keys_poll();

    while (keys_get_event(&evt)) {
        switch (evt.key) {
        case KEY_RIGHT:
            if (evt.type == KEY_EVT_DOWN) {
                g_app.right_long_fired = false;
            } else if (evt.type == KEY_EVT_LONG) {
                bool from_freq = (g_app.mode == MODE_FREQ);
                g_app.right_long_fired = true;
                mode_next();
                if (!from_freq) {
                    beep_once(80u);
                }
                changed = true;
            } else if (is_short_up_event(&evt, g_app.right_long_fired)) {
                active_mode_desc()->range_next_fn(&g_app);
                if (g_app.mode != MODE_FREQ) {
                    beep_once(30u);
                }
                changed = true;
            }
            break;

        case KEY_LEFT:
#if APP_DEBUG_LEFT_KEY_ENABLE
            if (evt.type == KEY_EVT_DOWN) {
                g_app.left_long_fired = false;
            } else if (evt.type == KEY_EVT_LONG) {
                g_app.left_long_fired = true;
            } else if (is_short_up_event(&evt, g_app.left_long_fired)) {
                toggle_debug_view();
                if (g_app.mode != MODE_FREQ) {
                    beep_once(25u);
                }
                changed = true;
            }
#endif
            break;

        default:
            /* Formal build consumes only RIGHT (and optional LEFT debug toggle). */
            break;
        }
    }

    if (changed) {
        g_app.ui_dirty = true;
        g_app.next_ui_ms = now;
        g_app.next_meas_ms = now;
    }
}

void app_measure_tick(void)
{
    uint32_t now = bsp_millis();
    uint32_t period_ms = MEAS_PERIOD_MS;

    if (g_app.mode == MODE_CONT) {
        period_ms = CONT_MEAS_PERIOD_MS;
    } else if (g_app.mode == MODE_VDC) {
        period_ms = VDC_SAMPLE_PERIOD_MS;
    }

    if ((int32_t)(now - g_app.next_meas_ms) < 0) {
        return;
    }
    g_app.next_meas_ms = now + period_ms;

    active_mode_desc()->measure_fn(&g_app, now);
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();
    app_ui_frame_t frame;
    app_err_t err;

    app_display_poll();
    freq_recovery_tick(now);

    if ((int32_t)(now - g_app.next_debug_adc_ms) >= 0) {
        if ((g_app.mode != MODE_RES) && (g_app.mode != MODE_CONT) &&
            (g_app.mode != MODE_DIODE) && (g_app.mode != MODE_VDC) &&
            (g_app.mode != MODE_CAP)) {
            ui_update_debug_adc_sample();
        }
        g_app.next_debug_adc_ms = now + DEBUG_ADC_REFRESH_MS;
        if (g_app.view == VIEW_RUN_DEBUG) {
            g_app.ui_dirty = true;
        }
    }

    if ((int32_t)(now - g_app.next_ui_ms) < 0) {
        return;
    }
    g_app.next_ui_ms = now + UI_REFRESH_MS;

    if (!g_app.ui_dirty) {
        return;
    }

    if (!app_ui_presenter_ready()) {
        g_app.presenter_err = app_ui_presenter_last_err();
        if (g_app.mode != MODE_FREQ) {
            bootdiag_set_fault(BOOT_FAULT_DISPLAY_INIT);
            bootdiag_set_stage(BOOT_FAULT);
        }
        g_app.ui_dirty = true;
        return;
    }

    if (g_app.view == VIEW_RUN_DEBUG) {
        build_debug_frame(&frame);
    } else {
        build_main_frame(&frame);
    }

    err = app_ui_presenter_flush(&frame);
    if (err != ERR_OK) {
        g_app.presenter_err = err;
        if (g_app.mode != MODE_FREQ) {
            bootdiag_set_fault(BOOT_FAULT_UI_FLUSH);
            bootdiag_set_stage(BOOT_FAULT);
        }
        g_app.ui_dirty = true;
        return;
    }

    if (g_app.mode == MODE_FREQ) {
        g_app.freq_last_flush_ok_ms = now;
        g_app.freq_recover_state = FREQ_RECOVER_IDLE;
    }
    g_app.ui_dirty = false;
}

void app_beep_tick(void)
{
    beep_tick();
}
