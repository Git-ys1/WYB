#include "drv_mux4051.h"

#include "../BSP/bsp.h"

static mux_res_range_t g_res_range = MUX_RES_200R;
static mux_mode_channel_t g_mode = MUX_MODE_VOLTAGE;
static mux_volt_range_t g_volt_range = MUX_VOLT_2000MV;
static uint8_t g_mode_phys_ch = 0u;
static uint8_t g_volt_phys_ch = 0u;

static void write_3bit_value(bsp_pin_t a_pin, bsp_pin_t b_pin, bsp_pin_t c_pin, uint8_t value)
{
    bsp_gpio_write(a_pin, (value & 0x01u) != 0u);
    bsp_gpio_write(b_pin, (value & 0x02u) != 0u);
    bsp_gpio_write(c_pin, (value & 0x04u) != 0u);
}

static uint8_t mode_to_phys_ch(mux_mode_channel_t mode)
{
    /* T-1.4.2-R1 hardware freeze:
     * U9 (MODE_MUX): CH0=VDC, CH1=RES/CONT/DIODE (shared path this round).
     */
    if (mode == MUX_MODE_VOLTAGE) {
        return 0u;
    }
    return 1u;
}

static uint8_t volt_to_phys_ch(mux_volt_range_t range)
{
    uint8_t ch_2000 = 0u;
    uint8_t ch_20 = 1u;

    /* T-1.4.2-R1 hardware freeze:
     * U11 (VOLT_RANGE_MUX): CH0=2000mV, CH1=20V.
     */
#if VDC_SWAP_U11_AB
    ch_2000 = 1u;
    ch_20 = 0u;
#endif

    if (range == MUX_VOLT_20V) {
        return ch_20;
    }
    return ch_2000;
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
    g_mode_phys_ch = mode_to_phys_ch(mode);
    write_3bit_value(BSP_PIN_MODE_A, BSP_PIN_MODE_B, BSP_PIN_MODE_C, g_mode_phys_ch);
}

void mux_set_volt_range(mux_volt_range_t range)
{
    if (range >= MUX_VOLT_COUNT) {
        return;
    }
    g_volt_range = range;
    g_volt_phys_ch = volt_to_phys_ch(range);
    bsp_gpio_write(BSP_PIN_VOLT_A, (g_volt_phys_ch & 0x01u) != 0u);
    bsp_gpio_write(BSP_PIN_VOLT_B, (g_volt_phys_ch & 0x02u) != 0u);
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

uint8_t mux_get_mode_phys_ch(void)
{
    return g_mode_phys_ch;
}

uint8_t mux_get_volt_phys_ch(void)
{
    return g_volt_phys_ch;
}

