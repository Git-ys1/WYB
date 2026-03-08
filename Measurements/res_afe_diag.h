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

typedef enum {
    RES_AFE_WIN_INVALID = 0,
    RES_AFE_WIN_SHORT,
    RES_AFE_WIN_MID,
    RES_AFE_WIN_OPEN
} res_afe_window_t;

void res_afe_diag_reset(uint8_t range_sel);
void res_afe_diag_reset_all(void);
void res_afe_diag_update(uint8_t range_sel, const res_sample_t *s);
res_afe_health_t res_afe_diag_get(uint8_t range_sel);
res_afe_window_t res_afe_get_window(uint8_t range_sel, const res_sample_t *s);
const char *res_afe_window_name(res_afe_window_t w);
bool res_check_afe_health(uint8_t range_sel, const res_sample_t *s);

#endif
