#ifndef MEASURE_CAP_H
#define MEASURE_CAP_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "stm32g4xx_hal.h"

#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_error.h"

typedef enum {
    CAP_RANGE_20N = 0,
    CAP_RANGE_2U,
    CAP_RANGE_200U,
    CAP_RANGE_COUNT
} cap_range_t;

typedef enum {
    CAP_STAT_PROBE = 0,
    CAP_STAT_DISCH,
    CAP_STAT_MEAS,
    CAP_STAT_OK,
    CAP_STAT_OL,
    CAP_STAT_ERR
} cap_stat_t;

typedef enum {
    CAP_SM_IDLE_SAFE = 0,
    CAP_SM_PRE_DISCHARGE,
    CAP_SM_WAIT_EMPTY,
    CAP_SM_ARM_CHARGE,
    CAP_SM_CHARGING,
    CAP_SM_CAPTURED,
    CAP_SM_HOLD_RESULT,
    CAP_SM_TIMEOUT,
    CAP_SM_ERROR
} cap_sm_state_t;

typedef struct {
    uint8_t valid;
    uint8_t over;
    cap_range_t range;
    uint32_t adc_threshold;
    uint32_t elapsed_cycles;
    uint16_t adc_raw_last;
    float value_f;
    float value_pf;
    float value_nf;
    float value_uf;
    cap_stat_t stat;
    app_err_t err;
    uint8_t empty_timeout;
    uint8_t timeout;
    cap_sm_state_t sm_state;
} cap_result_t;

/* Threshold and RC constant for alpha = 2587/4095. */
#define CAP_ADC_THRESHOLD_COUNT 2587.0f
#define CAP_ADC_THRESHOLD_RAW ((uint16_t)2587u)
#define CAP_K_COEFF 0.9989824477f

/* Optional fixture parasitic compensation (default disabled). */
#define CAP_OFFSET_PF 0.0f

/* Hardware freeze: PE3/PE4/PE5 charge selects, PE6 discharge. */
#define CAP_CHG_20N_GPIO_Port GPIOE
#define CAP_CHG_20N_Pin GPIO_PIN_3
#define CAP_CHG_2U_GPIO_Port GPIOE
#define CAP_CHG_2U_Pin GPIO_PIN_4
#define CAP_CHG_200U_GPIO_Port GPIOE
#define CAP_CHG_200U_Pin GPIO_PIN_5
#define CAP_DISCH_GPIO_Port GPIOE
#define CAP_DISCH_Pin GPIO_PIN_6

#define CAP_EMPTY_ADC_RAW 16u
#define CAP_EMPTY_STABLE_COUNT 3u
#define CAP_THRESHOLD_STABLE_COUNT 2u

#define CAP_WAIT_EMPTY_TIMEOUT_20N_US 10000u
#define CAP_WAIT_EMPTY_TIMEOUT_2U_US 50000u
#define CAP_WAIT_EMPTY_TIMEOUT_200U_US 500000u

#define CAP_TIMEOUT_20N_US 5000u
#define CAP_TIMEOUT_2U_US 50000u
#define CAP_TIMEOUT_200U_US 500000u

#define CAP_HOLD_20N_US 50000u
#define CAP_HOLD_2U_US 100000u
#define CAP_HOLD_200U_US 200000u

#define CAP_TIMEOUT_RECOVER_US 20000u
#define CAP_ERROR_RECOVER_US 20000u

typedef struct {
    bool entered;
    cap_range_t range;
    cap_sm_state_t state;
    uint32_t state_start_cycles;
    uint32_t charge_start_cycles;
    uint8_t empty_stable_count;
    uint8_t threshold_stable_count;
    uint8_t empty_timeout_flag;
    cap_result_t live;
    cap_result_t last_good;
} cap_sm_ctx_t;

static cap_sm_ctx_t g_cap_ctx = {
    .entered = false,
    .range = CAP_RANGE_20N,
    .state = CAP_SM_IDLE_SAFE
};

static inline void cap_dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t cap_cycles_now(void)
{
    return DWT->CYCCNT;
}

static inline uint32_t cap_cycles_from_us(uint32_t us)
{
    uint64_t ticks = ((uint64_t)SystemCoreClock * (uint64_t)us) / 1000000ull;
    if (ticks == 0ull) {
        ticks = 1ull;
    }
    if (ticks > 0xFFFFFFFFull) {
        ticks = 0xFFFFFFFFull;
    }
    return (uint32_t)ticks;
}

static inline bool cap_cycles_elapsed(uint32_t start, uint32_t target)
{
    return ((uint32_t)(cap_cycles_now() - start) >= target);
}

