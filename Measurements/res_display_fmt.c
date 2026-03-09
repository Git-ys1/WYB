#include "res_display_fmt.h"

#include <stdio.h>
#include <string.h>

static void format_r_eng(float r_ohm, char *out, size_t out_sz)
{
    if ((out == NULL) || (out_sz == 0u)) {
        return;
    }

    if (r_ohm < 0.0f) {
        (void)snprintf(out, out_sz, "----");
        return;
    }

    if (r_ohm < 200.0f) {
        (void)snprintf(out, out_sz, "%.1fOhm", r_ohm);
    } else if (r_ohm < 2000.0f) {
        (void)snprintf(out, out_sz, "%.0fOhm", r_ohm);
    } else if (r_ohm < 10000.0f) {
        (void)snprintf(out, out_sz, "%.2fk", r_ohm / 1000.0f);
    } else if (r_ohm < 1000000.0f) {
        (void)snprintf(out, out_sz, "%.1fk", r_ohm / 1000.0f);
    } else {
        (void)snprintf(out, out_sz, "OL");
    }
}

static void format_lines(res_display_text_t *out)
{
    if (out == NULL) {
        return;
    }
    (void)snprintf(out->line_value, sizeof(out->line_value), "R: %s", out->r_disp_str);
    (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: %s", out->stat_str);
}

void res_format_display_state(const res_range_binding_t *binding,
                              const res_sample_t *s,
                              bool calc_ok,
                              float r_calc_ohm,
                              res_live_stat_t state,
                              res_display_text_t *out)
{
    (void)binding;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "----");
    (void)snprintf(out->stat_str, sizeof(out->stat_str), "ERR");

    if ((s == NULL) || !s->valid) {
        format_lines(out);
        return;
    }

    switch (state) {
    case RES_STAT_OK:
        if (!calc_ok) {
            (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "----");
            (void)snprintf(out->stat_str, sizeof(out->stat_str), "ERR");
            format_lines(out);
            return;
        }
        format_r_eng(r_calc_ohm, out->r_disp_str, sizeof(out->r_disp_str));
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "OK");
        break;

    case RES_STAT_SHORT:
        if (calc_ok) {
            format_r_eng(r_calc_ohm, out->r_disp_str, sizeof(out->r_disp_str));
        } else {
            (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "0.0Ohm");
        }
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "SHORT");
        break;

    case RES_STAT_OPEN:
    case RES_STAT_OVR:
        (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "OL");
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "%s", measure_res_stat_name(state));
        break;

    case RES_STAT_AFE_BAD:
        (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "----");
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "AFE BAD");
        break;

    case RES_STAT_ERR:
    default:
        (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "----");
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "ERR");
        break;
    }

    format_lines(out);
}

void res_format_display(const res_range_binding_t *binding,
                        const res_sample_t *s,
                        bool afe_ok,
                        bool calc_ok,
                        float r_calc_ohm,
                        res_display_text_t *out)
{
    res_live_stat_t state;

    if ((s == NULL) || !s->valid) {
        state = RES_STAT_ERR;
    } else if (!afe_ok) {
        state = RES_STAT_AFE_BAD;
    } else if (!calc_ok) {
        state = RES_STAT_OVR;
    } else {
        state = RES_STAT_OK;
    }

    res_format_display_state(binding, s, calc_ok, r_calc_ohm, state, out);
}
