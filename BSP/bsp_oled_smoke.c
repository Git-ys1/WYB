#include "bsp_oled_smoke.h"

#include <stdio.h>
#include <string.h>

#include "../Core/Inc/main.h"

extern I2C_HandleTypeDef hi2c2;

#define OLED_SMOKE_W 128u
#define OLED_SMOKE_H 64u
#define OLED_SMOKE_PAGES (OLED_SMOKE_H / 8u)
#define OLED_SMOKE_FB_SIZE (OLED_SMOKE_W * OLED_SMOKE_PAGES)
#define OLED_SMOKE_CHUNK_BYTES 16u

#define OLED_SMOKE_TIMEOUT_PROBE_MS 25u
#define OLED_SMOKE_TIMEOUT_CMD_MS 25u
#define OLED_SMOKE_TIMEOUT_DATA_MS 25u
#define OLED_SMOKE_PROBE_TRIALS 5u

#define OLED_SMOKE_PAGE_INVALID 0xFFu
#define OLED_SMOKE_CHUNK_INVALID 0xFFu

#ifndef OLED_INIT_PROFILE_A
#define OLED_INIT_PROFILE_A 1
#endif

static oled_smoke_profile_t s_profile = OLED_PROFILE_SSD1315_PAGE;
static uint8_t s_addr7 = OLED_ADDR_7BIT;
static uint16_t s_addr_hal = OLED_ADDR_HAL;
static uint8_t s_fb[OLED_SMOKE_FB_SIZE];

static oled_smoke_diag_t s_diag;
static oled_smoke_phase_log_t s_phase_log;
static uint8_t s_phase_cmd_captured;
static uint8_t s_phase_data_captured;

static uint8_t s_first_cmd_pkt[2];
static uint8_t s_first_data_pkt[1u + OLED_SMOKE_CHUNK_BYTES];
static uint16_t s_first_cmd_len;
static uint16_t s_first_data_len;

typedef struct {
    char c;
    uint8_t col[5];
} glyph_t;

