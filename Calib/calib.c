#include "calib.h"

#include <string.h>

static const calib_profile_t g_default = {
    .vdc = {
        [VDC_RANGE_2000MV] = {1.0000f, 0.0f},
        [VDC_RANGE_20V] = {1.0000f, 0.0f}
    },
    .res = {
        [RES_RANGE_200] = {1.0000f, 0.0f},
        [RES_RANGE_2K] = {1.0000f, 0.0f},
        [RES_RANGE_20K] = {1.0000f, 0.0f},
        [RES_RANGE_200K] = {1.0000f, 0.0f}
    },
    .diode = {1.0000f, 0.0f},
    .v_div = {
        [VDC_RANGE_2000MV] = 1.0f,
        [VDC_RANGE_20V] = 10.0f
    },
    .r_ref_ohm = {
        [RES_RANGE_200] = 200.0f,
        [RES_RANGE_2K] = 2000.0f,
        [RES_RANGE_20K] = 20000.0f,
        [RES_RANGE_200K] = 200000.0f
    },
    .ads_lsb_mv = 0.0625f,
    .v_exc = 3.3f,
    .continuity_on_ohm = 10.0f,
    .continuity_off_ohm = 15.0f
};

app_err_t calib_load(calib_profile_t *out)
{
    if (out == 0) {
        return ERR_INVALID_ARG;
    }

    memcpy(out, &g_default, sizeof(g_default));
    return ERR_OK;
}

app_err_t calib_save(const calib_profile_t *in)
{
    (void)in;
    return ERR_NOT_IMPL;
}
