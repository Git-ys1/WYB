#include "app_display_service.h"

#include <stdio.h>
#include <string.h>

#include "../BSP/bsp_oled_smoke.h"
#include "stm32g4xx_hal.h"

static bool s_ready;
static app_err_t s_last_err;

static app_err_t map_diag_err(void)
{
    oled_smoke_diag_t diag;

    oled_smoke_diag_get(&diag);
    if (diag.stage == OLED_SMOKE_FAIL_PROBE) {
        return ERR_I2C_NACK;
    }
    if (diag.stage == OLED_SMOKE_FAIL_INIT) {
        return ERR_HW_FAIL;
    }
    if ((diag.stage == OLED_SMOKE_FAIL_FLUSH_CMD) || (diag.stage == OLED_SMOKE_FAIL_FLUSH_DATA)) {
        return ERR_HW_FAIL;
    }
    return ERR_HW_FAIL;
}

static bool display_flush_frame(const app_ui_frame_t *frame)
{
    uint8_t i;

    if (frame == NULL) {
        return false;
    }

    oled_smoke_fb_clear(0x00u);
    for (i = 0u; i < 8u; i++) {
        if (frame->line[i][0] != '\0') {
            oled_smoke_draw_text_line(i, frame->line[i]);
        }
    }
    return oled_smoke_flush_full();
}

app_err_t app_display_init(void)
{
    s_ready = false;
    s_last_err = ERR_OK;

    HAL_Delay(250);
    if (!oled_smoke_init(OLED_PROFILE_SSD1315_PAGE, OLED_ADDR_7BIT)) {
        s_last_err = map_diag_err();
        return s_last_err;
    }

    s_ready = true;
    s_last_err = ERR_OK;
    return ERR_OK;
}

bool app_display_ready(void)
{
    return s_ready;
}

app_err_t app_display_last_err(void)
{
    return s_last_err;
}

app_err_t app_display_show_boot(void)
{
    app_ui_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    (void)snprintf(frame.line[0], sizeof(frame.line[0]), "BOOT OK");
    (void)snprintf(frame.line[1], sizeof(frame.line[1]), "UI UNIFY R1");

    return app_display_show_menu_frame(&frame);
}

app_err_t app_display_show_adc_debug(uint16_t raw, bool raw_valid,
                                     uint32_t mv, bool mv_valid,
                                     uint32_t vdda_mv, bool vdda_valid,
                                     app_err_t stat)
{
    app_ui_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    (void)snprintf(frame.line[0], sizeof(frame.line[0]), "DEBUG/ADC");
    if (raw_valid) {
        (void)snprintf(frame.line[1], sizeof(frame.line[1]), "RAW:%5u", raw);
    } else {
        (void)snprintf(frame.line[1], sizeof(frame.line[1]), "RAW: ----");
    }

    if (mv_valid) {
        (void)snprintf(frame.line[2], sizeof(frame.line[2]), "MV :%lu.%03lu",
                       (unsigned long)(mv / 1000u),
                       (unsigned long)(mv % 1000u));
    } else {
        (void)snprintf(frame.line[2], sizeof(frame.line[2]), "MV : ----");
    }

    if (vdda_valid) {
        (void)snprintf(frame.line[3], sizeof(frame.line[3]), "VDDA:%4lu", (unsigned long)vdda_mv);
    } else {
        (void)snprintf(frame.line[3], sizeof(frame.line[3]), "VDDA:----");
    }

    if (stat == ERR_OK) {
        (void)snprintf(frame.line[4], sizeof(frame.line[4]), "STAT:OK");
    } else {
        (void)snprintf(frame.line[4], sizeof(frame.line[4]), "STAT:ERR%d", (int)stat);
    }

    return app_display_show_menu_frame(&frame);
}

app_err_t app_display_show_menu_frame(const app_ui_frame_t *frame)
{
    app_err_t init_err;

    if (frame == NULL) {
        s_last_err = ERR_INVALID_ARG;
        return s_last_err;
    }

    if (!s_ready) {
        init_err = app_display_init();
        if (init_err != ERR_OK) {
            (void)app_display_show_fallback("EX2");
            return init_err;
        }
    }

    if (!display_flush_frame(frame)) {
        s_last_err = map_diag_err();
        s_ready = false;
        (void)app_display_show_fallback("EX3");
        return s_last_err;
    }

    s_last_err = ERR_OK;
    s_ready = true;
    return ERR_OK;
}

app_err_t app_display_show_fallback(const char *err_tag)
{
    app_ui_frame_t frame;
    app_err_t init_err;
    const char *tag = (err_tag == NULL) ? "EX?" : err_tag;

    if (!s_ready) {
        init_err = app_display_init();
        if (init_err != ERR_OK) {
            return init_err;
        }
    }

    memset(&frame, 0, sizeof(frame));
    (void)snprintf(frame.line[0], sizeof(frame.line[0]), "OLED FALLBACK");
    (void)snprintf(frame.line[1], sizeof(frame.line[1]), "ERR: %s", tag);
    (void)snprintf(frame.line[2], sizeof(frame.line[2]), "USE SMOKE BASE");
    (void)snprintf(frame.line[3], sizeof(frame.line[3]), "CHECK EXP SWITCH");

    if (!display_flush_frame(&frame)) {
        s_last_err = map_diag_err();
        s_ready = false;
        return s_last_err;
    }

    s_last_err = ERR_OK;
    s_ready = true;
    return ERR_OK;
}