static const uint8_t *glyph_for_char(char c)
{
    static const glyph_t s_glyphs[] = {
        {' ', {0x00u, 0x00u, 0x00u, 0x00u, 0x00u}},
        {'-', {0x08u, 0x08u, 0x08u, 0x08u, 0x08u}},
        {'.', {0x00u, 0x60u, 0x60u, 0x00u, 0x00u}},
        {'/', {0x20u, 0x10u, 0x08u, 0x04u, 0x02u}},
        {':', {0x00u, 0x36u, 0x36u, 0x00u, 0x00u}},
        {'<', {0x08u, 0x14u, 0x22u, 0x41u, 0x00u}},
        {'>', {0x41u, 0x22u, 0x14u, 0x08u, 0x00u}},
        {'%', {0x63u, 0x13u, 0x08u, 0x64u, 0x63u}},
        {'0', {0x3Eu, 0x51u, 0x49u, 0x45u, 0x3Eu}},
        {'1', {0x00u, 0x42u, 0x7Fu, 0x40u, 0x00u}},
        {'2', {0x62u, 0x51u, 0x49u, 0x49u, 0x46u}},
        {'3', {0x22u, 0x49u, 0x49u, 0x49u, 0x36u}},
        {'4', {0x18u, 0x14u, 0x12u, 0x7Fu, 0x10u}},
        {'5', {0x2Fu, 0x49u, 0x49u, 0x49u, 0x31u}},
        {'6', {0x3Eu, 0x49u, 0x49u, 0x49u, 0x32u}},
        {'7', {0x01u, 0x71u, 0x09u, 0x05u, 0x03u}},
        {'8', {0x36u, 0x49u, 0x49u, 0x49u, 0x36u}},
        {'9', {0x26u, 0x49u, 0x49u, 0x49u, 0x3Eu}},
        {'A', {0x7Eu, 0x09u, 0x09u, 0x09u, 0x7Eu}},
        {'B', {0x7Fu, 0x49u, 0x49u, 0x49u, 0x36u}},
        {'C', {0x3Eu, 0x41u, 0x41u, 0x41u, 0x22u}},
        {'D', {0x7Fu, 0x41u, 0x41u, 0x22u, 0x1Cu}},
        {'E', {0x7Fu, 0x49u, 0x49u, 0x49u, 0x41u}},
        {'F', {0x7Fu, 0x09u, 0x09u, 0x09u, 0x01u}},
        {'G', {0x3Eu, 0x41u, 0x49u, 0x49u, 0x7Au}},
        {'H', {0x7Fu, 0x08u, 0x08u, 0x08u, 0x7Fu}},
        {'I', {0x00u, 0x41u, 0x7Fu, 0x41u, 0x00u}},
        {'J', {0x20u, 0x40u, 0x41u, 0x3Fu, 0x01u}},
        {'K', {0x7Fu, 0x08u, 0x14u, 0x22u, 0x41u}},
        {'L', {0x7Fu, 0x40u, 0x40u, 0x40u, 0x40u}},
        {'M', {0x7Fu, 0x02u, 0x0Cu, 0x02u, 0x7Fu}},
        {'N', {0x7Fu, 0x04u, 0x08u, 0x10u, 0x7Fu}},
        {'O', {0x3Eu, 0x41u, 0x41u, 0x41u, 0x3Eu}},
        {'P', {0x7Fu, 0x09u, 0x09u, 0x09u, 0x06u}},
        {'Q', {0x3Eu, 0x41u, 0x51u, 0x21u, 0x5Eu}},
        {'R', {0x7Fu, 0x09u, 0x19u, 0x29u, 0x46u}},
        {'S', {0x46u, 0x49u, 0x49u, 0x49u, 0x31u}},
        {'T', {0x01u, 0x01u, 0x7Fu, 0x01u, 0x01u}},
        {'U', {0x3Fu, 0x40u, 0x40u, 0x40u, 0x3Fu}},
        {'V', {0x1Fu, 0x20u, 0x40u, 0x20u, 0x1Fu}},
        {'W', {0x7Fu, 0x20u, 0x18u, 0x20u, 0x7Fu}},
        {'X', {0x63u, 0x14u, 0x08u, 0x14u, 0x63u}},
        {'Y', {0x03u, 0x04u, 0x78u, 0x04u, 0x03u}},
        {'Z', {0x61u, 0x51u, 0x49u, 0x45u, 0x43u}}
    };
    uint32_t i;
    char uc = c;

    if ((uc >= 'a') && (uc <= 'z')) {
        uc = (char)(uc - ('a' - 'A'));
    }

    for (i = 0u; i < (sizeof(s_glyphs) / sizeof(s_glyphs[0])); i++) {
        if (s_glyphs[i].c == uc) {
            return s_glyphs[i].col;
        }
    }

    return s_glyphs[0].col;
}

static void fb_clear(uint8_t fill)
{
    memset(s_fb, fill, sizeof(s_fb));
}

static void fb_set_pixel(uint8_t x, uint8_t y, uint8_t on)
{
    uint16_t idx;
    uint8_t mask;

    if ((x >= OLED_SMOKE_W) || (y >= OLED_SMOKE_H)) {
        return;
    }

    idx = (uint16_t)x + ((uint16_t)(y / 8u) * OLED_SMOKE_W);
    mask = (uint8_t)(1u << (y % 8u));
    if (on != 0u) {
        s_fb[idx] |= mask;
    } else {
        s_fb[idx] &= (uint8_t)~mask;
    }
}

static void fb_draw_char(uint8_t x, uint8_t y, char c)
{
    uint8_t col;
    uint8_t row;
    const uint8_t *g = glyph_for_char(c);

    for (col = 0u; col < 5u; col++) {
        for (row = 0u; row < 7u; row++) {
            fb_set_pixel((uint8_t)(x + col), (uint8_t)(y + row), (uint8_t)((g[col] >> row) & 0x01u));
        }
    }
}

