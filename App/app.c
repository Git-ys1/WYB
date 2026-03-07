#include "app.h"
#include "app_heartbeat.h"

/* T-1.1.5D-R0 rollback note:
 * Display path is temporarily owned by main.c OLED smoke entry.
 * app.c is kept for later staged reintegration and is not active when APP_SMOKE_OLED_TEST=1.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "../BSP/bsp.h"
#include "../BSP/bsp_keys.h"
#include "../Calib/calib.h"
#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_beep.h"
#include "../Drivers/drv_freq_ic.h"
#include "../Drivers/drv_mux4051.h"
#include "../Drivers/drv_oled_ssd1306.h"
#include "../Measurements/measurements.h"

#define FW_VERSION "T1.1.4I-20260307"
#define FW_VERSION_SHORT "T1.1.4I"

#define OLED_ADDR_3C 0x3Cu
#define OLED_ADDR_3D 0x3Du

#define I2C_SCAN_SAVE_MAX 8u
#define LOG_VIEW_LINES 1u
#define LOG_FIRST_LINE 7u

#define VREF_RES_V 3.300f
#define RES_RREF_2K_OHM 10000.0f
#define RES_RREF_20K_OHM 100000.0f
#define RES_RREF_200K_OHM 1000000.0f

#define MEAS_PERIOD_MS 40u
#define UI_DEBUG_PERIOD_MS 200u
#define UI_INTERACT_GRACE_MS 60u
#define UI_RUN_REFRESH_MS 80u
#define TIM2_HEALTH_PERIOD_MS 1000u
#define OLED_FLUSH_FAIL_RECOVER_COUNT 3u
#define OLED_RECOVER_FAST_WINDOW_MS 10000u
#define OLED_RECOVER_FAST_PERIOD_MS 200u
#define OLED_RECOVER_SLOW_PERIOD_MS 1000u
#define OLED_FAULT_FAIL_LIMIT 5u
#define HEARTBEAT_PERIOD_MS 400u
#define BOOT_BANNER_HOLD_MS 2000u
#define OLED_LINE_COUNT 8u
#define OLED_LINE_TEXT_MAX 21u

#ifndef APP_BOOT_SAFE_MODE
#define APP_BOOT_SAFE_MODE 0
#endif

#ifndef APP_OLED_RESCUE_MODE
#define APP_OLED_RESCUE_MODE 1
#endif

#if APP_BOOT_SAFE_MODE
#define APP_STAGE_DEFAULT 0
#else
#define APP_STAGE_DEFAULT 1
#endif

#ifndef APP_STAGE_ENABLE_OLED
#define APP_STAGE_ENABLE_OLED APP_STAGE_DEFAULT
#endif
#ifndef APP_STAGE_ENABLE_I2C_SCAN
#define APP_STAGE_ENABLE_I2C_SCAN APP_STAGE_DEFAULT
#endif
#ifndef APP_STAGE_ENABLE_ADC1
#define APP_STAGE_ENABLE_ADC1 APP_STAGE_DEFAULT
#endif
#ifndef APP_STAGE_ENABLE_RES_RUN
#define APP_STAGE_ENABLE_RES_RUN APP_STAGE_DEFAULT
#endif

typedef enum {
    UI_MOD_DEBUG = 0,
    UI_MOD_MEAS
} ui_module_t;

typedef enum {
    MENU_L1_MODULE = 1,
    MENU_L2_DEBUG_PAGE = 2,
    MENU_L2_MEAS_FUNC = 3,
    MENU_L3_RES_RANGE = 4,
    MENU_L4_RES_READY = 5
} menu_level_t;

typedef enum {
    MEAS_FUNC_RES = 0,
    MEAS_FUNC_COUNT
} meas_func_t;

typedef enum {
    RES_SEL_AUTO = 0,
    RES_SEL_200,
    RES_SEL_2K,
    RES_SEL_20K,
    RES_SEL_200K,
    RES_SEL_COUNT
} res_sel_t;

typedef enum {
    MOD_I2C2 = 0,
    MOD_OLED,
    MOD_ADC1,
    MOD_TIM2IC,
    MOD_KEY,
    MOD_BEEP,
    MOD_MUX,
    MOD_COUNT
} module_id_t;

typedef enum {
    MOD_STATE_UNKNOWN = 0,
    MOD_STATE_OK,
    MOD_STATE_FAIL
} module_state_t;

typedef struct {
    module_state_t state;
    app_err_t last_err;
} module_status_t;

typedef struct {
    uint8_t addrs[I2C_SCAN_SAVE_MAX];
    uint8_t saved_count;
    uint8_t total_count;
    bool has_3c;
    bool has_3d;
    bool has_selected;
    uint8_t selected_addr;
} i2c2_scan_result_t;

typedef enum {
    OLED_BOOT_IDLE = 0,
    OLED_BOOT_HW_PROBE,
    OLED_BOOT_HW_INIT,
    OLED_BOOT_HW_FLUSH,
    OLED_BOOT_SW_PROBE,
    OLED_BOOT_SW_INIT,
    OLED_BOOT_SW_FLUSH,
    OLED_BOOT_DONE,
    OLED_BOOT_FAIL_WAIT
} oled_boot_state_t;

typedef struct {
    calib_profile_t calib;
    oled_t oled;
    bool oled_ready;

    module_status_t modules[MOD_COUNT];
    i2c2_scan_result_t i2c2_scan;

    ui_module_t module_sel;
    menu_level_t menu_level;
    uint8_t meas_func_idx;
    res_sel_t res_sel;
    uint8_t debug_text_page_idx;
    bool meas_run_enabled;

    res_manual_result_t res_result;
    app_err_t last_res_err;
    bool res_not_ready_logged;
    uint16_t adc_raw_u16;
    uint32_t adc_mv;
    app_err_t adc_status;

    uint8_t heartbeat_idx;
    bool freq_capture_started;
    uint32_t next_meas_ms;
    uint32_t next_ui_ms;
    uint32_t next_run_ui_ms;
    uint32_t next_tim2_check_ms;
    uint32_t next_oled_retry_ms;
    uint32_t next_heartbeat_ms;
    uint32_t boot_ms;
    uint32_t oled_boot_ok_ms;
    bool ui_dirty;
    bool ui_cache_valid;
    uint8_t oled_flush_fail_count;
    uint8_t oled_recover_fail_count;
    oled_boot_state_t oled_boot_state;
    char ui_line_cache[OLED_LINE_COUNT][OLED_LINE_TEXT_MAX + 1u];
} app_ctx_t;

static app_ctx_t g_app;
static volatile boot_stage_t g_bootdiag_stage = BOOT_STAGE_10_GPIO_OK;
static volatile int g_bootdiag_err = 0;
static volatile uint32_t g_bootdiag_ms = 0u;

static const char *k_module_name[MOD_COUNT] = {
    "I2C2", "OLED", "ADC1", "TIM2IC", "KEY", "BEEP", "MUX"
};

static const char *k_res_sel_name[RES_SEL_COUNT] = {
    "AUTO", "200", "2K", "20K", "200K"
};

static const char *k_res_channel_name[RES_SEL_COUNT] = {
    "--", "CH0", "CH1", "CH2", "CH3"
};

static const char *k_meas_func_name[MEAS_FUNC_COUNT] = {
    "RES"
};

static const char k_heartbeat_frames[4] = {'|', '/', '-', '\\'};

static void bootdiag_set(boot_stage_t stage, int err)
{
    g_bootdiag_stage = stage;
    g_bootdiag_err = err;
    g_bootdiag_ms = bsp_millis();
}

static void freq_capture_mark_started(void)
{
    if (g_app.freq_capture_started) {
        return;
    }

    bsp_freq_capture_start();
    g_app.freq_capture_started = true;
}

static void ui_cache_reset(void)
{
    memset(g_app.ui_line_cache, 0, sizeof(g_app.ui_line_cache));
    g_app.ui_cache_valid = false;
    oled_invalidate_all();
}

static void ui_set_line(uint8_t line, const char *text)
{
    char clipped[OLED_LINE_TEXT_MAX + 1u];
    uint8_t i = 0u;

    if (line >= OLED_LINE_COUNT) {
        return;
    }

    if (text != NULL) {
        while ((text[i] != '\0') && (i < OLED_LINE_TEXT_MAX)) {
            clipped[i] = text[i];
            i++;
        }
    }
    clipped[i] = '\0';

    if (g_app.ui_cache_valid && (strcmp(g_app.ui_line_cache[line], clipped) == 0)) {
        return;
    }

    oled_clear_line(line);
    if (clipped[0] != '\0') {
        oled_draw_text_line(line, clipped);
    }
    (void)strncpy(g_app.ui_line_cache[line], clipped, OLED_LINE_TEXT_MAX);
    g_app.ui_line_cache[line][OLED_LINE_TEXT_MAX] = '\0';
}

static void module_set(module_id_t module, module_state_t state, app_err_t err)
{
    module_status_t *slot;

    if (module >= MOD_COUNT) {
        return;
    }

    slot = &g_app.modules[module];
    if ((slot->state == state) && (slot->last_err == err)) {
        return;
    }

    slot->state = state;
    slot->last_err = err;
    g_app.ui_dirty = true;

    if (state == MOD_STATE_OK) {
        LOGI("%s OK", k_module_name[module]);
    } else if (state == MOD_STATE_FAIL) {
        LOGE("%s E%d", k_module_name[module], (int)err);
    }
}

static bool res_sel_is_manual(res_sel_t sel)
{
    return (sel == RES_SEL_2K) || (sel == RES_SEL_20K) || (sel == RES_SEL_200K);
}

static res_range_t res_sel_to_range(res_sel_t sel)
{
    if (sel == RES_SEL_2K) {
        return RES_RANGE_2K;
    }
    if (sel == RES_SEL_20K) {
        return RES_RANGE_20K;
    }
    if (sel == RES_SEL_200K) {
        return RES_RANGE_200K;
    }
    return RES_RANGE_200;
}

static float res_sel_to_rref_ohm(res_sel_t sel)
{
    if (sel == RES_SEL_2K) {
        return RES_RREF_2K_OHM;
    }
    if (sel == RES_SEL_20K) {
        return RES_RREF_20K_OHM;
    }
    if (sel == RES_SEL_200K) {
        return RES_RREF_200K_OHM;
    }
    return 0.0f;
}

static void res_sel_step(int dir)
{
    int16_t next = (int16_t)g_app.res_sel + (int16_t)dir;

    if (next < 0) {
        next = (int16_t)RES_SEL_COUNT - 1;
    } else if (next >= (int16_t)RES_SEL_COUNT) {
        next = 0;
    }

    if (g_app.res_sel != (res_sel_t)next) {
        g_app.res_sel = (res_sel_t)next;
        g_app.res_not_ready_logged = false;
        LOGI("RES RNG %s", k_res_sel_name[g_app.res_sel]);
    }
}

static app_err_t app_oled_flush_ui(void)
{
#if APP_OLED_RESCUE_MODE
    return oled_flush(&g_app.oled);
#else
    return oled_flush_dirty(&g_app.oled);
#endif
}

static bool oled_scan_i2c2_pair(void)
{
    uint8_t i;
    static const uint8_t k_oled_probe_addr[] = {OLED_ADDR_3C, OLED_ADDR_3D};

    memset(&g_app.i2c2_scan, 0, sizeof(g_app.i2c2_scan));

#if !APP_STAGE_ENABLE_I2C_SCAN
    g_app.i2c2_scan.has_selected = true;
    g_app.i2c2_scan.selected_addr = OLED_ADDR_3C;
    module_set(MOD_I2C2, MOD_STATE_UNKNOWN, ERR_NOT_IMPL);
    return true;
#else
    for (i = 0u; i < (sizeof(k_oled_probe_addr) / sizeof(k_oled_probe_addr[0])); i++) {
        uint8_t addr = k_oled_probe_addr[i];
        if (!bsp_i2c_probe(BSP_I2C_BUS_OLED, addr, 20u)) {
            continue;
        }

        if (g_app.i2c2_scan.total_count < 255u) {
            g_app.i2c2_scan.total_count++;
        }
        if (g_app.i2c2_scan.saved_count < I2C_SCAN_SAVE_MAX) {
            g_app.i2c2_scan.addrs[g_app.i2c2_scan.saved_count++] = addr;
        }

        if (addr == OLED_ADDR_3C) {
            g_app.i2c2_scan.has_3c = true;
        } else if (addr == OLED_ADDR_3D) {
            g_app.i2c2_scan.has_3d = true;
        }
    }

    if (g_app.i2c2_scan.has_3c) {
        g_app.i2c2_scan.has_selected = true;
        g_app.i2c2_scan.selected_addr = OLED_ADDR_3C;
    } else if (g_app.i2c2_scan.has_3d) {
        g_app.i2c2_scan.has_selected = true;
        g_app.i2c2_scan.selected_addr = OLED_ADDR_3D;
    } else if (g_app.i2c2_scan.saved_count > 0u) {
        g_app.i2c2_scan.has_selected = true;
        g_app.i2c2_scan.selected_addr = g_app.i2c2_scan.addrs[0];
    }

    return g_app.i2c2_scan.has_selected;
#endif
}

static uint32_t oled_retry_period_ms(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - g_app.boot_ms;

    if (elapsed < OLED_RECOVER_FAST_WINDOW_MS) {
        return OLED_RECOVER_FAST_PERIOD_MS;
    }
    return OLED_RECOVER_SLOW_PERIOD_MS;
}

static void oled_schedule_retry(uint32_t now_ms)
{
    g_app.next_oled_retry_ms = now_ms + oled_retry_period_ms(now_ms);
}

static app_err_t oled_show_boot_banner(void)
{
    app_err_t err;

    oled_clear();
    oled_draw_text_line(0u, "BOOT OK");
    oled_draw_text_line(1u, FW_VERSION_SHORT);
    err = oled_flush(&g_app.oled);
    if (err == ERR_OK) {
        g_app.oled_ready = true;
        g_app.oled_flush_fail_count = 0u;
        g_app.ui_cache_valid = false;
        g_app.oled_boot_ok_ms = bsp_millis();
        bootdiag_set(BOOT_STAGE_40_OLED_FLUSH_OK, ERR_OK);
        freq_capture_mark_started();
        module_set(MOD_OLED, MOD_STATE_OK, ERR_OK);
    } else {
        g_app.oled_ready = false;
        bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, err);
        module_set(MOD_OLED, MOD_STATE_FAIL, err);
    }

    return err;
}

static bool oled_try_init_pair(app_err_t *last_err)
{
    uint8_t addr_try[2];
    uint8_t addr_count = 0u;
    uint8_t i;
    app_err_t err = ERR_I2C_NACK;

    if (g_app.i2c2_scan.has_selected) {
        addr_try[addr_count++] = g_app.i2c2_scan.selected_addr;
    }
    if ((addr_count == 0u) || (addr_try[0] != OLED_ADDR_3C)) {
        addr_try[addr_count++] = OLED_ADDR_3C;
    }
    if ((addr_count == 1u) && (addr_try[0] != OLED_ADDR_3D)) {
        addr_try[addr_count++] = OLED_ADDR_3D;
    }

    for (i = 0u; i < addr_count; i++) {
        err = oled_init(&g_app.oled, BSP_I2C_BUS_OLED, addr_try[i]);
        if (err != ERR_OK) {
            continue;
        }

        g_app.i2c2_scan.has_selected = true;
        g_app.i2c2_scan.selected_addr = addr_try[i];
        g_app.oled_ready = true;
        g_app.oled_flush_fail_count = 0u;
        ui_cache_reset();
        g_app.ui_dirty = true;
        g_app.next_ui_ms = bsp_millis();
        bootdiag_set(BOOT_STAGE_30_OLED_INIT_OK, ERR_OK);
        module_set(MOD_OLED, MOD_STATE_OK, ERR_OK);
        LOGI("OLED@%02X OK", addr_try[i]);
        if (last_err != NULL) {
            *last_err = ERR_OK;
        }
        return true;
    }

    if (last_err != NULL) {
        *last_err = err;
    }
    return false;
}

static void oled_enter_fail_wait(uint32_t now_ms, app_err_t err)
{
    if (g_app.oled_recover_fail_count < 255u) {
        g_app.oled_recover_fail_count++;
    }

    g_app.oled_ready = false;
    g_app.oled_boot_state = OLED_BOOT_FAIL_WAIT;
    bootdiag_set(BOOT_STAGE_EX4_OLED_RECOVER_FAIL, err);
    module_set(MOD_OLED, MOD_STATE_FAIL, err);

    if (g_app.oled_recover_fail_count >= OLED_FAULT_FAIL_LIMIT) {
        hb_force_fault(1u);
        g_app.next_oled_retry_ms = now_ms + OLED_RECOVER_SLOW_PERIOD_MS;
        return;
    }

    oled_schedule_retry(now_ms);
}

static void app_oled_recover_tick(uint32_t now_ms, bool force_now)
{
    uint8_t steps = 0u;
    app_err_t err;
    bool found;

    if (!force_now
        && (g_app.oled_boot_state == OLED_BOOT_FAIL_WAIT)
        && ((int32_t)(now_ms - g_app.next_oled_retry_ms) < 0)) {
        return;
    }

    while (steps < 12u) {
        steps++;
        switch (g_app.oled_boot_state) {
            case OLED_BOOT_IDLE:
                g_app.oled_boot_state = OLED_BOOT_HW_PROBE;
                continue;

            case OLED_BOOT_HW_PROBE:
                (void)bsp_oled_bus_set_mode(BSP_OLED_BUS_HW_I2C2);
                found = oled_scan_i2c2_pair();
                if (found) {
                    bootdiag_set(BOOT_STAGE_20_I2C2_PROBE_OK, ERR_OK);
#if APP_STAGE_ENABLE_I2C_SCAN
                    module_set(MOD_I2C2, MOD_STATE_OK, ERR_OK);
#endif
                } else {
                    bootdiag_set(BOOT_STAGE_EX1_I2C2_PROBE_FAIL, ERR_I2C_NACK);
                    module_set(MOD_I2C2, MOD_STATE_FAIL, ERR_I2C_NACK);
                }
                g_app.oled_boot_state = OLED_BOOT_HW_INIT;
                continue;

            case OLED_BOOT_HW_INIT:
                if (oled_try_init_pair(&err)) {
                    g_app.oled_boot_state = OLED_BOOT_HW_FLUSH;
                    continue;
                }
                if (err == ERR_OK) {
                    err = ERR_I2C_NACK;
                }
                bootdiag_set(BOOT_STAGE_EX2_OLED_INIT_FAIL, err);
                module_set(MOD_OLED, MOD_STATE_FAIL, err);
                g_app.oled_boot_state = OLED_BOOT_SW_PROBE;
                continue;

            case OLED_BOOT_HW_FLUSH:
                err = oled_show_boot_banner();
                if (err == ERR_OK) {
                    g_app.oled_boot_state = OLED_BOOT_DONE;
                    g_app.oled_recover_fail_count = 0u;
                    hb_force_fault(0u);
                    return;
                }
                bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, err);
                module_set(MOD_OLED, MOD_STATE_FAIL, err);
                g_app.oled_boot_state = OLED_BOOT_SW_PROBE;
                continue;

            case OLED_BOOT_SW_PROBE:
                (void)bsp_oled_bus_set_mode(BSP_OLED_BUS_SOFT_I2C);
                found = oled_scan_i2c2_pair();
                if (found) {
                    bootdiag_set(BOOT_STAGE_20_I2C2_PROBE_OK, ERR_OK);
#if APP_STAGE_ENABLE_I2C_SCAN
                    module_set(MOD_I2C2, MOD_STATE_OK, ERR_OK);
#endif
                } else {
                    bootdiag_set(BOOT_STAGE_EX1_I2C2_PROBE_FAIL, ERR_I2C_NACK);
                    module_set(MOD_I2C2, MOD_STATE_FAIL, ERR_I2C_NACK);
                }
                g_app.oled_boot_state = OLED_BOOT_SW_INIT;
                continue;

            case OLED_BOOT_SW_INIT:
                if (oled_try_init_pair(&err)) {
                    g_app.oled_boot_state = OLED_BOOT_SW_FLUSH;
                    continue;
                }
                if (err == ERR_OK) {
                    err = ERR_I2C_NACK;
                }
                bootdiag_set(BOOT_STAGE_EX2_OLED_INIT_FAIL, err);
                module_set(MOD_OLED, MOD_STATE_FAIL, err);
                oled_enter_fail_wait(now_ms, err);
                return;

            case OLED_BOOT_SW_FLUSH:
                err = oled_show_boot_banner();
                if (err == ERR_OK) {
                    g_app.oled_boot_state = OLED_BOOT_DONE;
                    g_app.oled_recover_fail_count = 0u;
                    hb_force_fault(0u);
                    return;
                }
                bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, err);
                module_set(MOD_OLED, MOD_STATE_FAIL, err);
                oled_enter_fail_wait(now_ms, err);
                return;

            case OLED_BOOT_FAIL_WAIT:
                if (!force_now && ((int32_t)(now_ms - g_app.next_oled_retry_ms) < 0)) {
                    return;
                }
                (void)bsp_i2c2_bus_recover();
                (void)bsp_i2c2_reinit_100k();
                (void)bsp_oled_bus_set_mode(BSP_OLED_BUS_HW_I2C2);
                g_app.oled_boot_state = OLED_BOOT_HW_PROBE;
                continue;

            case OLED_BOOT_DONE:
            default:
                return;
        }
    }
}

static void oled_mark_flush_result(app_err_t err)
{
    if (err == ERR_OK) {
        g_app.oled_ready = true;
        g_app.oled_flush_fail_count = 0u;
        module_set(MOD_OLED, MOD_STATE_OK, ERR_OK);
        return;
    }

    bootdiag_set(BOOT_STAGE_EX3_OLED_FLUSH_FAIL, err);
    if (g_app.oled_flush_fail_count < 255u) {
        g_app.oled_flush_fail_count++;
    }

    if (g_app.oled_flush_fail_count >= OLED_FLUSH_FAIL_RECOVER_COUNT) {
        oled_invalidate_all();
        g_app.ui_cache_valid = false;
        g_app.ui_dirty = true;
        g_app.oled_flush_fail_count = 0u;
        g_app.oled_ready = false;
        g_app.oled_boot_state = OLED_BOOT_FAIL_WAIT;
        oled_schedule_retry(bsp_millis());
        LOGW("OLED FORCE FULL");
    } else {
        g_app.oled_ready = true;
    }

    module_set(MOD_OLED, MOD_STATE_FAIL, err);
}

static void app_oled_boot_minimal(void)
{
#if APP_STAGE_ENABLE_OLED
    g_app.oled_ready = false;
    g_app.oled_boot_state = OLED_BOOT_HW_PROBE;
    g_app.oled_recover_fail_count = 0u;
    g_app.oled_flush_fail_count = 0u;
    g_app.oled_boot_ok_ms = 0u;
    g_app.next_oled_retry_ms = bsp_millis();
    app_oled_recover_tick(g_app.next_oled_retry_ms, true);
#else
    g_app.oled_ready = false;
    g_app.oled_boot_state = OLED_BOOT_IDLE;
    module_set(MOD_OLED, MOD_STATE_UNKNOWN, ERR_NOT_IMPL);
    LOGW("SAFE: OLED off");
#endif
}

static void run_boot_selftest(void)
{
#if APP_STAGE_ENABLE_ADC1
    app_err_t err;
    uint32_t mv;
    uint16_t raw;
#endif

#if APP_STAGE_ENABLE_ADC1
    err = adc1_init();
    if (err != ERR_OK) {
        module_set(MOD_ADC1, MOD_STATE_FAIL, err);
        LOGE("ADC1 INIT E%d", (int)err);
    } else {
        err = adc1_read_filtered(&raw, &mv);
        g_app.adc_raw_u16 = raw;
        g_app.adc_mv = mv;
        g_app.adc_status = adc1_read_status();
        if (err == ERR_OK) {
            module_set(MOD_ADC1, MOD_STATE_OK, ERR_OK);
            LOGI("ADC1 RAW %u", (unsigned int)raw);
        } else {
            module_set(MOD_ADC1, MOD_STATE_FAIL, err);
            LOGE("ADC1 RD E%d", (int)err);
        }
    }
#else
    g_app.adc_status = ERR_NOT_IMPL;
    module_set(MOD_ADC1, MOD_STATE_UNKNOWN, ERR_NOT_IMPL);
    LOGW("SAFE: ADC1 off");
#endif

    module_set(MOD_MUX, MOD_STATE_OK, ERR_OK);
    module_set(MOD_KEY, MOD_STATE_OK, ERR_OK);
    module_set(MOD_BEEP, MOD_STATE_OK, ERR_OK);
    module_set(MOD_TIM2IC, MOD_STATE_UNKNOWN, ERR_NO_SIGNAL);
}

static void update_tim2_health(void)
{
    float hz;
    app_err_t err = freq_get_hz(&hz);

    if (!g_app.freq_capture_started) {
        module_set(MOD_TIM2IC, MOD_STATE_UNKNOWN, ERR_NO_SIGNAL);
        return;
    }

    if (err == ERR_OK) {
        module_set(MOD_TIM2IC, MOD_STATE_OK, ERR_OK);
    } else {
        module_set(MOD_TIM2IC, MOD_STATE_FAIL, ERR_NO_SIGNAL);
    }
}

static void draw_debug_page(uint32_t now_ms)
{
    char line[32];
    app_err_t err;

    if ((int32_t)(now_ms - g_app.next_heartbeat_ms) >= 0) {
        g_app.heartbeat_idx = (uint8_t)((g_app.heartbeat_idx + 1u) % 4u);
        g_app.next_heartbeat_ms = now_ms + HEARTBEAT_PERIOD_MS;
    }

    if (g_app.debug_text_page_idx == 0u) {
        ui_set_line(0u, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        ui_set_line(1u, "0123456789");
        ui_set_line(2u, "RAW MV VDDA STAT");
        ui_set_line(3u, "VWXYZ");
        (void)snprintf(line, sizeof(line), "PAGE:0 %c", k_heartbeat_frames[g_app.heartbeat_idx]);
        ui_set_line(4u, line);
        ui_set_line(5u, "UP/DN: NEXT PAGE");
        ui_set_line(6u, "OK/BACK: MENU");
        ui_set_line(7u, "");
    } else {
        ui_set_line(0u, "OLED TXT OK");
        ui_set_line(1u, "RAW:1234");
        ui_set_line(2u, "MV :3.300");
        ui_set_line(3u, "VDDA:3334");
        ui_set_line(4u, "STAT:OK");
        (void)snprintf(line, sizeof(line), "PAGE:1 %c", k_heartbeat_frames[g_app.heartbeat_idx]);
        ui_set_line(5u, line);
        ui_set_line(6u, "UP/DN: PREV PAGE");
        ui_set_line(7u, "");
    }

    err = app_oled_flush_ui();
    if (err == ERR_OK) {
        g_app.ui_cache_valid = true;
    }
    oled_mark_flush_result(err);
}

static void draw_menu_l1_module(void)
{
    app_err_t err;

    ui_set_line(0u, "MAIN MENU");
    ui_set_line(1u, (g_app.module_sel == UI_MOD_DEBUG) ? "> DEBUG" : "  DEBUG");
    ui_set_line(2u, (g_app.module_sel == UI_MOD_MEAS) ? "> MEAS" : "  MEAS");
    ui_set_line(3u, "");
    ui_set_line(4u, "L/R or U/D: SEL");
    ui_set_line(5u, "OK: ENTER");
    ui_set_line(6u, "BACK: STAY");
    ui_set_line(7u, "");
    err = app_oled_flush_ui();
    if (err == ERR_OK) {
        g_app.ui_cache_valid = true;
    }
    oled_mark_flush_result(err);
}

static void draw_menu_l2_meas(void)
{
    char line[24];
    app_err_t err;

    ui_set_line(0u, "MEAS MENU");
    (void)snprintf(line, sizeof(line), "> %s", k_meas_func_name[g_app.meas_func_idx]);
    ui_set_line(1u, line);
    ui_set_line(2u, "");
    ui_set_line(3u, "OK: RANGE");
    ui_set_line(4u, "BACK: MAIN");
    ui_set_line(5u, "");
    ui_set_line(6u, "LONG OK: MAIN");
    ui_set_line(7u, "");
    err = app_oled_flush_ui();
    if (err == ERR_OK) {
        g_app.ui_cache_valid = true;
    }
    oled_mark_flush_result(err);
}

static void draw_menu_l3_range(void)
{
    char line[24];
    app_err_t err;

    ui_set_line(0u, "RES RANGE");
    (void)snprintf(line, sizeof(line), "SEL: %s %s", k_res_sel_name[g_app.res_sel], k_res_channel_name[g_app.res_sel]);
    ui_set_line(1u, line);

    if (!res_sel_is_manual(g_app.res_sel)) {
        ui_set_line(2u, "NOT READY");
    } else {
        ui_set_line(2u, "");
    }

    ui_set_line(3u, "UP/DN/LR: CHG");
    ui_set_line(4u, "OK: READY");
    ui_set_line(5u, "BACK: MEAS");
    ui_set_line(6u, "LONG OK: MAIN");
    ui_set_line(7u, "");
    err = app_oled_flush_ui();
    if (err == ERR_OK) {
        g_app.ui_cache_valid = true;
    }
    oled_mark_flush_result(err);
}

static void draw_menu_l4_res_ready(void)
{
    char line[32];
    app_err_t flush_err;

    ui_set_line(0u, "RES READY");
    (void)snprintf(line, sizeof(line), "RANGE: %s", k_res_sel_name[g_app.res_sel]);
    ui_set_line(1u, line);
    ui_set_line(2u, "");
    ui_set_line(3u, "PRESS OK TO RUN");
    ui_set_line(4u, "(RUN DISABLED)");
    ui_set_line(5u, "UP/DN/LR: CHG");
    ui_set_line(6u, "BACK: RANGE");
    ui_set_line(7u, "");

    flush_err = app_oled_flush_ui();
    if (flush_err == ERR_OK) {
        g_app.ui_cache_valid = true;
    }
    oled_mark_flush_result(flush_err);
}

static void draw_menu_page(void)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        draw_menu_l1_module();
    } else if (g_app.menu_level == MENU_L2_MEAS_FUNC) {
        draw_menu_l2_meas();
    } else if (g_app.menu_level == MENU_L3_RES_RANGE) {
        draw_menu_l3_range();
    } else {
        draw_menu_l4_res_ready();
    }
}

static bool menu_is_meas_level(void)
{
    return (g_app.menu_level == MENU_L2_MEAS_FUNC)
        || (g_app.menu_level == MENU_L3_RES_RANGE)
        || (g_app.menu_level == MENU_L4_RES_READY);
}

static void menu_enter(void)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        if (g_app.module_sel == UI_MOD_DEBUG) {
            g_app.menu_level = MENU_L2_DEBUG_PAGE;
            g_app.debug_text_page_idx = 0u;
            LOGI("MENU DEBUG");
        } else {
            g_app.menu_level = MENU_L2_MEAS_FUNC;
            LOGI("MENU MEAS");
        }
        return;
    }

    if (g_app.menu_level == MENU_L2_MEAS_FUNC) {
        g_app.menu_level = MENU_L3_RES_RANGE;
        LOGI("RES RANGE");
        return;
    }

    if (g_app.menu_level == MENU_L3_RES_RANGE) {
        g_app.menu_level = MENU_L4_RES_READY;
        g_app.res_not_ready_logged = false;
        g_app.meas_run_enabled = false;
        LOGI("RES READY");
        return;
    }

    if (g_app.menu_level == MENU_L4_RES_READY) {
        LOGI("RES RUN blocked");
    }
}

static void menu_back(void)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        return;
    }

    if ((g_app.menu_level == MENU_L2_DEBUG_PAGE) || (g_app.menu_level == MENU_L2_MEAS_FUNC)) {
        g_app.menu_level = MENU_L1_MODULE;
        g_app.meas_run_enabled = false;
        LOGI("MENU MAIN");
        return;
    }

    if (g_app.menu_level == MENU_L3_RES_RANGE) {
        g_app.menu_level = MENU_L2_MEAS_FUNC;
        g_app.meas_run_enabled = false;
        LOGI("MENU MEAS");
        return;
    }

    if (g_app.menu_level == MENU_L4_RES_READY) {
        g_app.menu_level = MENU_L3_RES_RANGE;
        LOGI("RES RANGE");
        g_app.meas_run_enabled = false;
    }
}

static void menu_move_vertical(int dir)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        g_app.module_sel = (g_app.module_sel == UI_MOD_DEBUG) ? UI_MOD_MEAS : UI_MOD_DEBUG;
        return;
    }

    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        if (dir > 0) {
            g_app.debug_text_page_idx = (uint8_t)((g_app.debug_text_page_idx + 1u) & 0x01u);
        } else {
            g_app.debug_text_page_idx = (g_app.debug_text_page_idx == 0u) ? 1u : 0u;
        }
        return;
    }

    if ((g_app.menu_level == MENU_L3_RES_RANGE) || (g_app.menu_level == MENU_L4_RES_READY)) {
        res_sel_step(dir);
    }
}

static void menu_move_horizontal(int dir)
{
    if (g_app.menu_level == MENU_L1_MODULE) {
        g_app.module_sel = (dir < 0) ? UI_MOD_DEBUG : UI_MOD_MEAS;
        return;
    }

    if ((g_app.menu_level == MENU_L3_RES_RANGE) || (g_app.menu_level == MENU_L4_RES_READY)) {
        res_sel_step(dir);
    }
}

static void handle_key_long(key_id_t key)
{
    if ((key == KEY_OK) && (g_app.module_sel == UI_MOD_MEAS) && menu_is_meas_level()) {
        g_app.menu_level = MENU_L1_MODULE;
        g_app.meas_run_enabled = false;
        LOGI("LONG OK -> MAIN");
        beep_once(80u);
    }
}

static void handle_key_short(key_id_t key)
{
    if (key == KEY_UP) {
        menu_move_vertical(-1);
        beep_once(25u);
    } else if (key == KEY_DOWN) {
        menu_move_vertical(1);
        beep_once(25u);
    } else if (key == KEY_LEFT) {
        menu_move_horizontal(-1);
        beep_once(25u);
    } else if (key == KEY_RIGHT) {
        menu_move_horizontal(1);
        beep_once(25u);
    } else if (key == KEY_OK) {
        menu_enter();
        beep_once(40u);
    } else if (key == KEY_BACK) {
        menu_back();
        beep_once(30u);
    }
}

void app_init(void)
{
    uint8_t i;
    app_err_t err;
    uint32_t now;

    memset(&g_app, 0, sizeof(g_app));
    bootdiag_set(BOOT_STAGE_10_GPIO_OK, ERR_OK);
    hb_init();
    ui_cache_reset();

    app_log_init();
    LOGI("BOOT OK %.14s", FW_VERSION);
#if APP_BOOT_SAFE_MODE
    LOGW("SAFE MODE ON");
#else
    LOGI("SAFE MODE OFF");
#endif

    g_app.module_sel = UI_MOD_DEBUG;
    g_app.menu_level = MENU_L1_MODULE;
    g_app.meas_func_idx = MEAS_FUNC_RES;
    g_app.res_sel = RES_SEL_2K;
    g_app.debug_text_page_idx = 0u;
    g_app.meas_run_enabled = false;
    g_app.last_res_err = ERR_OK;
    g_app.res_result.err = ERR_NOT_IMPL;
    g_app.adc_raw_u16 = 0u;
    g_app.adc_mv = 0u;
    g_app.adc_status = ERR_NOT_IMPL;
    g_app.ui_dirty = true;
    g_app.ui_cache_valid = false;
    g_app.boot_ms = bsp_millis();

    for (i = 0u; i < MOD_COUNT; i++) {
        g_app.modules[i].state = MOD_STATE_UNKNOWN;
        g_app.modules[i].last_err = ERR_OK;
    }

    bsp_keys_init();
    beep_init(2700u);
    mux_init();
    freq_start();
    bsp_oled_bus_reset_stats();
    (void)bsp_oled_bus_set_mode(BSP_OLED_BUS_HW_I2C2);

    err = calib_load(&g_app.calib);
    if (err != ERR_OK) {
        LOGE("CAL E%d", (int)err);
    } else {
        LOGI("CAL OK");
    }

    measurements_init(&g_app.calib);
    app_oled_boot_minimal();
    run_boot_selftest();
    if (g_app.oled_ready) {
        hb_force_fault(0u);
    } else {
        hb_force_fault(1u);
    }

    now = bsp_millis();
    g_app.next_meas_ms = now;
    g_app.next_ui_ms = now;
    g_app.next_run_ui_ms = now;
    g_app.next_tim2_check_ms = now + TIM2_HEALTH_PERIOD_MS;
    if (!g_app.oled_ready) {
        oled_schedule_retry(now);
    }
    g_app.next_heartbeat_ms = now + HEARTBEAT_PERIOD_MS;
}

void app_poll_button(void)
{
    key_event_t evt;
    bool ui_dirty = false;

    hb_kick();
    keys_poll();
    while (keys_get_event(&evt)) {
        if (evt.key >= KEY_COUNT) {
            continue;
        }

        if (evt.type == KEY_EVT_DOWN) {
            handle_key_short(evt.key);
            ui_dirty = true;
        } else if (evt.type == KEY_EVT_LONG) {
            handle_key_long(evt.key);
            ui_dirty = true;
        }
    }

    if (ui_dirty) {
        uint32_t now = bsp_millis();
        g_app.ui_dirty = true;
        g_app.next_ui_ms = now;
        g_app.next_meas_ms = now + UI_INTERACT_GRACE_MS;
    }
}

void app_measure_tick(void)
{
    uint32_t now = bsp_millis();
#if APP_STAGE_ENABLE_RES_RUN
    uint32_t vdda_mv = 3300u;
    app_err_t err;
    app_err_t vdda_err;
    res_range_t range;
    float rref_ohm;
    float vref_v = VREF_RES_V;
#endif

    if ((int32_t)(now - g_app.next_meas_ms) < 0) {
        return;
    }
    g_app.next_meas_ms = now + MEAS_PERIOD_MS;

#if !APP_STAGE_ENABLE_RES_RUN
    return;
#else
    if ((g_app.module_sel != UI_MOD_MEAS)
        || (g_app.menu_level != MENU_L4_RES_READY)
        || (!g_app.meas_run_enabled)) {
        return;
    }

    hb_set_load_hint(800u);

    if (!res_sel_is_manual(g_app.res_sel)) {
        g_app.res_result.err = ERR_NOT_IMPL;
        g_app.res_result.flags = 0u;
        if (!g_app.res_not_ready_logged) {
            if (g_app.res_sel == RES_SEL_200) {
                LOGW("RES 200 not ready");
            } else {
                LOGW("RES AUTO not ready");
            }
            g_app.res_not_ready_logged = true;
        }
        return;
    }

    g_app.res_not_ready_logged = false;
    range = res_sel_to_range(g_app.res_sel);
    rref_ohm = res_sel_to_rref_ohm(g_app.res_sel);

    vdda_err = adc1_read_vdda_mv(&vdda_mv);
    if ((vdda_err == ERR_OK) && (vdda_mv >= 2500u) && (vdda_mv <= 3600u)) {
        vref_v = (float)vdda_mv / 1000.0f;
    }

    err = meas_resistance_manual(range, rref_ohm, vref_v, &g_app.res_result);
    g_app.adc_raw_u16 = (g_app.res_result.raw >= 0) ? (uint16_t)g_app.res_result.raw : 0u;
    g_app.adc_mv = (uint32_t)(g_app.res_result.vred_v * 1000.0f);
    g_app.adc_status = adc1_read_status();

    if (err != g_app.last_res_err) {
        if (err == ERR_OK) {
            LOGI("RES ADC RECOVER");
        } else {
            LOGE("RES ADC E%d", (int)err);
        }
        g_app.last_res_err = err;
    }

    if ((err == ERR_OK) && (g_app.adc_status == ERR_OK)) {
        module_set(MOD_ADC1, MOD_STATE_OK, ERR_OK);
    } else if (g_app.adc_status == ERR_ADC_TIMEOUT) {
        module_set(MOD_ADC1, MOD_STATE_FAIL, ERR_ADC_TIMEOUT);
    } else if (g_app.adc_status == ERR_HW_FAIL) {
        module_set(MOD_ADC1, MOD_STATE_FAIL, ERR_HW_FAIL);
    } else {
        module_set(MOD_ADC1, MOD_STATE_FAIL, err);
    }

    if ((int32_t)(now - g_app.next_run_ui_ms) >= 0) {
        g_app.ui_dirty = true;
        g_app.next_ui_ms = now;
        g_app.next_run_ui_ms = now + UI_RUN_REFRESH_MS;
    }
#endif
}

void app_ui_tick(void)
{
    uint32_t now = bsp_millis();

    if ((int32_t)(now - g_app.next_tim2_check_ms) >= 0) {
        update_tim2_health();
        g_app.next_tim2_check_ms = now + TIM2_HEALTH_PERIOD_MS;
    }

#if !APP_STAGE_ENABLE_OLED
    hb_set_load_hint(400u);
    (void)now;
    return;
#else
    if (!g_app.oled_ready) {
        app_oled_recover_tick(now, false);
        hb_set_load_hint(200u);
        return;
    }

#if APP_OLED_RESCUE_MODE
    if ((g_app.oled_boot_ok_ms > 0u) && ((int32_t)(now - g_app.oled_boot_ok_ms) < (int32_t)BOOT_BANNER_HOLD_MS)) {
        hb_set_load_hint(200u);
        return;
    }
#endif

    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        if ((int32_t)(now - g_app.next_ui_ms) >= 0) {
            g_app.next_ui_ms = now + UI_DEBUG_PERIOD_MS;
            g_app.ui_dirty = true;
        }
    } else {
        if ((int32_t)(now - g_app.next_ui_ms) < 0) {
            return;
        }
    }

    if (!g_app.ui_dirty) {
        if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
            hb_set_load_hint(200u);
        } else {
            hb_set_load_hint(400u);
        }
        return;
    }

    if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
        draw_debug_page(now);
    } else {
        draw_menu_page();
    }

    if (g_app.oled_ready) {
        g_app.ui_dirty = false;
        g_app.oled_recover_fail_count = 0u;
        hb_force_fault(0u);
        if (g_app.menu_level == MENU_L2_DEBUG_PAGE) {
            hb_set_load_hint(200u);
        } else {
            hb_set_load_hint(400u);
        }
    }
#endif
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
