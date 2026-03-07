#include <stdbool.h>
#include <string.h>

#include "../BSP/bsp_oled_smoke.h"
#include "../Drivers/drv_error.h"

typedef struct {
    char line[8][22];
} app_ui_frame_t;

#define PRESENTER_FAIL_STREAK_FALLBACK 2u

static bool s_ready;
static app_err_t s_last_err;
static uint8_t s_fail_streak;

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

static bool render_frame(const char lines[8][22])
{
    uint8_t i;

    oled_smoke_fb_clear(0x00u);
    for (i = 0u; i < 8u; i++) {
        if (lines[i][0] != '\0') {
            oled_smoke_draw_text_line(i, lines[i]);
        }
    }
    return oled_smoke_flush_full();
}

static void render_fallback(void)
{
    static const char k_fallback[8][22] = {
        "OLED FALLBACK",
        "ERR: EX3",
        "USE SMOKE BASE",
        "CHECK EXP SWITCH",
        "",
        "",
        "",
        ""
    };

    (void)render_frame(k_fallback);
}

app_err_t app_ui_presenter_init(void)
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
    s_fail_streak = 0u;
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
    if (frame == NULL) {
        s_last_err = ERR_INVALID_ARG;
        return s_last_err;
    }

    if (!s_ready) {
        app_err_t init_err = app_ui_presenter_init();
        if (init_err != ERR_OK) {
            s_fail_streak++;
            if (s_fail_streak >= PRESENTER_FAIL_STREAK_FALLBACK) {
                render_fallback();
            }
            return init_err;
        }
    }

    if (!render_frame(frame->line)) {
        s_last_err = map_diag_err();
        s_fail_streak++;
        /* Keep presenter alive; do not permanently lock on a single failure. */
        s_ready = true;
        if (s_fail_streak >= PRESENTER_FAIL_STREAK_FALLBACK) {
            render_fallback();
        }
        return s_last_err;
    }

    s_fail_streak = 0u;
    s_last_err = ERR_OK;
    return ERR_OK;
}
