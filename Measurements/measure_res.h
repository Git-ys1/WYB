#ifndef MEASURE_RES_H
#define MEASURE_RES_H

#include <stdbool.h>
#include <stdint.h>

#include "../Drivers/drv_error.h"

typedef enum {
    RES_STAT_OK = 0,
    RES_STAT_OPEN,
    RES_STAT_SHORT,
    RES_STAT_OVR,
    RES_STAT_ERR
} res_live_stat_t;

typedef struct {
    bool valid;
    uint16_t raw_u16;
    uint32_t mv;
    uint32_t vdda_mv;
    float r_ohm;
    res_live_stat_t stat;
    app_err_t err;
} res_live_sample_t;

app_err_t measure_res_manual_sample(uint8_t range_sel, res_live_sample_t *out);
const char *measure_res_range_name(uint8_t range_sel);
bool measure_res_range_is_exp(uint8_t range_sel);
const char *measure_res_stat_name(res_live_stat_t stat);

#endif
