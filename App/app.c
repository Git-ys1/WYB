#include "app.h"
#include "app_heartbeat.h"
#include "app_ui_presenter.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define BEEP_FREQ_HZ 2700u

typedef enum {
    UI_MOD_DEBUG = 0,
    UI_MOD_MEAS
} ui_module_t;

typedef enum {
    MENU_L1_MODULE = 1,
    MENU_L2_DEBUG_PAGE,
    MENU_L2_MEAS_FUNC,
    MENU_L3_RES_RANGE,
    MENU_L4_RES_READY,
    MENU_L4_RES_RUN
} menu_level_t;

typedef enum {
    MEAS_FUNC_RES = 0,
    MEAS_FUNC_VDC,
    MEAS_FUNC_FREQ,
    MEAS_FUNC_CONT,
    MEAS_FUNC_DIODE,
    MEAS_FUNC_COUNT
} meas_func_t;

typedef enum {
    RES_RANGE_AUTO = 0,
    RES_RANGE_200,
    RES_RANGE_2K,
    RES_RANGE_20K,
    RES_RANGE_200K,
    RES_RANGE_COUNT
} res_range_sel_t;

typedef struct {
    ui_module_t module_sel;
    menu_level_t menu_level;
    meas_func_t meas_func_sel;
    res_range_sel_t res_range_sel;
    bool meas_run_enabled;

    app_err_t presenter_err;
    app_err_t adc_init_err;
    app_err_t adc_last_err;
    bool raw_valid;
    bool mv_valid;
    bool vdda_valid;
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;

    uint32_t next_ui_ms;
    uint32_t next_debug_adc_ms;
    bool ui_dirty;
} app_ctx_t;

static app_ctx_t g_app;
static volatile boot_stage_t g_bootdiag_stage = BOOT_STAGE_10_GPIO_OK;
static volatile int g_bootdiag_err = 0;
static volatile uint32_t g_bootdiag_ms = 0u;

static const char *k_module_name[2] = {"DEBUG", "MEAS"};
static const char *k_meas_name[MEAS_FUNC_COUNT] = {"RES", "VDC", "FREQ", "CONT", "DIODE"};
static const char *k_res_range_name[RES_RANGE_COUNT] = {"AUTO", "200", "2K", "20K", "200K"};

static void bootdiag_set(boot_stage_t stage, int err)
{
    g_bootdiag_stage = stage;
    g_bootdiag_err = err;
    g_bootdiag_ms = bsp_millis();
}

static void frame_clear(app_ui_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
}

static void frame_set_line(app_ui_frame_t *frame, uint8_t line, const char *text)
{
    if ((frame == NULL) || (line >= 8u) || (text == NULL)) {
        return;
    }
    (void)snprintf(frame->line[line], sizeof(frame->line[line]), "%s", text);
}

static void frame_set_linef(app_ui_frame_t *frame, uint8_t line, const char *fmt, ...)
{
    va_list args;

    if ((frame == NULL) || (line >= 8u) || (fmt == NULL)) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(frame->line[line], sizeof(frame->line[line]), fmt, args);
    va_end(args);
}

static void meas_func_step(int dir)
{
    int next = (int)g_app.meas_func_sel + dir;

    if (next < 0) {
        next = (int)MEAS_FUNC_COUNT - 1;
    } else if (next >= (int)MEAS_FUNC_COUNT) {
        next = 0;
    }
    g_app.meas_func_sel = (meas_func_t)next;
}

static void res_range_step(int dir)
{
    int next = (int)g_app.res_range_sel + dir;

    if (next < 0) {
        next = (int)RES_RANGE_COUNT - 1;
    } else if (next >= (int)RES_RANGE_COUNT) {
        next = 0;
    }
    g_app.res_range_sel = (res_range_sel_t)next;
}

static void ui_update_debug_adc_sample(void)
{
    app_err_t err;
    uint32_t vdda = 3300u;
    uint16_t raw = 0u;
    uint32_t mv = 0u;

    g_app.raw_valid = false;
    g_app.mv_valid = false;
    g_app.vdda_valid = false;
    g_app.raw_u16 = 0u;
    g_app.mv = 0u;
    g_app.vdda_mv = 3300u;

    if (g_app.adc_init_err != ERR_OK) {
        g_app.adc_last_err = g_app.adc_init_err;
        return;
    }

    err = adc1_read_raw_u16(&raw);
    if (err == ERR_OK) {
        g_app.raw_valid = true;
        g_app.raw_u16 = raw;
    } else {
        g_app.adc_last_err = err;
        return;
    }

    err = adc1_read_mv(&mv);
    if (err == ERR_OK) {
        g_app.mv_valid = true;
        g_app.mv = mv;
    } else {
        g_app.adc_last_err = err;
        return;
    }

    err = adc1_read_vdda_mv(&vdda);
    if (err == ERR_OK) {
        g_app.vdda_valid = true;
        g_app.vdda_mv = vdda;
        g_app.adc_last_err = ERR_OK;
    } else {
        g_app.vdda_valid = false;
        g_app.vdda_mv = 3300u;
        g_app.adc_last_err = err;
    }
}

