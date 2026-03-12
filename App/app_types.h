#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

#include "../Drivers/drv_error.h"

typedef enum {
    MODE_VDC = 0,
    MODE_RES,
    MODE_FREQ,
    MODE_CONT,
    MODE_DIODE,
    MODE_COUNT
} app_mode_t;

typedef enum {
    UNIT_NONE = 0,
    UNIT_MV,
    UNIT_V,
    UNIT_OHM,
    UNIT_HZ,
    UNIT_PERCENT
} meas_unit_t;

enum {
    MEAS_FLAG_NONE = 0,
    MEAS_FLAG_OVERRANGE = 1 << 0,
    MEAS_FLAG_NEGATIVE = 1 << 1,
    MEAS_FLAG_OPEN = 1 << 2,
    MEAS_FLAG_NO_CONDUCTION = 1 << 3
};

typedef struct {
    float value;
    meas_unit_t unit;
    uint32_t flags;
    app_err_t err;
} meas_result_t;

typedef enum {
    VDC_RANGE_2000MV = 0,
    VDC_RANGE_20V,
    VDC_RANGE_COUNT
} vdc_range_t;

typedef enum {
    RES_RANGE_200 = 0,
    RES_RANGE_2K,
    RES_RANGE_20K,
    RES_RANGE_200K,
    RES_RANGE_COUNT
} res_range_t;

typedef enum {
    FREQ_RANGE_AUTO = 0,
    FREQ_RANGE_20HZ,
    FREQ_RANGE_200HZ,
    FREQ_RANGE_2KHZ,
    FREQ_RANGE_20KHZ,
    FREQ_RANGE_200KHZ,
    FREQ_RANGE_COUNT
} freq_range_t;

#endif