static void fb_draw_text(uint8_t x, uint8_t y, const char *text)
{
    uint8_t px = x;

    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        if (px > (OLED_SMOKE_W - 6u)) {
            break;
        }
        fb_draw_char(px, y, *text);
        px = (uint8_t)(px + 6u);
        text++;
    }
}

void oled_smoke_diag_reset(void)
{
    s_diag.stage = OLED_SMOKE_FAIL_NONE;
    s_diag.page = OLED_SMOKE_PAGE_INVALID;
    s_diag.chunk = OLED_SMOKE_CHUNK_INVALID;
    s_diag.hal_status = HAL_OK;
}

void oled_smoke_diag_get(oled_smoke_diag_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_diag;
}

void oled_smoke_set_phase(uint8_t phase_id)
{
    s_phase_log.phase_id = phase_id;
}

void oled_smoke_get_phase_log(oled_smoke_phase_log_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_phase_log;
}

static void phase_log_prepare(void)
{
    s_phase_log.page = OLED_SMOKE_PAGE_INVALID;
    s_phase_log.chunk = OLED_SMOKE_CHUNK_INVALID;
    s_phase_log.hal_status = HAL_OK;
    memset(s_phase_log.cmd_first, 0, sizeof(s_phase_log.cmd_first));
    memset(s_phase_log.data_first8, 0, sizeof(s_phase_log.data_first8));
    s_phase_cmd_captured = 0u;
    s_phase_data_captured = 0u;
}

static void phase_log_note_failure(uint8_t page, uint8_t chunk, HAL_StatusTypeDef status)
{
    if (s_phase_log.hal_status != HAL_OK) {
        return;
    }
    s_phase_log.page = page;
    s_phase_log.chunk = chunk;
    s_phase_log.hal_status = status;
}

static void oled_smoke_diag_set(oled_smoke_fail_stage_t stage, uint8_t page, uint8_t chunk, HAL_StatusTypeDef status)
{
    if (s_diag.stage != OLED_SMOKE_FAIL_NONE) {
        return;
    }

    s_diag.stage = stage;
    s_diag.page = page;
    s_diag.chunk = chunk;
    s_diag.hal_status = status;
}

static bool oled_write_cmd_ex(uint8_t cmd, oled_smoke_fail_stage_t fail_stage, uint8_t page, uint8_t chunk)
{
    uint8_t pkt[2];
    HAL_StatusTypeDef st;

    pkt[0] = 0x00u;
    pkt[1] = cmd;

    if (s_first_cmd_len == 0u) {
        s_first_cmd_pkt[0] = pkt[0];
        s_first_cmd_pkt[1] = pkt[1];
        s_first_cmd_len = 2u;
    }

    if (s_phase_cmd_captured == 0u) {
        s_phase_log.cmd_first[0] = pkt[0];
        s_phase_log.cmd_first[1] = pkt[1];
        s_phase_cmd_captured = 1u;
    }

    st = HAL_I2C_Master_Transmit(&hi2c2, s_addr_hal, pkt, 2u, OLED_SMOKE_TIMEOUT_CMD_MS);
    if (st != HAL_OK) {
        oled_smoke_diag_set(fail_stage, page, chunk, st);
        phase_log_note_failure(page, chunk, st);
        return false;
    }

    return true;
}

static bool oled_write_data_chunk(const uint8_t *buf16, uint8_t page, uint8_t chunk)
{
    uint8_t pkt[1u + OLED_SMOKE_CHUNK_BYTES];
    HAL_StatusTypeDef st;

    if (buf16 == NULL) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_FLUSH_DATA, page, chunk, HAL_ERROR);
        phase_log_note_failure(page, chunk, HAL_ERROR);
        return false;
    }

    pkt[0] = 0x40u;
    memcpy(&pkt[1], buf16, OLED_SMOKE_CHUNK_BYTES);

    if (s_first_data_len == 0u) {
        memcpy(s_first_data_pkt, pkt, sizeof(pkt));
        s_first_data_len = (uint16_t)sizeof(pkt);
    }

    if (s_phase_data_captured == 0u) {
        memcpy(s_phase_log.data_first8, buf16, sizeof(s_phase_log.data_first8));
        s_phase_log.page = page;
        s_phase_log.chunk = chunk;
        s_phase_data_captured = 1u;
    }

    st = HAL_I2C_Master_Transmit(&hi2c2, s_addr_hal, pkt, (uint16_t)sizeof(pkt), OLED_SMOKE_TIMEOUT_DATA_MS);
    if (st != HAL_OK) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_FLUSH_DATA, page, chunk, st);
        phase_log_note_failure(page, chunk, st);
        return false;
    }

    return true;
}

