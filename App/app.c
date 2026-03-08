#include "app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"
#include "../Measurements/measure_res.h"
#include "../Measurements/res_afe_diag.h"
#include "../Measurements/res_display_fmt.h"
#include "app_bootdiag.h"
#include "app_display_service.h"
#include "app_types.h"
#include "app_ui_presenter.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define MEAS_PERIOD_MS 40u
#define BEEP_FREQ_HZ 2700u

typedef enum {
    VIEW_RUN_MAIN = 0,
    VIEW_RUN_DEBUG
} app_view_t;

typedef struct {
    app_mode_t mode;
    app_view_t view;

    uint8_t res_range_sel;
    uint8_t vdc_range_sel;
    uint8_t freq_range_sel;

    bool right_long_fired;
    bool left_long_fired;

    app_err_t presenter_err;
    app_err_t adc_init_err;
    app_err_t adc_last_err;

    bool dbg_raw_valid;
    bool dbg_mv_valid;
    bool dbg_vdda_valid;
    uint16_t dbg_raw_u16;
    uint32_t dbg_mv;
    uint32_t dbg_vdda_mv;

    res_sample_t res_sample;
    res_afe_health_t res_health;
    bool res_afe_ok;
    bool res_calc_ok;
    app_err_t res_calc_err;
    float res_r_ohm;
    res_display_text_t res_disp;

    uint32_t next_meas_ms;
    uint32_t next_ui_ms;
    uint32_t next_debug_adc_ms;
    bool ui_dirty;
} app_ctx_t;

static app_ctx_t g_app;

static const char *k_mode_name[MODE_COUNT] = {
    "RES",
    "VDC",
    "FREQ",
    "CONT",
    "DIODE"
};

static const char *k_vdc_name[VDC_RANGE_COUNT] = {"2000mV", "20V"};
static const char *k_freq_name[FREQ_RANGE_COUNT] = {"20Hz", "200Hz", "2kHz", "20kHz", "200kHz"};

static const char *mode_name(app_mode_t mode)
{
    if ((uint32_t)mode >= MODE_COUNT) {
        return "UNK";
    }
    return k_mode_name[mode];
}

static const char *range_name_for_mode(void)
{
    if (g_app.mode == MODE_RES) {
        return measure_res_range_name(g_app.res_range_sel);
    }
    if (g_app.mode == MODE_VDC) {
        return k_vdc_name[g_app.vdc_range_sel % VDC_RANGE_COUNT];
    }
    if (g_app.mode == MODE_FREQ) {
        return k_freq_name[g_app.freq_range_sel % FREQ_RANGE_COUNT];
    }
    if (g_app.mode == MODE_CONT) {
        return "BEEP";
    }
    return "Vf";
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

static void mode_next(void)
{
    g_app.mode = (app_mode_t)((g_app.mode + 1u) % MODE_COUNT);
}

static void range_next_in_mode(void)
{
    if (g_app.mode == MODE_RES) {
        g_app.res_range_sel = (uint8_t)((g_app.res_range_sel + 1u) % RES_RANGE_SEL_COUNT);
    } else if (g_app.mode == MODE_VDC) {
        g_app.vdc_range_sel = (uint8_t)((g_app.vdc_range_sel + 1u) % VDC_RANGE_COUNT);
    } else if (g_app.mode == MODE_FREQ) {
        g_app.freq_range_sel = (uint8_t)((g_app.freq_range_sel + 1u) % FREQ_RANGE_COUNT);
    }
}

static void toggle_debug_view(void)
{
    g_app.view = (g_app.view == VIEW_RUN_MAIN) ? VIEW_RUN_DEBUG : VIEW_RUN_MAIN;
}

static void build_main_frame(app_ui_frame_t *frame)
{
    char line[22];

    memset(frame, 0, sizeof(*frame));

    (void)snprintf(frame->line[0], sizeof(frame->line[0]), "FUNC: %s", mode_name(g_app.mode));
    if ((g_app.mode == MODE_RES) && measure_res_range_is_exp(g_app.res_range_sel)) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: %s EXP", range_name_for_mode());
    } else {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RANGE: %s", range_name_for_mode());
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
    } else {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "VALUE: READY");
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "STAT : READY");
    }

    (void)snprintf(frame->line[7], sizeof(frame->line[7]), "R:range/mode L:dbg");
}

