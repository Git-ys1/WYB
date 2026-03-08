#include "res_afe_diag.h"

#include <string.h>

typedef struct {
    uint16_t short_th_mv;
    uint16_t open_margin_mv;
} afe_threshold_t;

/* Conservative thresholds for pre-OPAMP diagnosis. */
static const afe_threshold_t k_th[RES_RANGE_SEL_COUNT] = {
    [RES_RANGE_SEL_AUTO] = {0u, 0u},
    [RES_RANGE_SEL_200] = {30u, 120u},
    [RES_RANGE_SEL_2K] = {20u, 100u},
    [RES_RANGE_SEL_20K] = {15u, 90u},
    [RES_RANGE_SEL_200K] = {12u, 80u}
};

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
    const afe_threshold_t *th;
    res_afe_health_t *h;

    if ((range_sel >= RES_RANGE_SEL_COUNT) || (s == NULL) || !s->valid) {
        return;
    }
    if (range_sel == RES_RANGE_SEL_AUTO) {
        return;
    }

    th = &k_th[range_sel];
    h = &g_health[range_sel];

    if (s->mv <= th->short_th_mv) {
        h->short_seen = true;
    }
    if (s->vdda_mv > th->open_margin_mv) {
        if (s->mv >= (s->vdda_mv - th->open_margin_mv)) {
            h->open_seen = true;
        }
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
