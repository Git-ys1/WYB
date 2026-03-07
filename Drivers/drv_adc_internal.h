#ifndef DRV_ADC_INTERNAL_H
#define DRV_ADC_INTERNAL_H

#include <stdint.h>

#include "drv_error.h"

app_err_t adc1_init(void);
app_err_t adc1_read_raw_u16(uint16_t *raw);
app_err_t adc1_read_mv(uint32_t *mv);
app_err_t adc1_read_filtered(uint16_t *raw, uint32_t *mv);
app_err_t adc1_read_vdda_mv(uint32_t *vdda_mv);
app_err_t adc1_read_status(void);

#endif
