#ifndef APP_MENU_TREE_H
#define APP_MENU_TREE_H

#include <stdbool.h>
#include <stdint.h>

#include "../BSP/bsp_keys.h"
#include "../Drivers/drv_error.h"
#include "app_display_service.h"

typedef enum {
    UI_DIAG = 0,
    UI_MAIN_MENU,
    UI_DEBUG_MENU,
    UI_DEBUG_ADC,
    UI_BOOT_INFO,
    UI_MEASURE_MENU,
    UI_RES_RANGE,
    UI_RES_READY,
    UI_RES_RUN,
    UI_VDC_READY,
    UI_FREQ_READY,
    UI_CONT_READY,
    UI_DIODE_READY
} app_ui_page_t;

typedef struct {
    app_ui_page_t page;
    uint8_t main_sel;
    uint8_t debug_sel;
    uint8_t measure_sel;
    uint8_t res_range_sel;
    bool menu_unlocked;
} menu_state_t;

typedef struct {
    uint16_t raw_u16;
    bool raw_valid;
    uint32_t mv;
    bool mv_valid;
    uint32_t vdda_mv;
    bool vdda_valid;
    app_err_t adc_stat;
    uint8_t stage;
    uint8_t fault;
    bool display_ready;
    bool menu_enabled;
    uint32_t menu_wait_sec;
    bool res_valid;
    bool res_is_exp;
    uint16_t res_raw_u16;
    uint32_t res_mv;
    float res_ohm;
    uint8_t res_stat;
} app_runtime_data_t;

void app_menu_init(menu_state_t *state);
void app_menu_set_unlocked(menu_state_t *state, bool unlocked);
bool app_menu_is_unlocked(const menu_state_t *state);
bool app_menu_allows_adc_updates(const menu_state_t *state);
bool app_menu_is_run_page(const menu_state_t *state);
const char *app_menu_res_range_name(uint8_t sel);
void app_menu_handle_key(menu_state_t *state, key_id_t key);
void app_menu_build_frame(const menu_state_t *state, const app_runtime_data_t *rt, app_ui_frame_t *frame);

#endif