static inline void cap_gpio_as_analog(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin;
    init.Mode = GPIO_MODE_ANALOG;
    init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &init);
}

static inline void cap_gpio_as_output_high(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

static inline void cap_all_charge_hiz(void)
{
    cap_gpio_as_analog(CAP_CHG_20N_GPIO_Port, CAP_CHG_20N_Pin);
    cap_gpio_as_analog(CAP_CHG_2U_GPIO_Port, CAP_CHG_2U_Pin);
    cap_gpio_as_analog(CAP_CHG_200U_GPIO_Port, CAP_CHG_200U_Pin);
}

static inline void cap_select_charge_20n(void)
{
    cap_all_charge_hiz();
    cap_gpio_as_output_high(CAP_CHG_20N_GPIO_Port, CAP_CHG_20N_Pin);
}

static inline void cap_select_charge_2u(void)
{
    cap_all_charge_hiz();
    cap_gpio_as_output_high(CAP_CHG_2U_GPIO_Port, CAP_CHG_2U_Pin);
}

static inline void cap_select_charge_200u(void)
{
    cap_all_charge_hiz();
    cap_gpio_as_output_high(CAP_CHG_200U_GPIO_Port, CAP_CHG_200U_Pin);
}

static inline void cap_select_charge_range(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        cap_select_charge_20n();
        break;
    case CAP_RANGE_2U:
        cap_select_charge_2u();
        break;
    case CAP_RANGE_200U:
    default:
        cap_select_charge_200u();
        break;
    }
}

static inline void cap_discharge_on(void)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = CAP_DISCH_Pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CAP_DISCH_GPIO_Port, &init);
    HAL_GPIO_WritePin(CAP_DISCH_GPIO_Port, CAP_DISCH_Pin, GPIO_PIN_SET);
}

static inline void cap_discharge_off(void)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = CAP_DISCH_Pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CAP_DISCH_GPIO_Port, &init);
    HAL_GPIO_WritePin(CAP_DISCH_GPIO_Port, CAP_DISCH_Pin, GPIO_PIN_RESET);
}

static inline app_err_t cap_adc_read(uint16_t *raw)
{
    return adc1_read_opamp1_raw_u16(raw);
}

static inline float cap_range_r_ohm(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        return 100000.0f;
    case CAP_RANGE_2U:
        return 10000.0f;
    case CAP_RANGE_200U:
    default:
        return 1000.0f;
    }
}

static inline uint32_t cap_range_wait_empty_timeout_us(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        return CAP_WAIT_EMPTY_TIMEOUT_20N_US;
    case CAP_RANGE_2U:
        return CAP_WAIT_EMPTY_TIMEOUT_2U_US;
    case CAP_RANGE_200U:
    default:
        return CAP_WAIT_EMPTY_TIMEOUT_200U_US;
    }
}

static inline uint32_t cap_range_charge_timeout_us(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        return CAP_TIMEOUT_20N_US;
    case CAP_RANGE_2U:
        return CAP_TIMEOUT_2U_US;
    case CAP_RANGE_200U:
    default:
        return CAP_TIMEOUT_200U_US;
    }
}

static inline uint32_t cap_range_hold_us(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        return CAP_HOLD_20N_US;
    case CAP_RANGE_2U:
        return CAP_HOLD_2U_US;
    case CAP_RANGE_200U:
    default:
        return CAP_HOLD_200U_US;
    }
}

static inline const char *cap_range_name(cap_range_t range)
{
    switch (range) {
    case CAP_RANGE_20N:
        return "20nF";
    case CAP_RANGE_2U:
        return "2uF";
    case CAP_RANGE_200U:
        return "200uF";
    default:
        return "UNK";
    }
}

static inline const char *cap_stat_name(cap_stat_t stat)
{
    switch (stat) {
    case CAP_STAT_DISCH:
        return "DISCH";
    case CAP_STAT_MEAS:
        return "MEAS";
    case CAP_STAT_OK:
        return "READY";
    case CAP_STAT_OL:
        return "OL";
    case CAP_STAT_ERR:
        return "ERR";
    case CAP_STAT_PROBE:
    default:
        return "PROBE";
    }
}

static inline const char *cap_sm_state_name(cap_sm_state_t state)
{
    switch (state) {
    case CAP_SM_IDLE_SAFE:
        return "IDLE";
    case CAP_SM_PRE_DISCHARGE:
        return "PRED";
    case CAP_SM_WAIT_EMPTY:
        return "WEMP";
    case CAP_SM_ARM_CHARGE:
        return "ARM";
    case CAP_SM_CHARGING:
        return "CHG";
    case CAP_SM_CAPTURED:
        return "CAP";
    case CAP_SM_HOLD_RESULT:
        return "HOLD";
    case CAP_SM_TIMEOUT:
        return "TO";
    case CAP_SM_ERROR:
    default:
        return "ERR";
    }
}

