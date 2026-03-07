#ifndef CALIB_H
#define CALIB_H

#include "../App/app_types.h"

typedef struct {
    float k;
    float b;
} calib_linear_t;

typedef struct {
    calib_linear_t vdc[VDC_RANGE_COUNT];
    calib_linear_t res[RES_RANGE_COUNT];
    calib_linear_t diode;
    float v_div[VDC_RANGE_COUNT];
    float r_ref_ohm[RES_RANGE_COUNT];
    float ads_lsb_mv;
    float v_exc;
    float continuity_on_ohm;
    float continuity_off_ohm;
} calib_profile_t;

app_err_t calib_load(calib_profile_t *out);
app_err_t calib_save(const calib_profile_t *in);

#endif
