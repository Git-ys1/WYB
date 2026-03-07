#include "app_ui_presenter.h"

#include <string.h>

#include "../BSP/bsp_oled_smoke.h"

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

app_err_t app_ui_presenter_init(void)
{
    s_ready = false;
    s_last_err = ERR_OK;

    /* Keep the proven smoke bring-up timing. */
    HAL_Delay(250);
    if (!oled_smoke_init(OLED_PROFILE_SSD1315_PAGE, OLED_ADDR_7BIT)) {
        s_last_err = map_diag_err();
        return s_last_err;
    }

    s_ready = true;
    s_last_err = ERR_OK;
    return ERR_OK;
}

bool app_ui_presenter_ready(void)
{
    return s_ready;
}

app_err_t app_ui_presenter_last_err(void)
{
    return s_last_err;
}

app_err_t app_ui_presenter_flush(const app_ui_frame_t *frame)
{
    uint8_t i;

    if (frame == NULL) {
        s_last_err = ERR_INVALID_ARG;
        return s_last_err;
    }
    if (!s_ready) {
        if (s_last_err == ERR_OK) {
            s_last_err = ERR_HW_FAIL;
        }
        return s_last_err;
    }

    oled_smoke_fb_clear(0x00u);
    for (i = 0u; i < 8u; i++) {
        if (frame->line[i][0] != '\0') {
            oled_smoke_draw_text_line(i, frame->line[i]);
        }
    }

    if (!oled_smoke_flush_full()) {
        s_last_err = map_diag_err();
        s_ready = false;
        return s_last_err;
    }

    s_last_err = ERR_OK;
    return ERR_OK;
}
