#ifndef RES_DISPLAY_FMT_H
#define RES_DISPLAY_FMT_H

#include <stdbool.h>
#include <stdint.h>

#include "measure_res.h"

typedef struct {
    char r_disp_str[16];
    char stat_str[12];
    char line_value[22];
    char line_stat[22];
} res_display_text_t;

void res_format_display(const res_range_binding_t *binding,
                        const res_sample_t *s,
                        bool afe_ok,
                        bool calc_ok,
                        float r_calc_ohm,
                        res_display_text_t *out);

#endif
