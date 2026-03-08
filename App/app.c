#include "app.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"
#include "app_bootdiag.h"
#include "app_display_service.h"
#include "app_ui_presenter.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define MEAS_PERIOD_MS 40u
#define BOOT_DIAG_STABLE_MS 10000u
#define BEEP_FREQ_HZ 2700u

typedef enum {
    UI_PAGE_DIAG = 0,
    UI_PAGE_MENU
} ui_page_t;

typedef enum {
    MENU_ITEM_DEBUG = 0,
    MENU_ITEM_MEASURE
} menu_item_t;

typedef struct {
    ui_page_t page;
    menu_item_t menu_sel;
    bool menu_enabled;
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
    uint32_t run_stage_ms;
    uint32_t next_ui_ms;
    uint32_t next_debug_adc_ms;
    uint32_t next_meas_ms;
    bool ui_dirty;
} app_ctx_t;

static app_ctx_t g_app;

static void frame_clear(app_ui_frame_t *frame)
{
    if (frame == NULL) {
        return;
    }
    memset(frame, 0, sizeof(*frame));
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

static void ui_update_debug_adc_sample(void)
{
    app_err_t err;
    uint16_t raw = 0u;
    uint32_t mv = 0u;
    uint32_t vdda_mv = 3300u;

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
    if (err != ERR_OK) {
        g_app.adc_last_err = err;
        return;
    }
    g_app.raw_valid = true;
    g_app.raw_u16 = raw;

    err = adc1_read_mv(&mv);
    if (err != ERR_OK) {
        g_app.adc_last_err = err;
        return;
    }
    g_app.mv_valid = true;
    g_app.mv = mv;

    err = adc1_read_vdda_mv(&vdda_mv);
    if (err == ERR_OK) {
        g_app.vdda_valid = true;
        g_app.vdda_mv = vdda_mv;
        g_app.adc_last_err = ERR_OK;
    } else {
        g_app.vdda_valid = false;
        g_app.vdda_mv = 3300u;
        g_app.adc_last_err = err;
    }
}

static void build_diag_frame(app_ui_frame_t *frame)
{
    uint32_t now = bsp_millis();
    uint32_t elapsed_ms = now - g_app.run_stage_ms;
    uint32_t remain_s = 0u;

    if (elapsed_ms < BOOT_DIAG_STABLE_MS) {
        remain_s = (BOOT_DIAG_STABLE_MS - elapsed_ms) / 1000u;
    }

    frame_clear(frame);
    frame_set_linef(frame, 0u, "BOOT OK");
    frame_set_linef(frame, 1u, "STAGE:%u", (unsigned)bootdiag_get_stage());
    frame_set_linef(frame, 2u, "FAULT:%u", (unsigned)bootdiag_get_fault());

    if (g_app.raw_valid) {
        frame_set_linef(frame, 3u, "RAW:%5u", g_app.raw_u16);
    } else {
        frame_set_linef(frame, 3u, "RAW: ----");
    }

    if (g_app.mv_valid) {
        frame_set_linef(frame, 4u, "MV :%lu.%03lu",
                        (unsigned long)(g_app.mv / 1000u),
                        (unsigned long)(g_app.mv % 1000u));
    } else {
        frame_set_linef(frame, 4u, "MV : ----");
    }

    if (g_app.vdda_valid) {
        frame_set_linef(frame, 5u, "VDDA:%4lu", (unsigned long)g_app.vdda_mv);
    } else {
        frame_set_linef(frame, 5u, "VDDA:----");
    }

    if (g_app.adc_last_err == ERR_OK) {
        frame_set_linef(frame, 6u, "STAT:OK");
    } else {
        frame_set_linef(frame, 6u, "STAT:ERR%d", (int)g_app.adc_last_err);
    }

    if (g_app.menu_enabled) {
        frame_set_linef(frame, 7u, "OK/BACK:MENU");
    } else {
        frame_set_linef(frame, 7u, "MENU IN:%2lus", (unsigned long)remain_s);
    }
}

static void build_menu_frame(app_ui_frame_t *frame)
{
    frame_clear(frame);
    frame_set_linef(frame, 0u, "MAIN MENU");
    frame_set_linef(frame, 1u, "%c DEBUG", (g_app.menu_sel == MENU_ITEM_DEBUG) ? '>' : ' ');
    frame_set_linef(frame, 2u, "%c MEASURE", (g_app.menu_sel == MENU_ITEM_MEASURE) ? '>' : ' ');
    frame_set_linef(frame, 4u, "RUN DISABLED");
    frame_set_linef(frame, 6u, "UP/DN/LR:SEL");
    frame_set_linef(frame, 7u, "OK/BACK:DIAG");
}

static void handle_key_short(key_id_t key)
{
    if (g_app.page == UI_PAGE_DIAG) {
        if (g_app.menu_enabled && ((key == KEY_OK) || (key == KEY_BACK))) {
            g_app.page = UI_PAGE_MENU;
        }
        return;
    }

    if ((key == KEY_UP) || (key == KEY_DOWN) || (key == KEY_LEFT) || (key == KEY_RIGHT)) {
        g_app.menu_sel = (g_app.menu_sel == MENU_ITEM_DEBUG) ? MENU_ITEM_MEASURE : MENU_ITEM_DEBUG;
    } else if ((key == KEY_OK) || (key == KEY_BACK)) {
        g_app.page = UI_PAGE_DIAG;
    }
}

void app_init(void)
{
    app_err_t err;
    uint32_t now = bsp_millis();

    memset(&g_app, 0, sizeof(g_app));

    bsp_keys_init();
    beep_init(BEEP_FREQ_HZ);

    g_app.page = UI_PAGE_DIAG;
    g_app.menu_sel = MENU_ITEM_DEBUG;
    g_app.menu_enabled = false;
    g_app.meas_run_enabled = false;
    g_app.vdda_mv = 3300u;

    g_app.adc_init_err = adc1_init();
    g_app.adc_last_err = g_app.adc_init_err;
    ui_update_debug_adc_sample();

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

    g_app.run_stage_ms = bootdiag_get_ms();
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
        if (evt.type == KEY_EVT_DOWN) {
            handle_key_short(evt.key);
            beep_once(20u);
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
    uint32_t now = bsp_millis();

    if ((int32_t)(now - g_app.next_meas_ms) < 0) {
        return;
    }
    g_app.next_meas_ms = now + MEAS_PERIOD_MS;

    if (!g_app.meas_run_enabled) {
        return;
    }
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();
    app_ui_frame_t frame;
    app_err_t err;

    app_display_poll();

    if ((int32_t)(now - g_app.next_debug_adc_ms) >= 0) {
        ui_update_debug_adc_sample();
        g_app.ui_dirty = true;
        g_app.next_debug_adc_ms = now + DEBUG_ADC_REFRESH_MS;
    }

    if ((!g_app.menu_enabled) && app_display_ready() &&
        (bootdiag_get_stage() == BOOT_RUN) &&
        ((now - g_app.run_stage_ms) >= BOOT_DIAG_STABLE_MS)) {
        g_app.menu_enabled = true;
        g_app.ui_dirty = true;
    }

    if ((int32_t)(now - g_app.next_ui_ms) < 0) {
        return;
    }
    g_app.next_ui_ms = now + UI_REFRESH_MS;

    if (!g_app.ui_dirty) {
        return;
    }

    if (g_app.page == UI_PAGE_MENU) {
        build_menu_frame(&frame);
    } else {
        build_diag_frame(&frame);
    }

    if (!app_ui_presenter_ready()) {
        g_app.presenter_err = app_ui_presenter_last_err();
        bootdiag_set_fault(BOOT_FAULT_DISPLAY_INIT);
        bootdiag_set_stage(BOOT_FAULT);
        g_app.ui_dirty = true;
        return;
    }

    err = app_ui_presenter_flush(&frame);
    if (err != ERR_OK) {
        g_app.presenter_err = err;
        bootdiag_set_fault(BOOT_FAULT_UI_FLUSH);
        bootdiag_set_stage(BOOT_FAULT);
        (void)app_display_render_fault(bootdiag_get_fault(), (uint8_t)bootdiag_get_stage());
        g_app.ui_dirty = true;
        return;
    }

    g_app.ui_dirty = false;
}

void app_beep_tick(void)
{
    beep_tick();
}
