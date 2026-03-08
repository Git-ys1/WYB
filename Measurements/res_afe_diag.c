#include "res_afe_diag.h"

#include <string.h>

#define AFE_SHORT_TH_MV 50u
#define AFE_OPEN_RATIO_NUM 90u
#define AFE_OPEN_RATIO_DEN 100u

static res_afe_health_t g_health[RES_RANGE_SEL_COUNT];

static uint32_t afe_open_th_mv(const res_sample_t *s)
{
    return (s->vdda_mv * AFE_OPEN_RATIO_NUM) / AFE_OPEN_RATIO_DEN;
}

void res_afe_diag_reset(uint8_t range_sel)
{
    if (range_sel >= RES_RANGE_SEL_COUNT) {
        return;
    }
    memset(&g_health[range_sel], 0, sizeof(g_health[range_sel]));
}

void res_afe_diag_reset_all(void)
{
    memset(g_health, 0, sizeof(g_health));
}

void res_afe_diag_update(uint8_t range_sel, const res_sample_t *s)
{
    res_afe_health_t *h;
    res_afe_window_t w;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (s == NULL) || !s->valid) {
        return;
    }
    if (range_sel == RES_RANGE_SEL_AUTO) {
        return;
    }

    h = &g_health[range_sel];
    w = res_afe_get_window(range_sel, s);
    if (w == RES_AFE_WIN_SHORT) {
        h->short_seen = true;
    }
    if (w == RES_AFE_WIN_OPEN) {
        h->open_seen = true;
    }
    h->afe_ok = h->short_seen && h->open_seen;
}

res_afe_health_t res_afe_diag_get(uint8_t range_sel)
{
    res_afe_health_t out = {0};

    if (range_sel >= RES_RANGE_SEL_COUNT) {
        return out;
    }
    return g_health[range_sel];
}

res_afe_window_t res_afe_get_window(uint8_t range_sel, const res_sample_t *s)
{
    uint32_t open_th_mv;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (s == NULL) || !s->valid) {
        return RES_AFE_WIN_INVALID;
    }
    if (range_sel == RES_RANGE_SEL_AUTO) {
        return RES_AFE_WIN_INVALID;
    }

    if (s->mv < AFE_SHORT_TH_MV) {
        return RES_AFE_WIN_SHORT;
    }

    open_th_mv = afe_open_th_mv(s);
    if (s->mv > open_th_mv) {
        return RES_AFE_WIN_OPEN;
    }

    return RES_AFE_WIN_MID;
}

const char *res_afe_window_name(res_afe_window_t w)
{
    switch (w) {
    case RES_AFE_WIN_SHORT:
        return "SHORT";
    case RES_AFE_WIN_MID:
        return "MID";
    case RES_AFE_WIN_OPEN:
        return "OPEN";
    case RES_AFE_WIN_INVALID:
    default:
        return "INV";
    }
}

bool res_check_afe_health(uint8_t range_sel, const res_sample_t *s)
{
    return (res_afe_get_window(range_sel, s) == RES_AFE_WIN_MID);
}
