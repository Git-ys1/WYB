#include "drv_mux4051.h"

#include "../BSP/bsp.h"

static mux_res_range_t g_res_range = MUX_RES_200R;
static mux_mode_channel_t g_mode = MUX_MODE_VOLTAGE;
static mux_volt_range_t g_volt_range = MUX_VOLT_2000MV;

static void write_3bit_value(bsp_pin_t a_pin, bsp_pin_t b_pin, bsp_pin_t c_pin, uint8_t value)
{
    bsp_gpio_write(a_pin, (value & 0x01u) != 0u);
    bsp_gpio_write(b_pin, (value & 0x02u) != 0u);
    bsp_gpio_write(c_pin, (value & 0x04u) != 0u);
}

void mux_init(void)
{
    mux_set_res_range(g_res_range);
    mux_set_mode(g_mode);
    mux_set_volt_range(g_volt_range);
}

void mux_set_res_range(mux_res_range_t range)
{
    if (range >= MUX_RES_COUNT) {
        return;
    }
    g_res_range = range;
    write_3bit_value(BSP_PIN_RES_A, BSP_PIN_RES_B, BSP_PIN_RES_C, (uint8_t)range);
}

void mux_set_mode(mux_mode_channel_t mode)
{
    g_mode = mode;
    write_3bit_value(BSP_PIN_MODE_A, BSP_PIN_MODE_B, BSP_PIN_MODE_C, (uint8_t)mode);
}

void mux_set_volt_range(mux_volt_range_t range)
{
    if (range >= MUX_VOLT_COUNT) {
        return;
    }
    g_volt_range = range;
    bsp_gpio_write(BSP_PIN_VOLT_A, ((uint8_t)range & 0x01u) != 0u);
    bsp_gpio_write(BSP_PIN_VOLT_B, ((uint8_t)range & 0x02u) != 0u);
}

mux_res_range_t mux_get_res_range(void)
{
    return g_res_range;
}

mux_mode_channel_t mux_get_mode(void)
{
    return g_mode;
}

mux_volt_range_t mux_get_volt_range(void)
{
    return g_volt_range;
}

