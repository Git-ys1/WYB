#ifndef BSP_OLED_SMOKE_H
#define BSP_OLED_SMOKE_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"

#define OLED_ADDR_7BIT 0x3Cu
#define OLED_ADDR_HAL  ((uint16_t)(OLED_ADDR_7BIT << 1))

typedef enum {
    OLED_PROFILE_SSD1315_PAGE = 0,
    OLED_PROFILE_SSD1306_PAGE,
    OLED_PROFILE_SH1106_PAGE_XOFF2
} oled_smoke_profile_t;

typedef enum {
    OLED_STEP_CMD_A5 = 0,
    OLED_STEP_CMD_A4,
    OLED_STEP_BLACK,
    OLED_STEP_WHITE,
    OLED_STEP_TOP_HALF_WHITE_BOTTOM_BLACK,
    OLED_STEP_LEFT_HALF_WHITE_RIGHT_BLACK,
    OLED_STEP_BORDER_1PX,
    OLED_STEP_STRIPE_AA55,
    OLED_STEP_CHECKER
} oled_smoke_step_t;

typedef enum {
    OLED_SMOKE_FAIL_NONE = 0,
    OLED_SMOKE_FAIL_PROBE,
    OLED_SMOKE_FAIL_INIT,
    OLED_SMOKE_FAIL_FLUSH_CMD,
    OLED_SMOKE_FAIL_FLUSH_DATA
} oled_smoke_fail_stage_t;

typedef struct {
    oled_smoke_fail_stage_t stage;
    uint8_t page;
    uint8_t chunk;
    HAL_StatusTypeDef hal_status;
} oled_smoke_diag_t;

typedef struct {
    uint8_t phase_id;
    uint8_t page;
    uint8_t chunk;
    HAL_StatusTypeDef hal_status;
    uint8_t cmd_first[2];
    uint8_t data_first8[8];
} oled_smoke_phase_log_t;

bool oled_smoke_init(oled_smoke_profile_t profile, uint8_t addr7);
bool oled_smoke_test_pattern(oled_smoke_step_t step);
bool oled_smoke_show_counter(uint32_t cnt);
void oled_smoke_fb_clear(uint8_t fill);
void oled_smoke_fb_set_fullscreen_bitmap(const uint8_t *bmp);
void oled_smoke_draw_text_line(uint8_t line, const char *text);
bool oled_smoke_flush_full(void);
void oled_smoke_diag_reset(void);
void oled_smoke_diag_get(oled_smoke_diag_t *out);
void oled_smoke_set_phase(uint8_t phase_id);
void oled_smoke_get_phase_log(oled_smoke_phase_log_t *out);
bool oled_smoke_set_remap(uint8_t seg_remap_cmd, uint8_t com_scan_cmd);

const char *oled_smoke_profile_name(oled_smoke_profile_t p);
uint8_t oled_smoke_get_addr(void);
uint8_t oled_smoke_get_addr7(void);
uint16_t oled_smoke_get_addr_hal(void);
void oled_smoke_get_first_packets(const uint8_t **cmd_pkt, uint16_t *cmd_len,
                                  const uint8_t **data_pkt, uint16_t *data_len);

#endif
