#include "app_menu_tree.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

enum {
    MAIN_ITEM_DEBUG = 0,
    MAIN_ITEM_MEASURE = 1,
    MAIN_ITEM_COUNT = 2
};

enum {
    DEBUG_ITEM_ADC = 0,
    DEBUG_ITEM_BOOT = 1,
    DEBUG_ITEM_COUNT = 2
};

enum {
    MEASURE_ITEM_RES = 0,
    MEASURE_ITEM_VDC,
    MEASURE_ITEM_FREQ,
    MEASURE_ITEM_CONT,
    MEASURE_ITEM_DIODE,
    MEASURE_ITEM_COUNT
};

enum {
    RES_RANGE_AUTO = 0,
    RES_RANGE_200,
    RES_RANGE_2K,
    RES_RANGE_20K,
    RES_RANGE_200K,
    RES_RANGE_COUNT
};

static const char *k_main_name[MAIN_ITEM_COUNT] = {"DEBUG", "MEASURE"};
static const char *k_debug_name[DEBUG_ITEM_COUNT] = {"ADC DEBUG", "BOOT INFO"};
static const char *k_measure_name[MEASURE_ITEM_COUNT] = {"RES", "VDC", "FREQ", "CONT", "DIODE"};
static const char *k_res_range_name[RES_RANGE_COUNT] = {"AUTO", "200", "2K", "20K", "200K"};

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

static uint8_t sel_step(uint8_t cur, int dir, uint8_t count)
{
    int next = (int)cur + dir;

    if (next < 0) {
        next = (int)count - 1;
    } else if (next >= (int)count) {
        next = 0;
    }
    return (uint8_t)next;
}

void app_menu_init(menu_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->page = UI_DIAG;
    state->main_sel = MAIN_ITEM_DEBUG;
    state->debug_sel = DEBUG_ITEM_ADC;
    state->measure_sel = MEASURE_ITEM_RES;
    state->res_range_sel = RES_RANGE_2K;
    state->menu_unlocked = false;
}

void app_menu_set_unlocked(menu_state_t *state, bool unlocked)
{
    if (state == NULL) {
        return;
    }
    state->menu_unlocked = unlocked;
}

bool app_menu_is_unlocked(const menu_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    return state->menu_unlocked;
}

bool app_menu_allows_adc_updates(const menu_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    return (state->page == UI_DIAG) || (state->page == UI_DEBUG_ADC);
}

bool app_menu_is_run_page(const menu_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    return (state->page == UI_RES_RUN);
}

const char *app_menu_res_range_name(uint8_t sel)
{
    if (sel >= RES_RANGE_COUNT) {
        return "UNK";
    }
    return k_res_range_name[sel];
}

