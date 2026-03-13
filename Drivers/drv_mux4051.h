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

/* MODE CD4051 logical channel contract:
 * CH0=VOLTAGE, CH1=RES(+CONT in T-1.4.5C), CH2=DIODE, CH3=FREQ/CAP.
 * A/B/C control follows bit0/bit1/bit2 of the physical channel index.
 */
typedef enum {
    MUX_MODE_VOLTAGE = 0, /* logical VOLTAGE path */
    MUX_MODE_RES,         /* logical RES path */
    MUX_MODE_DIODE,       /* logical DIODE path */
    MUX_MODE_ONOFF,       /* logical CONT path (shared with RES this round) */
    MUX_MODE_FREQ,        /* logical FREQ path */
    MUX_MODE_CAP,         /* logical CAP path (T-1.7.5-R1 freeze: U4 CH3) */
    MUX_MODE_RESERVED6,
    MUX_MODE_RESERVED7
} mux_mode_channel_t;

typedef enum {
    MUX_VOLT_2000MV = 0,
    MUX_VOLT_20V,
    MUX_VOLT_COUNT
} mux_volt_range_t;

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
