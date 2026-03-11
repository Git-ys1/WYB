#include "bsp.h"

#if !BSP_USE_HAL_PORT

#include <stdio.h>
#include <string.h>

static uint32_t g_ms;
static bool g_pins[BSP_PIN_COUNT];

void bsp_init(void)
{
    memset(g_pins, 0, sizeof(g_pins));
    g_pins[BSP_PIN_KEY] = true;
    g_ms = 0;
}

uint32_t bsp_millis(void)
{
    return g_ms;
}

void bsp_delay_ms(uint32_t delay_ms)
{
    g_ms += delay_ms;
}

bool bsp_i2c_write(bsp_i2c_bus_t bus, uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)addr7;
    (void)data;
    (void)len;
    (void)timeout_ms;
    return false;
}

bool bsp_i2c_read(bsp_i2c_bus_t bus, uint8_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)bus;
    (void)addr7;
    (void)timeout_ms;
    if (data != NULL) {
        memset(data, 0, len);
    }
    return false;
}

bool bsp_i2c_probe(bsp_i2c_bus_t bus, uint8_t addr7, uint32_t timeout_ms)
{
    (void)bus;
    (void)addr7;
    (void)timeout_ms;
    return false;
}

void bsp_gpio_write(bsp_pin_t pin, bool level)
{
    if (pin < BSP_PIN_COUNT) {
        g_pins[pin] = level;
    }
}

bool bsp_gpio_read(bsp_pin_t pin)
{
    if (pin < BSP_PIN_COUNT) {
        return g_pins[pin];
    }
    return false;
}

bool bsp_pwm_start(bsp_pwm_t pwm, uint32_t freq_hz, uint8_t duty_pct)
{
    (void)pwm;
    (void)freq_hz;
    (void)duty_pct;
    return true;
}

void bsp_pwm_stop(bsp_pwm_t pwm)
{
    (void)pwm;
}

bool bsp_freq_get_capture(bsp_capture_t *capture)
{
    if (capture == NULL) {
        return false;
    }
    capture->period_ticks = 0u;
    capture->high_ticks = 0u;
    capture->tim_clk_hz = 0u;
    capture->last_capture_ms = 0u;
    capture->valid = false;
    return false;
}

bool bsp_freq_capture_start(void)
{
    return false;
}

void bsp_freq_capture_set_profile(bsp_freq_profile_t profile)
{
    (void)profile;
}

bool bsp_freq_get_diag(bsp_freq_diag_t *diag)
{
    if (diag == NULL) {
        return false;
    }
    memset(diag, 0, sizeof(*diag));
    return true;
}

void bsp_debug_log(const char *msg)
{
    if (msg != NULL) {
        printf("%s\n", msg);
    }
}

#endif
