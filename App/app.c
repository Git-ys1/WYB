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
#include "../Measurements/res_afe_diag.h"
#include "../Measurements/res_display_fmt.h"
#include "app_bootdiag.h"
#include "app_display_service.h"
#include "app_types.h"
#include "app_ui_presenter.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define MEAS_PERIOD_MS 40u
#define CONT_MEAS_PERIOD_MS 25u
#define BEEP_FREQ_HZ 2700u
#define DIODE_VF_DIRTY_DELTA_MV 8u
#define DIODE_RAW_DIRTY_DELTA 16u
#define APP_DEBUG_LEFT_KEY_ENABLE 1u

#define KEY_SHORT_MIN_MS 15u

typedef enum {
    VIEW_RUN_MAIN = 0,
    VIEW_RUN_DEBUG
} app_view_t;

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
    uint8_t freq_range_sel;
    float freq_hz;
    float freq_duty;
    bool freq_have_valid;
    app_err_t freq_err;

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
    diode_ctx_t diode_ctx;
    diode_latched_result_t diode;

    uint32_t next_meas_ms;
    uint32_t next_ui_ms;
    uint32_t next_debug_adc_ms;
    bool ui_dirty;
};

static app_ctx_t g_app;

static const char *k_vdc_name[VDC_RANGE_COUNT] = {"2000mV", "20V"};
static const char *k_freq_name[FREQ_RANGE_COUNT] = {"AUTO", "20Hz", "200Hz", "2kHz", "20kHz", "200kHz"};

static const char *range_name_res(const app_ctx_t *ctx)
{
    return measure_res_range_name(ctx->res_range_sel);
}

static const char *range_name_vdc(const app_ctx_t *ctx)
{
    return k_vdc_name[ctx->vdc_range_sel % VDC_RANGE_COUNT];
}

static const char *range_name_freq(const app_ctx_t *ctx)
{
    return k_freq_name[ctx->freq_range_sel % FREQ_RANGE_COUNT];
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
    ctx->vdc_range_sel = (uint8_t)((ctx->vdc_range_sel + 1u) % VDC_RANGE_COUNT);
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
            ctx->ui_dirty = true;
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
        ctx->ui_dirty = true;
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
        ctx->ui_dirty = true;
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
        ctx->ui_dirty = true;
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
    ctx->ui_dirty = true;
}

