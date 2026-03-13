#ifndef MEASURE_CAP_H
#define MEASURE_CAP_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../Drivers/drv_adc_internal.h"
#include "../Drivers/drv_error.h"
#include "stm32g4xx_hal.h"

typedef enum {
    CAP_RANGE_20N = 0,
    CAP_RANGE_2U,
    CAP_RANGE_200U,
    CAP_RANGE_COUNT
} cap_range_t;

typedef enum {
    CAP_STAT_PROBE = 0,
    CAP_STAT_OK,
    CAP_STAT_OL,
    CAP_STAT_ERR
} cap_stat_t;

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
} cap_result_t;

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

#define CAP_DISCHARGE_ADC_LOW_RAW 16u
#define CAP_DISCHARGE_WAIT_US 50000u

#define CAP_TIMEOUT_20N_US 5000u
#define CAP_TIMEOUT_2U_US 50000u
#define CAP_TIMEOUT_200U_US 500000u

static cap_range_t g_cap_range = CAP_RANGE_20N;
static bool g_cap_entered = false;

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

static inline void cap_select_charge_range(cap_range_t range)
{
    cap_all_charge_hiz();
    switch (range) {
    case CAP_RANGE_20N:
        cap_gpio_as_output_high(CAP_CHG_20N_GPIO_Port, CAP_CHG_20N_Pin);
        break;
    case CAP_RANGE_2U:
        cap_gpio_as_output_high(CAP_CHG_2U_GPIO_Port, CAP_CHG_2U_Pin);
        break;
    case CAP_RANGE_200U:
    default:
        cap_gpio_as_output_high(CAP_CHG_200U_GPIO_Port, CAP_CHG_200U_Pin);
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

static inline app_err_t cap_adc_read_once(uint16_t *raw)
{
    return adc1_read_opamp1_raw_u16(raw);
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
    case CAP_STAT_OK:
        return "OK";
    case CAP_STAT_OL:
        return "OL";
    case CAP_STAT_ERR:
        return "ERR";
    case CAP_STAT_PROBE:
    default:
        return "PROBE";
    }
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

static inline uint32_t cap_range_timeout_us(cap_range_t range)
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

static inline void cap_result_reset(cap_result_t *out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->range = g_cap_range;
    out->adc_threshold = (uint32_t)CAP_ADC_THRESHOLD_RAW;
    out->stat = CAP_STAT_PROBE;
    out->err = ERR_NOT_IMPL;
}

static inline void cap_enter(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    cap_dwt_enable();
    cap_all_charge_hiz();
    cap_discharge_on();
    adc1_mark_input_path_changed();
    g_cap_entered = true;
}

static inline void cap_leave(void)
{
    cap_all_charge_hiz();
    cap_discharge_on();
    g_cap_entered = false;
}

static inline void cap_set_range(cap_range_t range)
{
    if (range >= CAP_RANGE_COUNT) {
        return;
    }
    g_cap_range = range;
}

static inline void cap_measure_once(cap_result_t *out)
{
    uint32_t discharge_start;
    uint32_t discharge_limit;
    uint32_t start_cycles;
    uint32_t timeout_cycles;
    uint32_t elapsed_cycles = 0u;
    uint16_t adc_raw = 0u;
    app_err_t err = ERR_OK;
    bool reached = false;
    float r_ohm;
    float denom;
    float c_f;
    float c_pf;

    cap_result_reset(out);
    if (out == NULL) {
        return;
    }

    if (!g_cap_entered) {
        out->stat = CAP_STAT_ERR;
        out->err = ERR_NOT_IMPL;
        return;
    }

    out->range = g_cap_range;
    cap_all_charge_hiz();
    cap_discharge_on();
    adc1_mark_input_path_changed();

    discharge_start = cap_cycles_now();
    discharge_limit = cap_cycles_from_us(CAP_DISCHARGE_WAIT_US);
    while (!cap_cycles_elapsed(discharge_start, discharge_limit)) {
        err = cap_adc_read_once(&adc_raw);
        if (err != ERR_OK) {
            out->stat = CAP_STAT_ERR;
            out->err = err;
            out->adc_raw_last = 0u;
            return;
        }
        out->adc_raw_last = adc_raw;
        if (adc_raw <= (uint16_t)CAP_DISCHARGE_ADC_LOW_RAW) {
            break;
        }
    }

    cap_select_charge_range(g_cap_range);
    cap_discharge_off();
    start_cycles = cap_cycles_now();
    timeout_cycles = cap_cycles_from_us(cap_range_timeout_us(g_cap_range));

    while (!cap_cycles_elapsed(start_cycles, timeout_cycles)) {
        err = cap_adc_read_once(&adc_raw);
        if (err != ERR_OK) {
            cap_all_charge_hiz();
            cap_discharge_on();
            out->stat = CAP_STAT_ERR;
            out->err = err;
            out->adc_raw_last = 0u;
            return;
        }
        if (adc_raw >= CAP_ADC_THRESHOLD_RAW) {
            elapsed_cycles = (uint32_t)(cap_cycles_now() - start_cycles);
            reached = true;
            break;
        }
    }

    cap_all_charge_hiz();
    cap_discharge_on();

    out->adc_raw_last = adc_raw;
    out->elapsed_cycles = elapsed_cycles;

    if (!reached) {
        out->valid = 0u;
        out->over = 1u;
        out->stat = CAP_STAT_OL;
        out->err = ERR_OVERRANGE;
        return;
    }

    r_ohm = cap_range_r_ohm(g_cap_range);
    denom = ((float)SystemCoreClock) * r_ohm * CAP_K_COEFF;
    if (denom <= 0.0f) {
        out->valid = 0u;
        out->over = 0u;
        out->stat = CAP_STAT_ERR;
        out->err = ERR_HW_FAIL;
        return;
    }

    c_f = ((float)elapsed_cycles) / denom;
    c_pf = c_f * 1.0e12f - CAP_OFFSET_PF;
    if (c_pf < 0.0f) {
        c_pf = 0.0f;
        c_f = 0.0f;
    }

    out->value_f = c_f;
    out->value_pf = c_pf;
    out->value_nf = c_pf / 1000.0f;
    out->value_uf = c_pf / 1000000.0f;
    out->valid = 1u;
    out->over = 0u;
    out->stat = CAP_STAT_OK;
    out->err = ERR_OK;
}

#endif
