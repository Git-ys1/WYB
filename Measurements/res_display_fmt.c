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

void res_format_display(uint8_t range_sel,
                        const res_sample_t *s,
                        bool afe_ok,
                        bool calc_ok,
                        float r_ohm,
                        res_display_text_t *out)
{
    char rbuf[20] = {0};

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    if ((s == NULL) || !s->valid) {
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: ----");
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: ERR");
        return;
    }

    if (!afe_ok) {
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: ----");
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: AFE BAD");
        return;
    }

    if (!calc_ok) {
        (void)snprintf(out->line_value, sizeof(out->line_value), "R: OL");
        (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: OVR");
        return;
    }

    format_r_ohm_3half(range_sel, r_ohm, rbuf, sizeof(rbuf));
    (void)snprintf(out->line_value, sizeof(out->line_value), "R: %.17s", rbuf);
    (void)snprintf(out->line_stat, sizeof(out->line_stat), "STAT: OK");
}
