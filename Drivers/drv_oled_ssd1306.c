#include "drv_oled_ssd1306.h"

#include <stddef.h>
#include <string.h>

static uint8_t s_fb[OLED_WIDTH * (OLED_HEIGHT / 8u)];
static oled_t s_oled;
static uint8_t s_dirty_page_mask;

typedef struct {
    char c;
    uint8_t col[5];
} glyph_t;

static const glyph_t s_glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
    {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'/', {0x20, 0x10, 0x08, 0x04, 0x02}},
    {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'<', {0x08, 0x14, 0x22, 0x41, 0x00}},
    {'>', {0x41, 0x22, 0x14, 0x08, 0x00}},
    {'%', {0x63, 0x13, 0x08, 0x64, 0x63}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x62, 0x51, 0x49, 0x49, 0x46}},
    {'3', {0x22, 0x49, 0x49, 0x49, 0x36}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
    {'5', {0x2F, 0x49, 0x49, 0x49, 0x31}},
    {'6', {0x3E, 0x49, 0x49, 0x49, 0x32}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {0x26, 0x49, 0x49, 0x49, 0x3E}},
    {'A', {0x7E, 0x09, 0x09, 0x09, 0x7E}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x7F, 0x20, 0x18, 0x20, 0x7F}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x03, 0x04, 0x78, 0x04, 0x03}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}}
};

static const uint8_t *find_glyph(char c)
{
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

static void set_pixel(uint8_t x, uint8_t y, bool on)
{
    uint16_t idx;
    uint8_t page;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
        return;
    }

    idx = (uint16_t)x + ((uint16_t)(y / 8u) * OLED_WIDTH);
    page = (uint8_t)(y / 8u);
    if (on) {
        s_fb[idx] |= (uint8_t)(1u << (y % 8u));
    } else {
        s_fb[idx] &= (uint8_t)~(1u << (y % 8u));
    }
    s_dirty_page_mask |= (uint8_t)(1u << page);
}

static void draw_char_5x7(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = find_glyph(c);
    uint8_t col;
    uint8_t row;

    for (col = 0u; col < 5u; col++) {
        for (row = 0u; row < 7u; row++) {
            bool on = ((glyph[col] >> row) & 0x01u) != 0u;
            set_pixel((uint8_t)(x + col), (uint8_t)(y + row), on);
        }
    }
}

static void draw_char_10x14(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = find_glyph(c);
    uint8_t col;
    uint8_t row;
    uint8_t dx;
    uint8_t dy;

    for (col = 0u; col < 5u; col++) {
        for (row = 0u; row < 7u; row++) {
            bool on = ((glyph[col] >> row) & 0x01u) != 0u;
            for (dx = 0u; dx < 2u; dx++) {
                for (dy = 0u; dy < 2u; dy++) {
                    set_pixel((uint8_t)(x + col * 2u + dx), (uint8_t)(y + row * 2u + dy), on);
                }
            }
        }
    }
}

static app_err_t send_cmd(uint8_t cmd)
{
    uint8_t tx[2];

    tx[0] = 0x00u;
    tx[1] = cmd;
    if (!bsp_i2c_write(s_oled.bus, s_oled.addr7, tx, 2u, 20u)) {
        return ERR_I2C_NACK;
    }
    return ERR_OK;
}

static app_err_t flush_page(const oled_t *ctx, uint8_t page)
{
    uint8_t packet[1u + OLED_WIDTH];

    if (send_cmd((uint8_t)(0xB0u + page)) != ERR_OK) {
        return ERR_I2C_NACK;
    }
    if (send_cmd(0x00u) != ERR_OK) {
        return ERR_I2C_NACK;
    }
    if (send_cmd(0x10u) != ERR_OK) {
        return ERR_I2C_NACK;
    }

    packet[0] = 0x40u;
    memcpy(&packet[1], &s_fb[(uint16_t)page * OLED_WIDTH], OLED_WIDTH);
    if (!bsp_i2c_write(ctx->bus, ctx->addr7, packet, sizeof(packet), 20u)) {
        return ERR_I2C_NACK;
    }

    return ERR_OK;
}

