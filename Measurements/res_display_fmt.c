#include "res_display_fmt.h"

#include <stdio.h>
#include <string.h>

static void format_r_ohm_3half(uint8_t range_sel, float r_ohm, char *out, size_t out_sz)
{
    uint32_t scaled;

    if ((out == NULL) || (out_sz == 0u)) {
        return;
    }

    switch (range_sel) {
    case RES_RANGE_SEL_200:
        if (r_ohm > 199.9f) {
            (void)snprintf(out, out_sz, "OL");
            return;
        }
        scaled = (uint32_t)(r_ohm * 10.0f + 0.5f);
        (void)snprintf(out, out_sz, "%lu.%01luOhm",
                       (unsigned long)(scaled / 10u),
                       (unsigned long)(scaled % 10u));
        return;

    case RES_RANGE_SEL_2K:
        if (r_ohm > 1999.0f) {
            (void)snprintf(out, out_sz, "OL");
            return;
        }
        scaled = (uint32_t)(r_ohm + 0.5f);
        (void)snprintf(out, out_sz, "%lu.%03luk",
                       (unsigned long)(scaled / 1000u),
                       (unsigned long)(scaled % 1000u));
        return;

    case RES_RANGE_SEL_20K:
        if (r_ohm > 19990.0f) {
            (void)snprintf(out, out_sz, "OL");
            return;
        }
        scaled = (uint32_t)(r_ohm / 10.0f + 0.5f);
        (void)snprintf(out, out_sz, "%lu.%02luk",
                       (unsigned long)(scaled / 100u),
                       (unsigned long)(scaled % 100u));
        return;

    case RES_RANGE_SEL_200K:
        if (r_ohm > 199900.0f) {
            (void)snprintf(out, out_sz, "OL");
            return;
        }
        scaled = (uint32_t)(r_ohm / 100.0f + 0.5f);
        (void)snprintf(out, out_sz, "%lu.%01luk",
                       (unsigned long)(scaled / 10u),
                       (unsigned long)(scaled % 10u));
        return;

    default:
        (void)snprintf(out, out_sz, "----");
        return;
    }
}

static void format_r_ohm_from_binding(const res_range_binding_t *binding, float r_calc_ohm, char *out, size_t out_sz)
{
    if ((binding == NULL) || (out == NULL) || (out_sz == 0u)) {
        return;
    }

    switch (binding->formatter_id) {
    case RES_FMT_200:
        format_r_ohm_3half(RES_RANGE_SEL_200, r_calc_ohm, out, out_sz);
        break;
    case RES_FMT_2K:
        format_r_ohm_3half(RES_RANGE_SEL_2K, r_calc_ohm, out, out_sz);
        break;
    case RES_FMT_20K:
        format_r_ohm_3half(RES_RANGE_SEL_20K, r_calc_ohm, out, out_sz);
        break;
    case RES_FMT_200K:
        format_r_ohm_3half(RES_RANGE_SEL_200K, r_calc_ohm, out, out_sz);
        break;
    case RES_FMT_NONE:
    default:
        (void)snprintf(out, out_sz, "----");
        break;
    }
}

void res_format_display(const res_range_binding_t *binding,
                        const res_sample_t *s,
                        bool afe_ok,
                        bool calc_ok,
                        float r_calc_ohm,
                        res_display_text_t *out)
{
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "----");
    (void)snprintf(out->stat_str, sizeof(out->stat_str), "ERR");

    if ((s == NULL) || !s->valid) {
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: %s", out->r_disp_str);
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: %s", out->stat_str);
        return;
    }

    if (!afe_ok) {
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "AFE BAD");
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: %s", out->r_disp_str);
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: %s", out->stat_str);
        return;
    }

    if (!calc_ok) {
        (void)snprintf(out->r_disp_str, sizeof(out->r_disp_str), "OL");
        (void)snprintf(out->stat_str, sizeof(out->stat_str), "OVR");
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: %s", out->r_disp_str);
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: %s", out->stat_str);
        return;
    }

    format_r_ohm_from_binding(binding, r_calc_ohm, out->r_disp_str, sizeof(out->r_disp_str));
    (void)snprintf(out->stat_str, sizeof(out->stat_str), "OK");
    (void)snprintf(out->line_value, sizeof(out->line_value), "R: %s", out->r_disp_str);
    (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: %s", out->stat_str);
}
