#ifndef RES_AFE_DIAG_H
#define RES_AFE_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#include "measure_res.h"

typedef struct {
    bool short_seen;
    bool open_seen;
    bool afe_ok;
} res_afe_health_t;

void res_afe_diag_reset(uint8_t range_sel);
void res_afe_diag_reset_all(void);
void res_afe_diag_update(uint8_t range_sel, const res_sample_t *s);
res_afe_health_t res_afe_diag_get(uint8_t range_sel);
bool res_check_afe_health(uint8_t range_sel, const res_sample_t *s);

#endif