void app_menu_handle_key(menu_state_t *state, key_id_t key)
{
    if (state == NULL) {
        return;
    }

    switch (state->page) {
    case UI_DIAG:
        if (key == KEY_OK) {
            state->menu_unlocked = true;
            state->page = UI_MAIN_MENU;
        }
        break;

    case UI_MAIN_MENU:
        if (key == KEY_LEFT) {
            state->main_sel = sel_step(state->main_sel, -1, MAIN_ITEM_COUNT);
        } else if (key == KEY_RIGHT) {
            state->main_sel = sel_step(state->main_sel, 1, MAIN_ITEM_COUNT);
        } else if (key == KEY_OK) {
            state->page = (state->main_sel == MAIN_ITEM_DEBUG) ? UI_DEBUG_MENU : UI_MEASURE_MENU;
        } else if (key == KEY_BACK) {
            state->page = UI_DIAG;
        }
        break;

    case UI_DEBUG_MENU:
        if (key == KEY_LEFT) {
            state->debug_sel = sel_step(state->debug_sel, -1, DEBUG_ITEM_COUNT);
        } else if (key == KEY_RIGHT) {
            state->debug_sel = sel_step(state->debug_sel, 1, DEBUG_ITEM_COUNT);
        } else if (key == KEY_OK) {
            state->page = (state->debug_sel == DEBUG_ITEM_ADC) ? UI_DEBUG_ADC : UI_BOOT_INFO;
        } else if (key == KEY_BACK) {
            state->page = UI_MAIN_MENU;
        }
        break;

    case UI_DEBUG_ADC:
    case UI_BOOT_INFO:
        if (key == KEY_BACK) {
            state->page = UI_DEBUG_MENU;
        }
        break;

    case UI_MEASURE_MENU:
        if (key == KEY_LEFT) {
            state->measure_sel = sel_step(state->measure_sel, -1, MEASURE_ITEM_COUNT);
        } else if (key == KEY_RIGHT) {
            state->measure_sel = sel_step(state->measure_sel, 1, MEASURE_ITEM_COUNT);
        } else if (key == KEY_OK) {
            if (state->measure_sel == MEASURE_ITEM_RES) {
                state->page = UI_RES_RANGE;
            } else if (state->measure_sel == MEASURE_ITEM_VDC) {
                state->page = UI_VDC_READY;
            } else if (state->measure_sel == MEASURE_ITEM_FREQ) {
                state->page = UI_FREQ_READY;
            } else if (state->measure_sel == MEASURE_ITEM_CONT) {
                state->page = UI_CONT_READY;
            } else {
                state->page = UI_DIODE_READY;
            }
        } else if (key == KEY_BACK) {
            state->page = UI_MAIN_MENU;
        }
        break;

    case UI_RES_RANGE:
        if (key == KEY_LEFT) {
            state->res_range_sel = sel_step(state->res_range_sel, -1, RES_RANGE_COUNT);
        } else if (key == KEY_RIGHT) {
            state->res_range_sel = sel_step(state->res_range_sel, 1, RES_RANGE_COUNT);
        } else if (key == KEY_OK) {
            state->page = UI_RES_READY;
        } else if (key == KEY_BACK) {
            state->page = UI_MEASURE_MENU;
        }
        break;

    case UI_RES_READY:
        if (key == KEY_OK) {
            state->page = UI_RES_RUN;
        } else if (key == KEY_BACK) {
            state->page = UI_RES_RANGE;
        }
        break;

    case UI_RES_RUN:
        if (key == KEY_BACK) {
            state->page = UI_RES_RANGE;
        }
        break;

    case UI_VDC_READY:
    case UI_FREQ_READY:
    case UI_CONT_READY:
    case UI_DIODE_READY:
        if (key == KEY_BACK) {
            state->page = UI_MEASURE_MENU;
        }
        break;

    default:
        state->page = UI_DIAG;
        break;
    }
}

static void build_diag_frame(const menu_state_t *state, const app_runtime_data_t *rt, app_ui_frame_t *frame)
{
    (void)state;

    frame_set_linef(frame, 0u, "BOOT DIAG");
    frame_set_linef(frame, 1u, "STAGE:%u", (unsigned)rt->stage);
    frame_set_linef(frame, 2u, "FAULT:%u", (unsigned)rt->fault);
    if (rt->raw_valid) {
        frame_set_linef(frame, 3u, "RAW:%5u", rt->raw_u16);
    } else {
        frame_set_linef(frame, 3u, "RAW: ----");
    }
    if (rt->mv_valid) {
        frame_set_linef(frame, 4u, "MV :%lu.%03lu",
                        (unsigned long)(rt->mv / 1000u),
                        (unsigned long)(rt->mv % 1000u));
    } else {
        frame_set_linef(frame, 4u, "MV : ----");
    }
    if (rt->vdda_valid) {
        frame_set_linef(frame, 5u, "VDDA:%4lu", (unsigned long)rt->vdda_mv);
    } else {
        frame_set_linef(frame, 5u, "VDDA:----");
    }
    if (rt->adc_stat == ERR_OK) {
        frame_set_linef(frame, 6u, "STAT:OK");
    } else {
        frame_set_linef(frame, 6u, "STAT:ERR%d", (int)rt->adc_stat);
    }
    if (rt->menu_enabled) {
        frame_set_linef(frame, 7u, "OK:MENU NOW");
    } else {
        frame_set_linef(frame, 7u, "MENU IN:%2lus", (unsigned long)rt->menu_wait_sec);
    }
}

