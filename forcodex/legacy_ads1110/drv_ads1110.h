#ifndef DRV_ADS1110_H
#define DRV_ADS1110_H

#include <stdint.h>

#include "../BSP/bsp.h"
#include "drv_error.h"

typedef struct {
    bsp_i2c_bus_t bus;
    uint8_t addr7;
    uint8_t cfg;
    float lsb_mv;
} ads1110_t;

#define ADS1110_CFG_DEFAULT 0x8Cu

app_err_t ads1110_init(ads1110_t *dev, bsp_i2c_bus_t bus, uint8_t addr7, uint8_t cfg, float lsb_mv);
app_err_t ads1110_set_cfg(ads1110_t *dev, uint8_t cfg);
app_err_t ads1110_read_raw(const ads1110_t *dev, int16_t *raw_code);
app_err_t ads1110_read_mv(const ads1110_t *dev, float *mv);

#endif