static void build_l1_module_frame(app_ui_frame_t *frame)
{
    frame_set_line(frame, 0u, "MAIN MENU");
    frame_set_linef(frame, 1u, "%c DEBUG", (g_app.module_sel == UI_MOD_DEBUG) ? '>' : ' ');
    frame_set_linef(frame, 2u, "%c MEASURE", (g_app.module_sel == UI_MOD_MEAS) ? '>' : ' ');
    frame_set_line(frame, 4u, "L/R U/D: SELECT");
    frame_set_line(frame, 5u, "OK: ENTER");
}

static void build_l2_debug_frame(app_ui_frame_t *frame)
{
    if (g_app.raw_valid) {
        frame_set_linef(frame, 1u, "RAW:%5u", g_app.raw_u16);
    } else {
        frame_set_line(frame, 1u, "RAW: ----");
    }

    if (g_app.mv_valid) {
        frame_set_linef(frame, 2u, "MV :%lu.%03lu",
                        (unsigned long)(g_app.mv / 1000u),
                        (unsigned long)(g_app.mv % 1000u));
    } else {
        frame_set_line(frame, 2u, "MV : ----");
    }

    if (g_app.vdda_valid) {
        frame_set_linef(frame, 3u, "VDDA:%4lu", (unsigned long)g_app.vdda_mv);
    } else {
        frame_set_line(frame, 3u, "VDDA:----");
    }

    if (g_app.adc_last_err == ERR_OK) {
        frame_set_line(frame, 4u, "STAT:OK");
    } else {
        frame_set_linef(frame, 4u, "STAT:ERR%d", (int)g_app.adc_last_err);
    }

    frame_set_line(frame, 0u, "DEBUG/ADC");
    frame_set_line(frame, 6u, "BACK: MAIN");
    frame_set_linef(frame, 7u, "MOD:%s", k_module_name[g_app.module_sel]);
}

static void build_l2_meas_frame(app_ui_frame_t *frame)
{
    uint8_t i;

    frame_set_line(frame, 0u, "MEASURE FUNC");
    for (i = 0u; i < MEAS_FUNC_COUNT; i++) {
        frame_set_linef(frame, (uint8_t)(i + 1u), "%c %s",
                        (g_app.meas_func_sel == (meas_func_t)i) ? '>' : ' ',
                        k_meas_name[i]);
    }
    frame_set_line(frame, 6u, "OK: ENTER/BROWSE");
    frame_set_line(frame, 7u, "BACK: MAIN");
}

static void build_l3_range_frame(app_ui_frame_t *frame)
{
    frame_set_line(frame, 0u, "RES RANGE");
    frame_set_linef(frame, 1u, "SEL: %s", k_res_range_name[g_app.res_range_sel]);
    frame_set_line(frame, 3u, "UP/DN/LR: CHG");
    frame_set_line(frame, 4u, "OK: READY");
    frame_set_line(frame, 5u, "BACK: FUNC");
}

static void build_l4_ready_frame(app_ui_frame_t *frame)
{
    frame_set_line(frame, 0u, "RES READY");
    frame_set_linef(frame, 1u, "RANGE: %s", k_res_range_name[g_app.res_range_sel]);
    frame_set_line(frame, 3u, "PRESS OK TO RUN");
    frame_set_line(frame, 4u, "(RUN DISABLED)");
    frame_set_line(frame, 5u, "UP/DN/LR: CHG");
    frame_set_line(frame, 6u, "BACK: RANGE");
}

static void build_current_frame(app_ui_frame_t *frame)
{
    frame_clear(frame);

    if (g_app.menu_level == MENU_L1_MODULE) {
        build_l1_module_frame(frame);
    } else if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        build_l2_debug_frame(frame);
    } else if (g_app.menu_level == MENU_L2_MEAS_FUNC) {
        build_l2_meas_frame(frame);
    } else if (g_app.menu_level == MENU_L3_RES_RANGE) {
        build_l3_range_frame(frame);
    } else {
        build_l4_ready_frame(frame);
    }
}