static inline void cap_result_clear(cap_result_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->range = g_cap_ctx.range;
    out->adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
    out->stat = CAP_STAT_PROBE;
    out->err = ERR_NOT_IMPL;
    out->sm_state = g_cap_ctx.state;
}

static inline void cap_enter_state(cap_sm_ctx_t *ctx, cap_sm_state_t state)
{
    if (ctx == NULL) {
        return;
    }
    ctx->state = state;
    ctx->state_start_cycles = cap_cycles_now();
    ctx->live.sm_state = state;
}

static inline void cap_enter(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();

    cap_dwt_enable();
    cap_all_charge_hiz();
    cap_discharge_on();
    adc1_mark_input_path_changed();

    memset(&g_cap_ctx, 0, sizeof(g_cap_ctx));
    g_cap_ctx.entered = true;
    g_cap_ctx.range = CAP_RANGE_20N;
    g_cap_ctx.live.adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
    g_cap_ctx.last_good.adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
    g_cap_ctx.live.range = g_cap_ctx.range;
    g_cap_ctx.last_good.range = g_cap_ctx.range;
    g_cap_ctx.live.stat = CAP_STAT_PROBE;
    g_cap_ctx.last_good.stat = CAP_STAT_PROBE;
    g_cap_ctx.live.err = ERR_NOT_IMPL;
    g_cap_ctx.last_good.err = ERR_NOT_IMPL;
    cap_enter_state(&g_cap_ctx, CAP_SM_IDLE_SAFE);
}

static inline void cap_leave(void)
{
    cap_all_charge_hiz();
    cap_discharge_on();
    g_cap_ctx.entered = false;
    cap_enter_state(&g_cap_ctx, CAP_SM_IDLE_SAFE);
}

static inline void cap_set_range(cap_range_t range)
{
    if (range >= CAP_RANGE_COUNT) {
        return;
    }

    if (g_cap_ctx.range == range) {
        return;
    }

    g_cap_ctx.range = range;
    g_cap_ctx.live.range = range;
    if (g_cap_ctx.last_good.valid != 0u) {
        g_cap_ctx.last_good.range = range;
    }

    if (g_cap_ctx.entered) {
        cap_all_charge_hiz();
        cap_discharge_on();
        g_cap_ctx.empty_stable_count = 0u;
        g_cap_ctx.threshold_stable_count = 0u;
        g_cap_ctx.live.timeout = 0u;
        g_cap_ctx.live.empty_timeout = 0u;
        g_cap_ctx.live.over = 0u;
        g_cap_ctx.live.err = ERR_OK;
        cap_enter_state(&g_cap_ctx, CAP_SM_PRE_DISCHARGE);
    }
}

static inline void cap_update_output(cap_result_t *out)
{
    if (out == NULL) {
        return;
    }

    if (g_cap_ctx.last_good.valid != 0u) {
        *out = g_cap_ctx.last_good;
    } else {
        cap_result_clear(out);
    }

    out->range = g_cap_ctx.range;
    out->adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
    out->adc_raw_last = g_cap_ctx.live.adc_raw_last;
    out->elapsed_cycles = g_cap_ctx.live.elapsed_cycles;
    out->sm_state = g_cap_ctx.state;
    out->empty_timeout = g_cap_ctx.live.empty_timeout;
    out->timeout = g_cap_ctx.live.timeout;
    out->over = g_cap_ctx.live.over;
    out->err = g_cap_ctx.live.err;

    switch (g_cap_ctx.state) {
    case CAP_SM_WAIT_EMPTY:
    case CAP_SM_PRE_DISCHARGE:
        out->stat = CAP_STAT_DISCH;
        break;
    case CAP_SM_ARM_CHARGE:
    case CAP_SM_CHARGING:
        out->stat = CAP_STAT_MEAS;
        break;
    case CAP_SM_TIMEOUT:
        out->stat = CAP_STAT_OL;
        if (g_cap_ctx.last_good.valid == 0u) {
            out->valid = 0u;
        }
        break;
    case CAP_SM_ERROR:
        out->stat = CAP_STAT_ERR;
        if (g_cap_ctx.last_good.valid == 0u) {
            out->valid = 0u;
        }
        break;
    case CAP_SM_HOLD_RESULT:
    case CAP_SM_CAPTURED:
        out->stat = CAP_STAT_OK;
        break;
    case CAP_SM_IDLE_SAFE:
    default:
        if (out->valid != 0u) {
            out->stat = CAP_STAT_OK;
        } else {
            out->stat = CAP_STAT_PROBE;
        }
        break;
    }
}