app_err_t oled_init(oled_t *oled, bsp_i2c_bus_t bus, uint8_t addr7)
{
    static const uint8_t seq[] = {
        0xAEu,
        0x20u, 0x00u,
        0xB0u,
        0xC8u,
        0x00u,
        0x10u,
        0x40u,
        0x81u, 0x7Fu,
        0xA1u,
        0xA6u,
        0xA8u, 0x3Fu,
        0xA4u,
        0xD3u, 0x00u,
        0xD5u, 0x80u,
        0xD9u, 0xF1u,
        0xDAu, 0x12u,
        0xDBu, 0x40u,
        0x8Du, 0x14u,
        0xAFu
    };
    uint32_t i;
    app_err_t err;

    s_oled.bus = bus;
    s_oled.addr7 = addr7;
    s_oled.initialized = false;

    for (i = 0u; i < sizeof(seq); i++) {
        err = send_cmd(seq[i]);
        if (err != ERR_OK) {
            return err;
        }
    }

    oled_clear();
    err = oled_flush(&s_oled);
    if (err != ERR_OK) {
        return err;
    }

    s_oled.initialized = true;
    if (oled != NULL) {
        *oled = s_oled;
    }

    return ERR_OK;
}

void oled_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
    s_dirty_page_mask = 0xFFu;
}

void oled_clear_line(uint8_t line)
{
    uint8_t page = line;

    if (page >= (OLED_HEIGHT / 8u)) {
        return;
    }

    memset(&s_fb[(uint16_t)page * OLED_WIDTH], 0, OLED_WIDTH);
    s_dirty_page_mask |= (uint8_t)(1u << page);
}

void oled_invalidate_all(void)
{
    s_dirty_page_mask = 0xFFu;
}

void oled_draw_text(uint8_t x, uint8_t y, const char *text)
{
    uint8_t px = x;

    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        draw_char_5x7(px, y, *text);
        px = (uint8_t)(px + 6u);
        if (px >= (OLED_WIDTH - 6u)) {
            break;
        }
        text++;
    }
}

void oled_draw_text_line(uint8_t line, const char *text)
{
    uint8_t y = (uint8_t)(line * 8u);

    if (y >= OLED_HEIGHT) {
        return;
    }

    oled_draw_text(0u, y, text);
}

void oled_draw_big_num(uint8_t x, uint8_t y, const char *text)
{
    uint8_t px = x;

    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        draw_char_10x14(px, y, *text);
        px = (uint8_t)(px + 12u);
        if (px >= (OLED_WIDTH - 10u)) {
            break;
        }
        text++;
    }
}

app_err_t oled_flush(const oled_t *oled)
{
    uint8_t page;
    const oled_t *ctx = (oled != NULL) ? oled : &s_oled;
    app_err_t err;

    if (!ctx->initialized && (ctx != &s_oled)) {
        return ERR_INVALID_ARG;
    }

    for (page = 0u; page < 8u; page++) {
        err = flush_page(ctx, page);
        if (err != ERR_OK) {
            return err;
        }
    }

    s_dirty_page_mask = 0u;
    return ERR_OK;
}

app_err_t oled_flush_dirty(const oled_t *oled)
{
    uint8_t page;
    const oled_t *ctx = (oled != NULL) ? oled : &s_oled;
    uint8_t pending = s_dirty_page_mask;
    app_err_t err;

    if (!ctx->initialized && (ctx != &s_oled)) {
        return ERR_INVALID_ARG;
    }

    if (pending == 0u) {
        return ERR_OK;
    }

    for (page = 0u; page < 8u; page++) {
        if ((pending & (uint8_t)(1u << page)) == 0u) {
            continue;
        }

        err = flush_page(ctx, page);
        if (err != ERR_OK) {
            return err;
        }
    }

    s_dirty_page_mask = 0u;
    return ERR_OK;
}
