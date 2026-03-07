#include "drv_ads1110.h"

static app_err_t to_i2c_error(bool ok)
{
    return ok ? ERR_OK : ERR_I2C_NACK;
}

app_err_t ads1110_init(ads1110_t *dev, bsp_i2c_bus_t bus, uint8_t addr7, uint8_t cfg, float lsb_mv)
{
    if ((dev == 0) || (lsb_mv <= 0.0f)) {
        return ERR_INVALID_ARG;
    }

    dev->bus = bus;
    dev->addr7 = addr7;
    dev->cfg = cfg;
    dev->lsb_mv = lsb_mv;

    return ads1110_set_cfg(dev, cfg);
}

app_err_t ads1110_set_cfg(ads1110_t *dev, uint8_t cfg)
{
    bool ok;

    if (dev == 0) {
        return ERR_INVALID_ARG;
    }

    dev->cfg = cfg;
    ok = bsp_i2c_write(dev->bus, dev->addr7, &cfg, 1u, 20u);
    return to_i2c_error(ok);
}

app_err_t ads1110_read_raw(const ads1110_t *dev, int16_t *raw_code)
{
    uint8_t rx[3];
    bool ok;

    if ((dev == 0) || (raw_code == 0)) {
        return ERR_INVALID_ARG;
    }

    if (!bsp_i2c_probe(dev->bus, dev->addr7, 4u)) {
        return ERR_I2C_NACK;
    }

    ok = bsp_i2c_read(dev->bus, dev->addr7, rx, 3u, 20u);
    if (!ok) {
        return ERR_ADC_TIMEOUT;
    }

    *raw_code = (int16_t)((((uint16_t)rx[0]) << 8u) | rx[1]);
    return ERR_OK;
}

app_err_t ads1110_read_mv(const ads1110_t *dev, float *mv)
{
    int16_t raw;
    app_err_t err;

    if ((dev == 0) || (mv == 0)) {
        return ERR_INVALID_ARG;
    }

    err = ads1110_read_raw(dev, &raw);
    if (err != ERR_OK) {
        return err;
    }

    *mv = (float)raw * dev->lsb_mv;
    return ERR_OK;
}
