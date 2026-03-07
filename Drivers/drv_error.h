#ifndef DRV_ERROR_H
#define DRV_ERROR_H

#include <stdint.h>

typedef enum {
    ERR_OK = 0,
    ERR_I2C_NACK,
    ERR_ADC_TIMEOUT,
    ERR_OVERRANGE,
    ERR_NO_SIGNAL,
    ERR_INVALID_ARG,
    ERR_NOT_IMPL,
    ERR_HW_FAIL
} app_err_t;

#endif