static inline void cap_measure_once(cap_result_t *out)
{
    uint16_t adc_raw = 0u;
    app_err_t err;
    uint32_t timeout_cycles;
    float r_ohm;
    float denom;
    float c_f;
    float c_pf;

    if (out == NULL) {
        return;
    }

    if (!g_cap_ctx.entered) {
        cap_result_clear(out);
        out->stat = CAP_STAT_ERR;
        out->err = ERR_NOT_IMPL;
        out->sm_state = CAP_SM_IDLE_SAFE;
        return;
    }

    g_cap_ctx.live.range = g_cap_ctx.range;
    g_cap_ctx.live.adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;

    switch (g_cap_ctx.state) {
    case CAP_SM_IDLE_SAFE:
        cap_discharge_on();
        cap_all_charge_hiz();
        g_cap_ctx.empty_stable_count = 0u;
        g_cap_ctx.threshold_stable_count = 0u;
        g_cap_ctx.live.timeout = 0u;
        g_cap_ctx.live.empty_timeout = 0u;
        g_cap_ctx.live.over = 0u;
        g_cap_ctx.live.err = ERR_OK;
        cap_enter_state(&g_cap_ctx, CAP_SM_PRE_DISCHARGE);
        break;

    case CAP_SM_PRE_DISCHARGE:
        cap_discharge_on();
        cap_all_charge_hiz();
        g_cap_ctx.empty_stable_count = 0u;
        cap_enter_state(&g_cap_ctx, CAP_SM_WAIT_EMPTY);
        break;

    case CAP_SM_WAIT_EMPTY:
        err = cap_adc_read(&adc_raw);
        if (err != ERR_OK) {
            g_cap_ctx.live.err = err;
            g_cap_ctx.live.adc_raw_last = 0u;
            cap_enter_state(&g_cap_ctx, CAP_SM_ERROR);
            break;
        }

        g_cap_ctx.live.err = ERR_OK;
        g_cap_ctx.live.adc_raw_last = adc_raw;
        if (adc_raw <= CAP_EMPTY_ADC_RAW) {
            if (g_cap_ctx.empty_stable_count < 0xFFu) {
                g_cap_ctx.empty_stable_count++;
            }
        } else {
            g_cap_ctx.empty_stable_count = 0u;
        }

        if (g_cap_ctx.empty_stable_count >= CAP_EMPTY_STABLE_COUNT) {
            g_cap_ctx.live.empty_timeout = 0u;
            cap_enter_state(&g_cap_ctx, CAP_SM_ARM_CHARGE);
            break;
        }

        timeout_cycles = cap_cycles_from_us(cap_range_wait_empty_timeout_us(g_cap_ctx.range));
        if (cap_cycles_elapsed(g_cap_ctx.state_start_cycles, timeout_cycles)) {
            g_cap_ctx.live.empty_timeout = 1u;
            cap_enter_state(&g_cap_ctx, CAP_SM_ARM_CHARGE);
        }
        break;

    case CAP_SM_ARM_CHARGE:
        cap_all_charge_hiz();
        g_cap_ctx.threshold_stable_count = 0u;
        g_cap_ctx.live.timeout = 0u;
        g_cap_ctx.live.over = 0u;
        g_cap_ctx.live.elapsed_cycles = 0u;
        DWT->CYCCNT = 0u;
        g_cap_ctx.charge_start_cycles = cap_cycles_now();
        cap_discharge_off();
        cap_select_charge_range(g_cap_ctx.range);
        cap_enter_state(&g_cap_ctx, CAP_SM_CHARGING);
        break;

    case CAP_SM_CHARGING:
        err = cap_adc_read(&adc_raw);
        if (err != ERR_OK) {
            cap_all_charge_hiz();
            cap_discharge_on();
            g_cap_ctx.live.err = err;
            g_cap_ctx.live.adc_raw_last = 0u;
            cap_enter_state(&g_cap_ctx, CAP_SM_ERROR);
            break;
        }

        g_cap_ctx.live.err = ERR_OK;
        g_cap_ctx.live.adc_raw_last = adc_raw;
        if (adc_raw >= CAP_ADC_THRESHOLD_RAW) {
            if (g_cap_ctx.threshold_stable_count < 0xFFu) {
                g_cap_ctx.threshold_stable_count++;
            }
        } else {
            g_cap_ctx.threshold_stable_count = 0u;
        }

        if (g_cap_ctx.threshold_stable_count >= CAP_THRESHOLD_STABLE_COUNT) {
            g_cap_ctx.live.elapsed_cycles = (uint32_t)(cap_cycles_now() - g_cap_ctx.charge_start_cycles);
            cap_all_charge_hiz();
            cap_enter_state(&g_cap_ctx, CAP_SM_CAPTURED);
            break;
        }

        timeout_cycles = cap_cycles_from_us(cap_range_charge_timeout_us(g_cap_ctx.range));
        if (cap_cycles_elapsed(g_cap_ctx.charge_start_cycles, timeout_cycles)) {
            cap_all_charge_hiz();
            cap_discharge_on();
            g_cap_ctx.live.timeout = 1u;
            g_cap_ctx.live.over = 1u;
            g_cap_ctx.live.err = ERR_OVERRANGE;
            cap_enter_state(&g_cap_ctx, CAP_SM_TIMEOUT);
        }
        break;

    case CAP_SM_CAPTURED:
        r_ohm = cap_range_r_ohm(g_cap_ctx.range);
        denom = ((float)SystemCoreClock) * r_ohm * CAP_K_COEFF;
        if ((g_cap_ctx.live.elapsed_cycles == 0u) || (denom <= 0.0f)) {
            g_cap_ctx.live.err = ERR_HW_FAIL;
            cap_enter_state(&g_cap_ctx, CAP_SM_ERROR);
            break;
        }

        c_f = ((float)g_cap_ctx.live.elapsed_cycles) / denom;
        c_pf = c_f * 1.0e12f - CAP_OFFSET_PF;
        if (c_pf < 0.0f) {
            c_pf = 0.0f;
            c_f = 0.0f;
        }

        g_cap_ctx.last_good.valid = 1u;
        g_cap_ctx.last_good.over = 0u;
        g_cap_ctx.last_good.range = g_cap_ctx.range;
        g_cap_ctx.last_good.adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
        g_cap_ctx.last_good.elapsed_cycles = g_cap_ctx.live.elapsed_cycles;
        g_cap_ctx.last_good.adc_raw_last = g_cap_ctx.live.adc_raw_last;
        g_cap_ctx.last_good.value_f = c_f;
        g_cap_ctx.last_good.value_pf = c_pf;
        g_cap_ctx.last_good.value_nf = c_pf / 1000.0f;
        g_cap_ctx.last_good.value_uf = c_pf / 1000000.0f;
        g_cap_ctx.last_good.stat = CAP_STAT_OK;
        g_cap_ctx.last_good.err = ERR_OK;
        g_cap_ctx.last_good.empty_timeout = g_cap_ctx.live.empty_timeout;
        g_cap_ctx.last_good.timeout = 0u;
        g_cap_ctx.last_good.sm_state = CAP_SM_HOLD_RESULT;

        g_cap_ctx.live.over = 0u;
        g_cap_ctx.live.timeout = 0u;
        g_cap_ctx.live.err = ERR_OK;
        cap_enter_state(&g_cap_ctx, CAP_SM_HOLD_RESULT);
        break;

    case CAP_SM_HOLD_RESULT:
        timeout_cycles = cap_cycles_from_us(cap_range_hold_us(g_cap_ctx.range));
        if (cap_cycles_elapsed(g_cap_ctx.state_start_cycles, timeout_cycles)) {
            cap_discharge_on();
            cap_enter_state(&g_cap_ctx, CAP_SM_PRE_DISCHARGE);
        }
        break;

    case CAP_SM_TIMEOUT:
        cap_all_charge_hiz();
        cap_discharge_on();
        timeout_cycles = cap_cycles_from_us(CAP_TIMEOUT_RECOVER_US);
        if (cap_cycles_elapsed(g_cap_ctx.state_start_cycles, timeout_cycles)) {
            cap_enter_state(&g_cap_ctx, CAP_SM_PRE_DISCHARGE);
        }
        break;

    case CAP_SM_ERROR:
    default:
        cap_all_charge_hiz();
        cap_discharge_on();
        timeout_cycles = cap_cycles_from_us(CAP_ERROR_RECOVER_US);
        if (cap_cycles_elapsed(g_cap_ctx.state_start_cycles, timeout_cycles)) {
            cap_enter_state(&g_cap_ctx, CAP_SM_PRE_DISCHARGE);
        }
        break;
    }

    cap_update_output(out);
}

#endif