static void handle_key_short(key_id_t key)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        if ((key == KEY_UP) || (key == KEY_DOWN)) {
            g_app.module_sel = (g_app.module_sel == UI_MOD_DEBUG) ? UI_MOD_MEAS : UI_MOD_DEBUG;
        } else if (key == KEY_LEFT) {
            g_app.module_sel = UI_MOD_DEBUG;
        } else if (key == KEY_RIGHT) {
            g_app.module_sel = UI_MOD_MEAS;
        } else if (key == KEY_OK) {
            g_app.menu_level = (g_app.module_sel == UI_MOD_DEBUG) ? MENU_L2_DEBUG_PAGE : MENU_L2_MEAS_FUNC;
        }
        return;
    }

    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        if (key == KEY_BACK) {
            g_app.menu_level = MENU_L1_MODULE;
        }
        return;
    }

    if (g_app.menu_level == MENU_L2_MEAS_FUNC) {
        if ((key == KEY_UP) || (key == KEY_LEFT)) {
            meas_func_step(-1);
        } else if ((key == KEY_DOWN) || (key == KEY_RIGHT)) {
            meas_func_step(1);
        } else if (key == KEY_OK) {
            if (g_app.meas_func_sel == MEAS_FUNC_RES) {
                g_app.menu_level = MENU_L3_RES_RANGE;
            }
        } else if (key == KEY_BACK) {
            g_app.menu_level = MENU_L1_MODULE;
        }
        return;
    }

    if (g_app.menu_level == MENU_L3_RES_RANGE) {
        if ((key == KEY_UP) || (key == KEY_LEFT)) {
            res_range_step(-1);
        } else if ((key == KEY_DOWN) || (key == KEY_RIGHT)) {
            res_range_step(1);
        } else if (key == KEY_OK) {
            g_app.menu_level = MENU_L4_RES_READY;
            g_app.meas_run_enabled = false;
        } else if (key == KEY_BACK) {
            g_app.menu_level = MENU_L2_MEAS_FUNC;
            g_app.meas_run_enabled = false;
        }
        return;
    }

    if (g_app.menu_level == MENU_L4_RES_READY) {
        if ((key == KEY_UP) || (key == KEY_LEFT)) {
            res_range_step(-1);
        } else if ((key == KEY_DOWN) || (key == KEY_RIGHT)) {
            res_range_step(1);
        } else if (key == KEY_BACK) {
            g_app.menu_level = MENU_L3_RES_RANGE;
            g_app.meas_run_enabled = false;
        } else if (key == KEY_OK) {
            g_app.meas_run_enabled = false;
        }
    }
}

static void handle_key_long(key_id_t key)
{
    if (key != KEY_OK) {
        return;
    }
    if (g_app.menu_level == MENU_L1_MODULE) {
        return;
    }

    g_app.menu_level = MENU_L1_MODULE;
    g_app.meas_run_enabled = false;
}

void app_init(void)
{
    app_err_t err;
    uint32_t now = bsp_millis();

    memset(&g_app, 0, sizeof(g_app));

    bootdiag_set(BOOT_STAGE_10_GPIO_OK, ERR_OK);
    hb_init();
    bsp_keys_init();
    beep_init(BEEP_FREQ_HZ);

    g_app.module_sel = UI_MOD_DEBUG;
    g_app.menu_level = MENU_L1_MODULE;
    g_app.meas_func_sel = MEAS_FUNC_RES;
    g_app.res_range_sel = RES_RANGE_2K;
    g_app.meas_run_enabled = false;

    err = app_ui_presenter_init();
    g_app.presenter_err = err;
    if (err == ERR_OK) {
        bootdiag_set(BOOT_STAGE_40_OLED_FLUSH_OK, ERR_OK);
        hb_force_fault(0u);
    } else {
        bootdiag_set(BOOT_STAGE_EX2_OLED_INIT_FAIL, err);
        hb_force_fault(1u);
    }

    g_app.adc_init_err = adc1_init();
    g_app.adc_last_err = g_app.adc_init_err;
    ui_update_debug_adc_sample();

    g_app.next_ui_ms = now;
    g_app.next_debug_adc_ms = now;
    g_app.ui_dirty = true;
}

void app_poll_button(void)
{
    key_event_t evt;
    uint32_t now = bsp_millis();
    bool changed = false;

    hb_kick();
    keys_poll();

    while (keys_get_event(&evt)) {
        if (evt.type == KEY_EVT_DOWN) {
            handle_key_short(evt.key);
            beep_once(20u);
            changed = true;
        } else if (evt.type == KEY_EVT_LONG) {
            handle_key_long(evt.key);
            beep_once(60u);
            changed = true;
        }
    }

    if (changed) {
        g_app.ui_dirty = true;
        g_app.next_ui_ms = now;
    }
}

void app_measure_tick(void)
{
    if ((g_app.menu_level != MENU_L4_RES_RUN) || (!g_app.meas_run_enabled)) {
        return;
    }
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();
    app_ui_frame_t frame;
    app_err_t err;

    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        if ((int32_t)(now - g_app.next_debug_adc_ms) >= 0) {
            ui_update_debug_adc_sample();
            g_app.ui_dirty = true;
            g_app.next_debug_adc_ms = now + DEBUG_ADC_REFRESH_MS;
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
        bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, app_ui_presenter_last_err());
        hb_force_fault(1u);
        return;
    }

    build_current_frame(&frame);
    err = app_ui_presenter_flush(&frame);
    if (err != ERR_OK) {
        g_app.presenter_err = err;
        bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, err);
        hb_force_fault(1u);
        return;
    }

    g_app.ui_dirty = false;
    hb_force_fault(0u);
    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        hb_set_load_hint(200u);
    } else {
        hb_set_load_hint(400u);
    }
}

void app_beep_tick(void)
{
    beep_tick();
}

boot_stage_t bootdiag_get_stage(void)
{
    return g_bootdiag_stage;
}

int bootdiag_get_err(void)
{
    return g_bootdiag_err;
}

uint32_t bootdiag_get_ms(void)
{
    return g_bootdiag_ms;
}
