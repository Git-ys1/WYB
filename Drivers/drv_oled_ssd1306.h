#ifndef DRV_OLED_SSD1306_H
#define DRV_OLED_SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#include "../BSP/bsp.h"
#include "drv_error.h"

#define OLED_WIDTH 128u
#define OLED_HEIGHT 64u

typedef struct {
    bsp_i2c_bus_t bus;
    uint8_t addr7;
    bool initialized;
} oled_t;

app_err_t oled_init(oled_t *oled, bsp_i2c_bus_t bus, uint8_t addr7);
void oled_clear(void);
void oled_clear_line(uint8_t line);
void oled_invalidate_all(void);
void oled_draw_text(uint8_t x, uint8_t y, const char *text);
void oled_draw_text_line(uint8_t line, const char *text);
void oled_draw_big_num(uint8_t x, uint8_t y, const char *text);
app_err_t oled_flush_dirty(const oled_t *oled);
app_err_t oled_flush(const oled_t *oled);

#endif

