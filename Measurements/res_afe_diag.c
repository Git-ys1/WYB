#include "res_afe_diag.h"

#include <string.h>

#define AFE_SHORT_TH_MV 50u
#define AFE_OPEN_RATIO_NUM 90u
#define AFE_OPEN_RATIO_DEN 100u

static res_afe_health_t g_health[RES_RANGE_SEL_COUNT];

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
    uint32_t open_th_mv;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (s == NULL) || !s->valid) {
        return;
    }
    if (range_sel == RES_RANGE_SEL_AUTO) {
        return;
    }

    h = &g_health[range_sel];

    if (s->mv < AFE_SHORT_TH_MV) {
        h->short_seen = true;
    }
    open_th_mv = (s->vdda_mv * AFE_OPEN_RATIO_NUM) / AFE_OPEN_RATIO_DEN;
    if (s->mv > open_th_mv) {
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

bool res_check_afe_health(uint8_t range_sel, const res_sample_t *s)
{
    if ((range_sel >= RES_RANGE_SEL_COUNT) || (s == NULL) || !s->valid) {
        return false;
    }
    if (range_sel == RES_RANGE_SEL_AUTO) {
        return false;
    }
    return g_health[range_sel].afe_ok;
}
