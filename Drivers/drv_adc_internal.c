#include "drv_adc_internal.h"

#include <stdbool.h>

#include "../Core/Inc/main.h"

#define ADC_FILTER_SAMPLES 16u
#define ADC_POLL_TIMEOUT_MS 10u
#define ADC_FALLBACK_VDDA_MV 3300u

static ADC_HandleTypeDef g_hadc1;
static uint32_t g_active_channel = 0xFFFFFFFFu;
static app_err_t g_last_status = ERR_NOT_IMPL;
static bool g_adc_ready;

static app_err_t adc1_set_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};

    if (!g_adc_ready) {
        return ERR_HW_FAIL;
    }

    if (g_active_channel == channel) {
        return ERR_OK;
    }

    cfg.Channel = channel;
    cfg.Rank = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    cfg.SingleDiff = ADC_SINGLE_ENDED;
    cfg.OffsetNumber = ADC_OFFSET_NONE;
    cfg.Offset = 0;

    if (HAL_ADC_ConfigChannel(&g_hadc1, &cfg) != HAL_OK) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    g_active_channel = channel;
    return ERR_OK;
}

static app_err_t adc1_single_read(uint16_t *raw)
{
    uint32_t val;

    if ((raw == 0) || !g_adc_ready) {
        return ERR_INVALID_ARG;
    }

    __HAL_ADC_CLEAR_FLAG(&g_hadc1, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR);

    if (HAL_ADC_Start(&g_hadc1) != HAL_OK) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    if (HAL_ADC_PollForConversion(&g_hadc1, ADC_POLL_TIMEOUT_MS) != HAL_OK) {
        (void)HAL_ADC_Stop(&g_hadc1);
        g_last_status = ERR_ADC_TIMEOUT;
        return g_last_status;
    }

    val = HAL_ADC_GetValue(&g_hadc1);
    (void)HAL_ADC_Stop(&g_hadc1);

    if (__HAL_ADC_GET_FLAG(&g_hadc1, ADC_FLAG_OVR) != RESET) {
        __HAL_ADC_CLEAR_FLAG(&g_hadc1, ADC_FLAG_OVR);
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    *raw = (uint16_t)(val & 0xFFFFu);
    g_last_status = ERR_OK;
    return ERR_OK;
}

app_err_t adc1_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    RCC_PeriphCLKInitTypeDef periph_clk = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpio);

    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    periph_clk.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    g_hadc1.Instance = ADC1;
    g_hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    g_hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    g_hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_hadc1.Init.GainCompensation = 0u;
    g_hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    g_hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    g_hadc1.Init.LowPowerAutoWait = DISABLE;
    g_hadc1.Init.ContinuousConvMode = DISABLE;
    g_hadc1.Init.NbrOfConversion = 1u;
    g_hadc1.Init.DiscontinuousConvMode = DISABLE;
    g_hadc1.Init.NbrOfDiscConversion = 1u;
    g_hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    g_hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_hadc1.Init.SamplingMode = ADC_SAMPLING_MODE_NORMAL;
    g_hadc1.Init.DMAContinuousRequests = DISABLE;
    g_hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    g_hadc1.Init.OversamplingMode = DISABLE;

    if (HAL_ADC_Init(&g_hadc1) != HAL_OK) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    if (HAL_ADCEx_Calibration_Start(&g_hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    g_active_channel = 0xFFFFFFFFu;
    g_adc_ready = true;
    g_last_status = ERR_OK;
    return ERR_OK;
}

app_err_t adc1_read_raw_u16(uint16_t *raw)
{
    app_err_t err;

    if (raw == 0) {
        return ERR_INVALID_ARG;
    }

    err = adc1_set_channel(ADC_CHANNEL_6);
    if (err != ERR_OK) {
        return err;
    }

    return adc1_single_read(raw);
}

app_err_t adc1_read_vdda_mv(uint32_t *vdda_mv)
{
    app_err_t err;
    uint16_t raw;
    uint32_t vdda;

    if (vdda_mv == 0) {
        return ERR_INVALID_ARG;
    }

    err = adc1_set_channel(ADC_CHANNEL_VREFINT);
    if (err != ERR_OK) {
        return err;
    }

    err = adc1_single_read(&raw);
    if (err != ERR_OK) {
        return err;
    }

    if (raw == 0u) {
        g_last_status = ERR_HW_FAIL;
        return g_last_status;
    }

    vdda = __HAL_ADC_CALC_VREFANALOG_VOLTAGE((uint32_t)raw, ADC_RESOLUTION_12B);
    *vdda_mv = vdda;
    return ERR_OK;
}

app_err_t adc1_read_mv(uint32_t *mv)
{
    app_err_t err;
    app_err_t vdda_err;
    uint16_t raw;
    uint32_t vdda_mv = ADC_FALLBACK_VDDA_MV;
    app_err_t status_before_vdda;

    if (mv == 0) {
        return ERR_INVALID_ARG;
    }

    err = adc1_read_raw_u16(&raw);
    if (err != ERR_OK) {
        return err;
    }

    status_before_vdda = g_last_status;
    vdda_err = adc1_read_vdda_mv(&vdda_mv);
    if (vdda_err != ERR_OK) {
        vdda_mv = ADC_FALLBACK_VDDA_MV;
        g_last_status = status_before_vdda;
    }

    *mv = __HAL_ADC_CALC_DATA_TO_VOLTAGE(vdda_mv, (uint32_t)raw, ADC_RESOLUTION_12B);
    return ERR_OK;
}

app_err_t adc1_read_filtered(uint16_t *raw, uint32_t *mv)
{
    uint16_t samples[ADC_FILTER_SAMPLES];
    uint32_t i;
    uint32_t sum = 0u;
    uint16_t min_v = 0xFFFFu;
    uint16_t max_v = 0u;
    uint32_t mean_raw;
    uint32_t vdda_mv = ADC_FALLBACK_VDDA_MV;
    app_err_t err;
    app_err_t vdda_err;
    app_err_t status_before_vdda;

    if ((raw == 0) || (mv == 0)) {
        return ERR_INVALID_ARG;
    }

    for (i = 0u; i < ADC_FILTER_SAMPLES; i++) {
        err = adc1_read_raw_u16(&samples[i]);
        if (err != ERR_OK) {
            return err;
        }

        if (samples[i] < min_v) {
            min_v = samples[i];
        }
        if (samples[i] > max_v) {
            max_v = samples[i];
        }
        sum += samples[i];
    }

    mean_raw = (sum - (uint32_t)min_v - (uint32_t)max_v) / (ADC_FILTER_SAMPLES - 2u);
    *raw = (uint16_t)mean_raw;

    status_before_vdda = g_last_status;
    vdda_err = adc1_read_vdda_mv(&vdda_mv);
    if (vdda_err != ERR_OK) {
        vdda_mv = ADC_FALLBACK_VDDA_MV;
        g_last_status = status_before_vdda;
    }

    *mv = __HAL_ADC_CALC_DATA_TO_VOLTAGE(vdda_mv, mean_raw, ADC_RESOLUTION_12B);
    return ERR_OK;
}

app_err_t adc1_read_status(void)
{
    return g_last_status;
}
