#ifndef DRV_MUX4051_H
#define DRV_MUX4051_H

#include <stdint.h>

typedef enum {
    MUX_RES_200R = 0,
    MUX_RES_2K,
    MUX_RES_20K,
    MUX_RES_200K,
    MUX_RES_COUNT
} mux_res_range_t;

typedef enum {
    MUX_MODE_VOLTAGE = 0,
    MUX_MODE_RES,
    MUX_MODE_DIODE,
    MUX_MODE_ONOFF,
    MUX_MODE_AC,
    MUX_MODE_CAP,
    MUX_MODE_RESERVED6,
    MUX_MODE_RESERVED7
} mux_mode_channel_t;

typedef enum {
    MUX_VOLT_2000MV = 0,
    MUX_VOLT_20V,
    MUX_VOLT_COUNT
} mux_volt_range_t;

/* T-1.4.3-R1 diagnostics only:
 * 0 = default mapping (2000mV->CH0, 20V->CH1)
 * 1 = swapped mapping (2000mV->CH1, 20V->CH0)
 */
#ifndef VDC_SWAP_U11_AB
#define VDC_SWAP_U11_AB 0
#endif

void mux_init(void);
void mux_set_res_range(mux_res_range_t range);
void mux_set_mode(mux_mode_channel_t mode);
void mux_set_volt_range(mux_volt_range_t range);

mux_res_range_t mux_get_res_range(void);
mux_mode_channel_t mux_get_mode(void);
mux_volt_range_t mux_get_volt_range(void);
uint8_t mux_get_mode_phys_ch(void);
uint8_t mux_get_volt_phys_ch(void);

#endif