static void measure_tick_freq(app_ctx_t *ctx, uint32_t now_ms)
{
    float hz = 0.0f;
    float duty = 0.0f;
    app_err_t err;

    (void)now_ms;

    err = freq_get(&hz, &duty);
    ctx->freq_err = err;
    if (err == ERR_OK) {
        ctx->freq_hz = hz;
        ctx->freq_duty = duty;
        ctx->freq_have_valid = true;
    }

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
        ctx->ui_dirty = true;
        return;
    }

    err = vdc_get_result(&ctx->vdc_ctx, &ctx->vdc);
    if (err != ERR_OK) {
        ctx->vdc.valid = false;
        ctx->vdc.status = VDC_STAT_ERR;
        ctx->vdc.err = err;
    }

    ctx->ui_dirty = true;
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
    }
    if ((prev == MODE_DIODE) && (g_app.mode != MODE_DIODE)) {
        diode_exit(&g_app.diode_ctx);
    }
    if ((prev != MODE_DIODE) && (g_app.mode == MODE_DIODE)) {
        diode_enter(&g_app.diode_ctx);
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

static void build_main_frame(app_ui_frame_t *frame)
{
    const mode_desc_t *md = active_mode_desc();
    const char *range = md->range_name_fn(&g_app);
    char line[22];

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
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG: %s", range);
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
        if (g_app.res_sample.valid) {
            (void)snprintf(line, sizeof(line), "MV:%lu RAW:%u",
                           (unsigned long)g_app.res_sample.mv,
                           (unsigned)g_app.res_sample.raw_u16);
        } else {
            (void)snprintf(line, sizeof(line), "MV:---- RAW:----");
        }
        (void)snprintf(frame->line[4], sizeof(frame->line[4]), "%s", line);
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
        if (g_app.diode.valid) {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "MV:%lu RAW:%u",
                           (unsigned long)g_app.diode.mv,
                           (unsigned)g_app.diode.raw_u16);
        } else {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "MV:---- RAW:----");
        }
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
        if (g_app.vdc.valid) {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VIN:%lu MV:%lu",
                           (unsigned long)g_app.vdc.vin_mv,
                           (unsigned long)g_app.vdc.mv_sense);
        } else {
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VIN:---- MV:----");
        }
    } else if (g_app.mode == MODE_FREQ) {
        if (g_app.freq_err == ERR_OK) {
            uint32_t hz_i = (uint32_t)(g_app.freq_hz + 0.5f);
            uint32_t duty_i = (uint32_t)(g_app.freq_duty + 0.5f);
            if (duty_i > 100u) {
                duty_i = 100u;
            }
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "F:%luHz", (unsigned long)hz_i);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:%lu%%", (unsigned long)duty_i);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: OK");
        } else if (g_app.freq_err == ERR_OVERRANGE) {
            if (g_app.freq_have_valid) {
                uint32_t hz_i = (uint32_t)(g_app.freq_hz + 0.5f);
                uint32_t duty_i = (uint32_t)(g_app.freq_duty + 0.5f);
                if (duty_i > 100u) {
                    duty_i = 100u;
                }
                (void)snprintf(frame->line[2], sizeof(frame->line[2]), "F:%luHz", (unsigned long)hz_i);
                (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:%lu%%", (unsigned long)duty_i);
            } else {
                (void)snprintf(frame->line[2], sizeof(frame->line[2]), "OVER");
                (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:--%%");
            }
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: OVER");
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "NO SIG");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "D:--%%");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "STAT: NO SIG");
        }
    } else {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "VALUE: READY");
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT : READY");
    }

    (void)snprintf(frame->line[7], sizeof(frame->line[7]), "R:RNG/MODE L:DBG");
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
    } else if (g_app.mode == MODE_VDC) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "MODE_CH:%u VOLT:%u",
                       (unsigned)mux_get_mode_phys_ch(),
                       (unsigned)mux_get_volt_phys_ch());
        if (g_app.vdc.valid) {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)g_app.vdc.raw);
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)g_app.vdc.mv_sense);
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VIN:%lu", (unsigned long)g_app.vdc.vin_mv);
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "VDDA:%lu", (unsigned long)g_app.vdc.vdda_mv);
        } else {
            (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:----");
            (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :----");
            (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VIN:----");
            (void)snprintf(frame->line[5], sizeof(frame->line[5]), "VDDA:----");
        }
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "STAT:%s", vdc_status_name(g_app.vdc.status));
        (void)snprintf(frame->line[7], sizeof(frame->line[7]), "ERR:%u", (unsigned)g_app.vdc.err);
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
    g_app.vdc_range_sel = 0u;
    g_app.freq_range_sel = FREQ_RANGE_AUTO;
    g_app.freq_hz = 0.0f;
    g_app.freq_duty = 0.0f;
    g_app.freq_have_valid = false;
    g_app.freq_err = ERR_NO_SIGNAL;

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

    if ((int32_t)(now - g_app.next_debug_adc_ms) >= 0) {
        if ((g_app.mode != MODE_RES) && (g_app.mode != MODE_CONT) && (g_app.mode != MODE_DIODE) && (g_app.mode != MODE_VDC)) {
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
        bootdiag_set_fault(BOOT_FAULT_DISPLAY_INIT);
        bootdiag_set_stage(BOOT_FAULT);
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
        bootdiag_set_fault(BOOT_FAULT_UI_FLUSH);
        bootdiag_set_stage(BOOT_FAULT);
        g_app.ui_dirty = true;
        return;
    }

    g_app.ui_dirty = false;
}

void app_beep_tick(void)
{
    beep_tick();
}