static bool oled_send_cmd_seq(const uint8_t *seq, uint16_t len)
{
    uint16_t i;

    if ((seq == NULL) || (len == 0u)) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
        return false;
    }

    for (i = 0u; i < len; i++) {
        if (!oled_write_cmd_ex(seq[i], OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID)) {
            return false;
        }
    }

    return true;
}

bool oled_smoke_set_remap(uint8_t seg_remap_cmd, uint8_t com_scan_cmd)
{
    if (!((seg_remap_cmd == 0xA0u) || (seg_remap_cmd == 0xA1u))) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
        return false;
    }

    if (!((com_scan_cmd == 0xC0u) || (com_scan_cmd == 0xC8u))) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
        return false;
    }

    if (!oled_write_cmd_ex(seg_remap_cmd, OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID)) {
        return false;
    }
    if (!oled_write_cmd_ex(com_scan_cmd, OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID)) {
        return false;
    }

    return true;
}

static bool oled_set_page_col(uint8_t page)
{
    if (!oled_write_cmd_ex((uint8_t)(0xB0u + page), OLED_SMOKE_FAIL_FLUSH_CMD, page, OLED_SMOKE_CHUNK_INVALID)) {
        return false;
    }
    if (!oled_write_cmd_ex(0x00u, OLED_SMOKE_FAIL_FLUSH_CMD, page, OLED_SMOKE_CHUNK_INVALID)) {
        return false;
    }
    if (!oled_write_cmd_ex(0x10u, OLED_SMOKE_FAIL_FLUSH_CMD, page, OLED_SMOKE_CHUNK_INVALID)) {
        return false;
    }

    return true;
}

static bool oled_flush_fb(void)
{
    uint8_t page;
    uint8_t chunk;

    for (page = 0u; page < OLED_SMOKE_PAGES; page++) {
        if (!oled_set_page_col(page)) {
            return false;
        }

        for (chunk = 0u; chunk < (OLED_SMOKE_W / OLED_SMOKE_CHUNK_BYTES); chunk++) {
            const uint8_t *chunk_ptr = &s_fb[(uint16_t)page * OLED_SMOKE_W + ((uint16_t)chunk * OLED_SMOKE_CHUNK_BYTES)];
            if (!oled_write_data_chunk(chunk_ptr, page, chunk)) {
                return false;
            }
        }
    }

    return true;
}

static bool oled_init_profile(oled_smoke_profile_t profile)
{
#if OLED_INIT_PROFILE_A
    static const uint8_t init_a[] = {
        0xAEu,
        0xD5u, 0x80u,
        0xA8u, 0x3Fu,
        0xD3u, 0x00u,
        0x40u,
        0x8Du, 0x14u,
        0x20u, 0x02u,
        0xA1u,
        0xC8u,
        0xDAu, 0x12u,
        0x81u, 0xCFu,
        0xD9u, 0xF1u,
        0xDBu, 0x40u,
        0xA4u,
        0xA6u,
        0x2Eu,
        0xAFu
    };
#else
    static const uint8_t init_b[] = {
        0xAEu,
        0xD3u, 0x00u,
        0x40u,
        0x20u, 0x02u,
        0xA1u,
        0xC8u,
        0xDAu, 0x12u,
        0xA6u,
        0xA4u,
        0xAFu
    };
#endif

    if ((profile != OLED_PROFILE_SSD1315_PAGE) &&
        (profile != OLED_PROFILE_SSD1306_PAGE) &&
        (profile != OLED_PROFILE_SH1106_PAGE_XOFF2)) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_INIT, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
        return false;
    }