static void build_debug_frame(app_ui_frame_t *frame)
{
    const res_sample_t *s = &g_app.res_sample;

    memset(frame, 0, sizeof(*frame));

    (void)snprintf(frame->line[0], sizeof(frame->line[0]), "DEBUG %s", mode_name(g_app.mode));
    if ((g_app.mode == MODE_RES) && measure_res_range_is_exp(g_app.res_range_sel)) {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG:%s EXP", range_name_for_mode());
    } else {
        (void)snprintf(frame->line[1], sizeof(frame->line[1]), "RNG:%s", range_name_for_mode());
    }

    if ((g_app.mode == MODE_RES) && s->valid) {
        (void)snprintf(frame->line[2], sizeof(frame->line[2]), "RAW:%u", (unsigned)s->raw_u16);
        (void)snprintf(frame->line[3], sizeof(frame->line[3]), "MV :%lu", (unsigned long)s->mv);
        (void)snprintf(frame->line[4], sizeof(frame->line[4]), "VDDA:%lu", (unsigned long)s->vdda_mv);
        (void)snprintf(frame->line[5], sizeof(frame->line[5]), "S:%u O:%u A:%u",
                       g_app.res_health.short_seen ? 1u : 0u,
                       g_app.res_health.open_seen ? 1u : 0u,
                       g_app.res_afe_ok ? 1u : 0u);
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "%s", g_app.res_disp.line_stat);
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
        (void)snprintf(frame->line[6], sizeof(frame->line[6]), "STAT: READY");
    }

    (void)snprintf(frame->line[7], sizeof(frame->line[7]), "LEFT:back");
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
    g_app.res_range_sel = RES_RANGE_SEL_2K;
    g_app.vdc_range_sel = 0u;
    g_app.freq_range_sel = 0u;

    g_app.adc_init_err = adc1_init();
    g_app.adc_last_err = g_app.adc_init_err;
    ui_update_debug_adc_sample();

    for (i = 0u; i < RES_RANGE_SEL_COUNT; i++) {
        res_afe_diag_reset(i);
    }
    res_format_display(g_app.res_range_sel, &g_app.res_sample, false, false, 0.0f, &g_app.res_disp);

    bootdiag_set_stage(BOOT_DISPLAY_INIT);
    err = app_ui_presenter_init();
    g_app.presenter_err = err;
    if (err != ERR_OK) {
        bootdiag_set_fault(BOOT_FAULT_DISPLAY_INIT);
        bootdiag_set_stage(BOOT_FAULT);
    } else {
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
                g_app.right_long_fired = true;
                mode_next();
                beep_once(80u);
                changed = true;
            } else if ((evt.type == KEY_EVT_UP) && !g_app.right_long_fired) {
                range_next_in_mode();
                beep_once(30u);
                changed = true;
            }
            break;

        case KEY_LEFT:
            if (evt.type == KEY_EVT_DOWN) {
                g_app.left_long_fired = false;
            } else if (evt.type == KEY_EVT_LONG) {
                g_app.left_long_fired = true;
            } else if ((evt.type == KEY_EVT_UP) && !g_app.left_long_fired) {
                toggle_debug_view();
                beep_once(25u);
                changed = true;
            }
            break;

        default:
            /* OK/BACK are dev-only in this phase; ignored by formal UI path. */
            break;
        }
    }

    if (changed) {
        g_app.ui_dirty = true;
        g_app.next_ui_ms = now;
    }
}

void app_measure_tick(void)
{
    uint32_t now = bsp_millis();
    app_err_t err;

    if ((int32_t)(now - g_app.next_meas_ms) < 0) {
        return;
    }
    g_app.next_meas_ms = now + MEAS_PERIOD_MS;

    if (g_app.mode != MODE_RES) {
        return;
    }

    err = res_acquire_sample(g_app.res_range_sel, &g_app.res_sample);
    if (err != ERR_OK) {
        g_app.res_afe_ok = false;
        g_app.res_calc_ok = false;
        g_app.res_calc_err = err;
        res_format_display(g_app.res_range_sel, &g_app.res_sample, false, false, 0.0f, &g_app.res_disp);
        g_app.ui_dirty = true;
        return;
    }

    res_afe_diag_update(g_app.res_range_sel, &g_app.res_sample);
    g_app.res_health = res_afe_diag_get(g_app.res_range_sel);
    g_app.res_afe_ok = res_check_afe_health(g_app.res_range_sel, &g_app.res_sample);

    if (g_app.res_afe_ok) {
        g_app.res_calc_err = res_estimate_rx(g_app.res_range_sel, &g_app.res_sample, &g_app.res_r_ohm);
        g_app.res_calc_ok = (g_app.res_calc_err == ERR_OK);
    } else {
        g_app.res_calc_ok = false;
        g_app.res_calc_err = ERR_HW_FAIL;
        g_app.res_r_ohm = 0.0f;
    }

    res_format_display(g_app.res_range_sel,
                       &g_app.res_sample,
                       g_app.res_afe_ok,
                       g_app.res_calc_ok,
                       g_app.res_r_ohm,
                       &g_app.res_disp);
    g_app.ui_dirty = true;
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();
    app_ui_frame_t frame;
    app_err_t err;

    app_display_poll();

    if ((int32_t)(now - g_app.next_debug_adc_ms) >= 0) {
        if (g_app.mode != MODE_RES) {
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
