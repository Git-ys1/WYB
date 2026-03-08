#ifndef MEASURE_RES_AUTO_H
#define MEASURE_RES_AUTO_H

#include <stdbool.h>
#include <stdint.h>

#include "measure_res.h"
#include "res_afe_diag.h"
#include "res_display_fmt.h"

typedef enum {
    AUTO_IDLE = 0,
    AUTO_TRACK,
    AUTO_SWITCH_WAIT
} res_auto_state_t;

typedef struct {
    bool valid;
    bool in_settle;
    uint8_t locked_range_sel;
    uint8_t vote_up;
    uint8_t vote_down;
    res_sample_t sample;
    res_range_binding_t binding;
    res_afe_health_t health_hist;
    bool afe_ok;
    res_afe_window_t window;
    bool calc_ok;
    app_err_t calc_err;
    float r_calc_ohm;
    res_display_text_t disp;
} res_auto_result_t;

void measure_res_auto_reset(void);
void measure_res_auto_enter(void);
app_err_t measure_res_auto_step(uint32_t now_ms, res_auto_result_t *out);
uint8_t measure_res_auto_locked_range(void);
void measure_res_auto_get_vote(uint8_t *up, uint8_t *down);
res_auto_state_t measure_res_auto_state(void);

#endif