static void build_main_menu_frame(const menu_state_t *state, app_ui_frame_t *frame)
{
    frame_set_linef(frame, 0u, "MAIN MENU");
    frame_set_linef(frame, 1u, "%c %s", (state->main_sel == MAIN_ITEM_DEBUG) ? '>' : ' ', k_main_name[0]);
    frame_set_linef(frame, 2u, "%c %s", (state->main_sel == MAIN_ITEM_MEASURE) ? '>' : ' ', k_main_name[1]);
    frame_set_linef(frame, 6u, "L/R:SEL");
    frame_set_linef(frame, 7u, "OK:ENTER BACK");
}

static void build_debug_menu_frame(const menu_state_t *state, app_ui_frame_t *frame)
{
    frame_set_linef(frame, 0u, "DEBUG MENU");
    frame_set_linef(frame, 1u, "%c %s", (state->debug_sel == DEBUG_ITEM_ADC) ? '>' : ' ', k_debug_name[0]);
    frame_set_linef(frame, 2u, "%c %s", (state->debug_sel == DEBUG_ITEM_BOOT) ? '>' : ' ', k_debug_name[1]);
    frame_set_linef(frame, 7u, "BACK:MAIN");
}

static void build_debug_adc_frame(const app_runtime_data_t *rt, app_ui_frame_t *frame)
{
    frame_set_linef(frame, 0u, "ADC DEBUG");
    if (rt->raw_valid) {
        frame_set_linef(frame, 1u, "RAW:%5u", rt->raw_u16);
    } else {
        frame_set_linef(frame, 1u, "RAW: ----");
    }
    if (rt->mv_valid) {
        frame_set_linef(frame, 2u, "MV :%lu.%03lu",
                        (unsigned long)(rt->mv / 1000u),
                        (unsigned long)(rt->mv % 1000u));
    } else {
        frame_set_linef(frame, 2u, "MV : ----");
    }
    if (rt->vdda_valid) {
        frame_set_linef(frame, 3u, "VDDA:%4lu", (unsigned long)rt->vdda_mv);
    } else {
        frame_set_linef(frame, 3u, "VDDA:----");
    }
    if (rt->adc_stat == ERR_OK) {
        frame_set_linef(frame, 4u, "STAT:OK");
    } else {
        frame_set_linef(frame, 4u, "STAT:ERR%d", (int)rt->adc_stat);
    }
    frame_set_linef(frame, 7u, "BACK:DEBUG");
}

static void build_boot_info_frame(const app_runtime_data_t *rt, app_ui_frame_t *frame)
{
    frame_set_linef(frame, 0u, "BOOT INFO");
    frame_set_linef(frame, 1u, "STAGE:%u", (unsigned)rt->stage);
    frame_set_linef(frame, 2u, "FAULT:%u", (unsigned)rt->fault);
    frame_set_linef(frame, 3u, "MENU EN:%u", rt->menu_enabled ? 1u : 0u);
    frame_set_linef(frame, 4u, "DISP RDY:%u", rt->display_ready ? 1u : 0u);
    frame_set_linef(frame, 7u, "BACK:DEBUG");
}

static void build_measure_menu_frame(const menu_state_t *state, app_ui_frame_t *frame)
{
    uint8_t i;

    frame_set_linef(frame, 0u, "MEASURE MENU");
    for (i = 0u; i < MEASURE_ITEM_COUNT; i++) {
        frame_set_linef(frame, (uint8_t)(i + 1u), "%c %s",
                        (state->measure_sel == i) ? '>' : ' ',
                        k_measure_name[i]);
    }
    frame_set_linef(frame, 7u, "BACK:MAIN");
}

static void build_res_range_frame(const menu_state_t *state, app_ui_frame_t *frame)
{
    uint8_t i;

    frame_set_linef(frame, 0u, "RES RANGE");
    for (i = 0u; i < RES_RANGE_COUNT; i++) {
        frame_set_linef(frame, (uint8_t)(i + 1u), "%c %s",
                        (state->res_range_sel == i) ? '>' : ' ',
                        k_res_range_name[i]);
    }
    frame_set_linef(frame, 7u, "OK:NEXT BACK");
}

static void build_ready_frame(const menu_state_t *state, app_ui_frame_t *frame, const char *title)
{
    frame_set_linef(frame, 0u, "%s", title);
    if (state->page == UI_RES_READY) {
        frame_set_linef(frame, 1u, "RANGE: %s", app_menu_res_range_name(state->res_range_sel));
    }
    frame_set_linef(frame, 3u, "PRESS OK TO RUN");
    frame_set_linef(frame, 4u, "RUN DISABLED");
    frame_set_linef(frame, 7u, "BACK");
}

