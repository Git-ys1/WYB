#include "app_display_service.h"

#include <stdio.h>
#include <string.h>

#include "../BSP/bsp_oled_smoke.h"
#include "stm32g4xx_hal.h"

#define APP_DISPLAY_FORCE_INIT_FAIL 0

typedef enum {
    DISP_UNINIT = 0,
    DISP_INITING,
    DISP_READY,
    DISP_FAULT
} disp_state_t;

static disp_state_t s_state = DISP_UNINIT;
static app_err_t s_last_err = ERR_OK;

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

static app_err_t flush_lines(const char lines[8][22])
{
    uint8_t i;

    oled_smoke_fb_clear(0x00u);
    for (i = 0u; i < 8u; i++) {
        if (lines[i][0] != '\0') {
            oled_smoke_draw_text_line(i, lines[i]);
        }
    }
    if (!oled_smoke_flush_full()) {
        s_last_err = map_diag_err();
        s_state = DISP_FAULT;
        return s_last_err;
    }

    s_last_err = ERR_OK;
    return ERR_OK;
}

app_err_t app_display_init_once(void)
{
    if (s_state == DISP_READY) {
        return ERR_OK;
    }
    if (s_state == DISP_INITING) {
        return ERR_HW_FAIL;
    }

    s_state = DISP_INITING;
    s_last_err = ERR_OK;

#if APP_DISPLAY_FORCE_INIT_FAIL
    s_last_err = ERR_HW_FAIL;
    s_state = DISP_FAULT;
    return s_last_err;
#endif

    HAL_Delay(250u);
    if (!oled_smoke_init(OLED_PROFILE_SSD1315_PAGE, OLED_ADDR_7BIT)) {
        s_last_err = map_diag_err();
        s_state = DISP_FAULT;
        return s_last_err;
    }

    s_state = DISP_READY;
    s_last_err = ERR_OK;
    return ERR_OK;
}

void app_display_poll(void)
{
    /* R1 keeps display polling lightweight and deterministic. */
}

bool app_display_ready(void)
{
    return (s_state == DISP_READY);
}

app_err_t app_display_last_err(void)
{
    return s_last_err;
}

app_err_t app_display_render(const app_ui_frame_t *frame)
{
    if (frame == NULL) {
        s_last_err = ERR_INVALID_ARG;
        return s_last_err;
    }
    if (s_state != DISP_READY) {
        if (s_last_err == ERR_OK) {
            s_last_err = ERR_HW_FAIL;
        }
        return s_last_err;
    }

    return flush_lines(frame->line);
}

app_err_t app_display_render_fault(uint8_t fault_code, uint8_t stage)
{
    char lines[8][22];

    if (s_state != DISP_READY) {
        if (s_last_err == ERR_OK) {
            s_last_err = ERR_HW_FAIL;
        }
        return s_last_err;
    }

    memset(lines, 0, sizeof(lines));
    (void)snprintf(lines[0], sizeof(lines[0]), "OLED FALLBACK");
    (void)snprintf(lines[1], sizeof(lines[1]), "ERR: %u", (unsigned)fault_code);
    (void)snprintf(lines[2], sizeof(lines[2]), "STAGE: %u", (unsigned)stage);
    (void)snprintf(lines[3], sizeof(lines[3]), "USE SMOKE BASE");
    (void)snprintf(lines[4], sizeof(lines[4]), "CHECK WIRING");

    return flush_lines(lines);
}