#if OLED_INIT_PROFILE_A
    if (!oled_send_cmd_seq(init_a, (uint16_t)sizeof(init_a))) {
        return false;
    }
#else
    if (!oled_send_cmd_seq(init_b, (uint16_t)sizeof(init_b))) {
        return false;
    }
#endif

    HAL_Delay(100);
    return true;
}

static void fb_fill_top_white_bottom_black(void)
{
    uint8_t page;

    for (page = 0u; page < OLED_SMOKE_PAGES; page++) {
        memset(&s_fb[(uint16_t)page * OLED_SMOKE_W], (page <= 3u) ? 0xFFu : 0x00u, OLED_SMOKE_W);
    }
}

static void fb_fill_left_white_right_black(void)
{
    uint8_t page;
    uint8_t x;

    for (page = 0u; page < OLED_SMOKE_PAGES; page++) {
        for (x = 0u; x < OLED_SMOKE_W; x++) {
            s_fb[(uint16_t)page * OLED_SMOKE_W + x] = (x < 64u) ? 0xFFu : 0x00u;
        }
    }
}

static void fb_fill_border_1px(void)
{
    uint8_t x;
    uint8_t y;

    fb_clear(0x00u);

    for (x = 0u; x < OLED_SMOKE_W; x++) {
        fb_set_pixel(x, 0u, 1u);
        fb_set_pixel(x, (uint8_t)(OLED_SMOKE_H - 1u), 1u);
    }

    for (y = 0u; y < OLED_SMOKE_H; y++) {
        fb_set_pixel(0u, y, 1u);
        fb_set_pixel((uint8_t)(OLED_SMOKE_W - 1u), y, 1u);
    }
}

static void fb_fill_stripe_aa55(void)
{
    uint8_t page;
    uint8_t x;

    for (page = 0u; page < OLED_SMOKE_PAGES; page++) {
        for (x = 0u; x < OLED_SMOKE_W; x++) {
            s_fb[(uint16_t)page * OLED_SMOKE_W + x] = ((x & 1u) == 0u) ? 0xAAu : 0x55u;
        }
    }
}

static void fb_fill_checker(void)
{
    uint8_t page;
    uint8_t x;
    uint8_t v;

    for (page = 0u; page < OLED_SMOKE_PAGES; page++) {
        for (x = 0u; x < OLED_SMOKE_W; x++) {
            v = ((x & 1u) == 0u) ? 0xAAu : 0x55u;
            if ((page & 1u) != 0u) {
                v = (uint8_t)~v;
            }
            s_fb[(uint16_t)page * OLED_SMOKE_W + x] = v;
        }
    }
}

const char *oled_smoke_profile_name(oled_smoke_profile_t p)
{
    switch (p) {
        case OLED_PROFILE_SSD1315_PAGE: return "1315";
        case OLED_PROFILE_SSD1306_PAGE: return "1306";
        case OLED_PROFILE_SH1106_PAGE_XOFF2: return "1106";
        default: return "UNKN";
    }
}

uint8_t oled_smoke_get_addr(void)
{
    return s_addr7;
}

uint8_t oled_smoke_get_addr7(void)
{
    return s_addr7;
}

uint16_t oled_smoke_get_addr_hal(void)
{
    return s_addr_hal;
}

void oled_smoke_get_first_packets(const uint8_t **cmd_pkt, uint16_t *cmd_len,
                                  const uint8_t **data_pkt, uint16_t *data_len)
{
    if (cmd_pkt != NULL) {
        *cmd_pkt = s_first_cmd_pkt;
    }
    if (cmd_len != NULL) {
        *cmd_len = s_first_cmd_len;
    }
    if (data_pkt != NULL) {
        *data_pkt = s_first_data_pkt;
    }
    if (data_len != NULL) {
        *data_len = s_first_data_len;
    }
}

