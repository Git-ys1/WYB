#include "app.h"

#include <stdbool.h>
#include <string.h>

#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"
#include "app_bootdiag.h"
#include "app_display_service.h"
#include "app_menu_tree.h"
#include "app_ui_presenter.h"

#define UI_REFRESH_MS 200u
#define DEBUG_ADC_REFRESH_MS 250u
#define MEAS_PERIOD_MS 40u
#define BOOT_DIAG_STABLE_MS 3000u
#define BEEP_FREQ_HZ 2700u

typedef struct {
    menu_state_t menu;
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

static void auto_unlock_menu_if_due(uint32_t now)
{
    if (app_menu_is_unlocked(&g_app.menu)) {
        return;
    }

    if (!app_display_ready()) {
        return;
    }

    if (bootdiag_get_stage() != BOOT_RUN) {
        return;
    }

    if ((now - g_app.run_stage_ms) >= BOOT_DIAG_STABLE_MS) {
        app_menu_set_unlocked(&g_app.menu, true);
        g_app.ui_dirty = true;
    }
}

void app_init(void)
{
    app_err_t err;
    uint32_t now = bsp_millis();

    memset(&g_app, 0, sizeof(g_app));

    bsp_keys_init();
    beep_init(BEEP_FREQ_HZ);
    app_menu_init(&g_app.menu);

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
            app_menu_handle_key(&g_app.menu, evt.key);
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

    if (!app_menu_is_run_page(&g_app.menu)) {
        return;
    }
    if (!g_app.meas_run_enabled) {
        return;
    }
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();
    app_ui_frame_t frame;
    app_runtime_data_t rt;
    app_err_t err;
    uint32_t elapsed_ms;

    app_display_poll();
    auto_unlock_menu_if_due(now);

    if (app_menu_allows_adc_updates(&g_app.menu) &&
        ((int32_t)(now - g_app.next_debug_adc_ms) >= 0)) {
        ui_update_debug_adc_sample();
        g_app.ui_dirty = true;
        g_app.next_debug_adc_ms = now + DEBUG_ADC_REFRESH_MS;
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

    elapsed_ms = now - g_app.run_stage_ms;
    memset(&rt, 0, sizeof(rt));
    rt.raw_u16 = g_app.raw_u16;
    rt.raw_valid = g_app.raw_valid;
    rt.mv = g_app.mv;
    rt.mv_valid = g_app.mv_valid;
    rt.vdda_mv = g_app.vdda_mv;
    rt.vdda_valid = g_app.vdda_valid;
    rt.adc_stat = g_app.adc_last_err;
    rt.stage = (uint8_t)bootdiag_get_stage();
    rt.fault = bootdiag_get_fault();
    rt.display_ready = app_display_ready();
    rt.menu_enabled = app_menu_is_unlocked(&g_app.menu);
    if (elapsed_ms >= BOOT_DIAG_STABLE_MS) {
        rt.menu_wait_sec = 0u;
    } else {
        rt.menu_wait_sec = (BOOT_DIAG_STABLE_MS - elapsed_ms) / 1000u;
    }

    app_menu_build_frame(&g_app.menu, &rt, &frame);

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