static const char *res_stat_text(uint8_t stat)
{
    switch (stat) {
    case 0u:
        return "OK";
    case 1u:
        return "OPEN";
    case 2u:
        return "SHORT";
    case 3u:
        return "OVR";
    default:
        return "ERR";
    }
}

static void build_res_run_value(float ohm, char *out, size_t out_size)
{
    uint32_t k_int;
    uint32_t k_frac;
    uint32_t tenth;

    if ((out == NULL) || (out_size == 0u)) {
        return;
    }

    if (ohm >= 1000.0f) {
        uint32_t whole = (uint32_t)(ohm + 0.5f);
        k_int = whole / 1000u;
        k_frac = whole % 1000u;
        (void)snprintf(out, out_size, "%lu.%03luK", (unsigned long)k_int, (unsigned long)k_frac);
    } else {
        tenth = (uint32_t)(ohm * 10.0f + 0.5f);
        (void)snprintf(out, out_size, "%lu.%lu",
                       (unsigned long)(tenth / 10u),
                       (unsigned long)(tenth % 10u));
    }
}

static void build_res_run_frame(const menu_state_t *state, const app_runtime_data_t *rt, app_ui_frame_t *frame)
{
    char r_text[16] = {0};

    if (rt->res_is_exp) {
        frame_set_linef(frame, 0u, "RES RUN %s EXP", app_menu_res_range_name(state->res_range_sel));
    } else {
        frame_set_linef(frame, 0u, "RES RUN %s", app_menu_res_range_name(state->res_range_sel));
    }

    if (rt->res_valid && (rt->res_stat == 0u)) {
        build_res_run_value(rt->res_ohm, r_text, sizeof(r_text));
        frame_set_linef(frame, 1u, "R: %s", r_text);
    } else {
        frame_set_linef(frame, 1u, "R: ----");
    }

    if (rt->res_valid) {
        frame_set_linef(frame, 2u, "MV: %lu", (unsigned long)rt->res_mv);
        frame_set_linef(frame, 3u, "RAW:%u", (unsigned)rt->res_raw_u16);
    } else {
        frame_set_linef(frame, 2u, "MV: ----");
        frame_set_linef(frame, 3u, "RAW:----");
    }
    frame_set_linef(frame, 4u, "STAT:%s", res_stat_text(rt->res_stat));
    frame_set_linef(frame, 7u, "BACK:RANGE");
}

void app_menu_build_frame(const menu_state_t *state, const app_runtime_data_t *rt, app_ui_frame_t *frame)
{
    if ((state == NULL) || (rt == NULL) || (frame == NULL)) {
        return;
    }

    frame_clear(frame);

    switch (state->page) {
    case UI_DIAG:
        build_diag_frame(state, rt, frame);
        break;
    case UI_MAIN_MENU:
        build_main_menu_frame(state, frame);
        break;
    case UI_DEBUG_MENU:
        build_debug_menu_frame(state, frame);
        break;
    case UI_DEBUG_ADC:
        build_debug_adc_frame(rt, frame);
        break;
    case UI_BOOT_INFO:
        build_boot_info_frame(rt, frame);
        break;
    case UI_MEASURE_MENU:
        build_measure_menu_frame(state, frame);
        break;
    case UI_RES_RANGE:
        build_res_range_frame(state, frame);
        break;
    case UI_RES_READY:
        build_ready_frame(state, frame, "RES READY");
        break;
    case UI_RES_RUN:
        build_res_run_frame(state, rt, frame);
        break;
    case UI_VDC_READY:
        build_ready_frame(state, frame, "VDC READY");
        break;
    case UI_FREQ_READY:
        build_ready_frame(state, frame, "FREQ READY");
        break;
    case UI_CONT_READY:
        build_ready_frame(state, frame, "CONT READY");
        break;
    case UI_DIODE_READY:
        build_ready_frame(state, frame, "DIODE READY");
        break;
    default:
        build_diag_frame(state, rt, frame);
        break;
    }
}