bool oled_smoke_init(oled_smoke_profile_t profile, uint8_t addr7)
{
    HAL_StatusTypeDef probe_status;

    oled_smoke_diag_reset();

    if (addr7 != OLED_ADDR_7BIT) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_PROBE, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
        return false;
    }

    s_addr7 = addr7;
    s_addr_hal = OLED_ADDR_HAL;

    probe_status = HAL_I2C_IsDeviceReady(&hi2c2, s_addr_hal, OLED_SMOKE_PROBE_TRIALS, OLED_SMOKE_TIMEOUT_PROBE_MS);
    if (probe_status != HAL_OK) {
        oled_smoke_diag_set(OLED_SMOKE_FAIL_PROBE, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, probe_status);
        return false;
    }

    s_profile = profile;
    s_first_cmd_len = 0u;
    s_first_data_len = 0u;

    if (!oled_init_profile(profile)) {
        return false;
    }

    phase_log_prepare();
    fb_clear(0x00u);
    return true;
}

bool oled_smoke_test_pattern(oled_smoke_step_t step)
{
    oled_smoke_diag_reset();
    phase_log_prepare();

    switch (step) {
        case OLED_STEP_CMD_A5:
            return oled_write_cmd_ex(0xA5u, OLED_SMOKE_FAIL_FLUSH_CMD, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID);

        case OLED_STEP_CMD_A4:
            return oled_write_cmd_ex(0xA4u, OLED_SMOKE_FAIL_FLUSH_CMD, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID);

        case OLED_STEP_BLACK:
            fb_clear(0x00u);
            return oled_flush_fb();

        case OLED_STEP_WHITE:
            fb_clear(0xFFu);
            return oled_flush_fb();

        case OLED_STEP_TOP_HALF_WHITE_BOTTOM_BLACK:
            fb_fill_top_white_bottom_black();
            return oled_flush_fb();

        case OLED_STEP_LEFT_HALF_WHITE_RIGHT_BLACK:
            fb_fill_left_white_right_black();
            return oled_flush_fb();

        case OLED_STEP_BORDER_1PX:
            fb_fill_border_1px();
            return oled_flush_fb();

        case OLED_STEP_STRIPE_AA55:
            fb_fill_stripe_aa55();
            return oled_flush_fb();

        case OLED_STEP_CHECKER:
            fb_fill_checker();
            return oled_flush_fb();

        default:
            oled_smoke_diag_set(OLED_SMOKE_FAIL_FLUSH_DATA, OLED_SMOKE_PAGE_INVALID, OLED_SMOKE_CHUNK_INVALID, HAL_ERROR);
            return false;
    }
}

bool oled_smoke_show_counter(uint32_t cnt)
{
    char line[20];

    oled_smoke_diag_reset();
    phase_log_prepare();
    fb_clear(0x00u);
    fb_draw_text(0u, 0u, "OLED OK");

    (void)snprintf(line, sizeof(line), "PROF:%s", oled_smoke_profile_name(s_profile));
    fb_draw_text(0u, 16u, line);

    (void)snprintf(line, sizeof(line), "ADDR:%02X", (unsigned int)s_addr7);
    fb_draw_text(0u, 32u, line);

    (void)snprintf(line, sizeof(line), "CNT:%04lu", (unsigned long)(cnt % 10000u));
    fb_draw_text(0u, 48u, line);

    return oled_flush_fb();
}

void oled_smoke_fb_clear(uint8_t fill)
{
    fb_clear(fill);
}

void oled_smoke_draw_text_line(uint8_t line, const char *text)
{
    if ((line >= OLED_SMOKE_PAGES) || (text == NULL)) {
        return;
    }

    fb_draw_text(0u, (uint8_t)(line * 8u), text);
}

bool oled_smoke_flush_full(void)
{
    oled_smoke_diag_reset();
    phase_log_prepare();
    return oled_flush_fb();
}
