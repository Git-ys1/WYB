#include "bsp.h"

#if BSP_USE_HAL_PORT

#include <string.h>

#include "../Core/Inc/main.h"

extern I2C_HandleTypeDef hi2c2;
extern I2C_HandleTypeDef hi2c3;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim16;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_desc_t;

static const pin_desc_t k_pin_desc[BSP_PIN_COUNT] = {
    [BSP_PIN_RES_A] = {RES_MODE_A_GPIO_Port, RES_MODE_A_Pin},
    [BSP_PIN_RES_B] = {RES_MODE_B_GPIO_Port, RES_MODE_B_Pin},
    [BSP_PIN_RES_C] = {RES_MODE_C_GPIO_Port, RES_MODE_C_Pin},
    [BSP_PIN_MODE_A] = {CHANNLE_SELEC_A_GPIO_Port, CHANNLE_SELEC_A_Pin},
    [BSP_PIN_MODE_B] = {CHANNLE_SELEC_B_GPIO_Port, CHANNLE_SELEC_B_Pin},
    [BSP_PIN_MODE_C] = {CHANNLE_SELEC_C_GPIO_Port, CHANNLE_SELEC_C_Pin},
    [BSP_PIN_VOLT_A] = {VOLTAGE_MODE_A_GPIO_Port, VOLTAGE_MODE_A_Pin},
    [BSP_PIN_VOLT_B] = {VOLTAGE_MODE_B_GPIO_Port, VOLTAGE_MODE_B_Pin},
    [BSP_PIN_KEY] = {KEY_GPIO_Port, KEY_Pin},
    [BSP_PIN_BEEP] = {BEEP_GPIO_Port, BEEP_Pin}
};

static volatile uint32_t g_cap_period_ticks;
static volatile uint32_t g_cap_high_ticks;
static volatile uint8_t g_cap_have_period;
static volatile uint8_t g_cap_have_high;
static volatile uint8_t g_cap_valid;
static volatile uint32_t g_cap_last_ms;
static volatile uint32_t g_key_edge_ms;
static uint8_t g_cap_started;

static uint32_t g_tim2_clk_hz;
static uint32_t g_tim2_tick_hz;

typedef struct {
    uint8_t enabled;
    uint8_t level;
    uint32_t half_period_ms;
    uint32_t next_toggle_ms;
} sw_pwm_t;

static uint8_t g_pwm_hw_active;
static sw_pwm_t g_sw_pwm;
static bsp_oled_bus_mode_t g_oled_bus_mode;
static uint32_t g_oled_nack_count;
static uint32_t g_oled_timeout_count;

#define I2C2_TIMING_100KHZ_16MHZ 0x20303E5Du
#define I2C2_TIMING_400KHZ_16MHZ 0x0010061Au
#define I2C2_RECOVERY_PULSES 9u
#define I2C2_RECOVERY_DELAY_NOP 64u
#define SOFT_I2C_DELAY_NOP 96u

static I2C_HandleTypeDef *i2c_handle_from_bus(bsp_i2c_bus_t bus)
{
    if (bus == BSP_I2C_BUS_OLED) {
        return &hi2c2;
    }
    if (bus == BSP_I2C_BUS_ADS) {
        return &hi2c3;
    }
    return 0;
}

static void beep_pin_to_gpio_output(void)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = BEEP_Pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_Port, &init);
}

static void beep_pin_to_tim16_af(void)
{
    GPIO_InitTypeDef init = {0};

    init.Pin = BEEP_Pin;
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = GPIO_AF1_TIM16;
    HAL_GPIO_Init(BEEP_GPIO_Port, &init);
}

static void sw_pwm_stop(void)
{
    g_sw_pwm.enabled = 0u;
    g_sw_pwm.level = 0u;
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

static void sw_pwm_start(uint32_t freq_hz)
{
    uint32_t half_period_ms;

    if (freq_hz == 0u) {
        half_period_ms = 1u;
    } else {
        half_period_ms = 1000u / (freq_hz * 2u);
        if (half_period_ms == 0u) {
            half_period_ms = 1u;
        }
    }

    beep_pin_to_gpio_output();
    g_sw_pwm.enabled = 1u;
    g_sw_pwm.level = 0u;
    g_sw_pwm.half_period_ms = half_period_ms;
    g_sw_pwm.next_toggle_ms = HAL_GetTick() + half_period_ms;
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

static void sw_pwm_update(uint32_t now_ms)
{
    if (!g_sw_pwm.enabled) {
        return;
    }

    if ((int32_t)(now_ms - g_sw_pwm.next_toggle_ms) >= 0) {
        g_sw_pwm.level ^= 1u;
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, g_sw_pwm.level ? GPIO_PIN_SET : GPIO_PIN_RESET);
        g_sw_pwm.next_toggle_ms = now_ms + g_sw_pwm.half_period_ms;
    }
}

static uint32_t tim2_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    RCC_ClkInitTypeDef clk = {0};
    uint32_t flash_latency;

    HAL_RCC_GetClockConfig(&clk, &flash_latency);
    if (clk.APB1CLKDivider == RCC_HCLK_DIV1) {
        return pclk1;
    }
    return pclk1 * 2u;
}

static bool i2c_retime_and_init(I2C_HandleTypeDef *hi2c, uint32_t timing)
{
    if (hi2c == 0) {
        return false;
    }

    if (HAL_I2C_DeInit(hi2c) != HAL_OK) {
        return false;
    }

    hi2c->Init.Timing = timing;
    if (HAL_I2C_Init(hi2c) != HAL_OK) {
        return false;
    }

    if (HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        return false;
    }

    if (HAL_I2CEx_ConfigDigitalFilter(hi2c, 0u) != HAL_OK) {
        return false;
    }

    return true;
}

static void i2c2_recover_delay(void)
{
    volatile uint32_t i;
    for (i = 0u; i < I2C2_RECOVERY_DELAY_NOP; i++) {
        __NOP();
    }
}

static void i2c2_bus_to_gpio_od(void)
{
    GPIO_InitTypeDef init = {0};

    init.Mode = GPIO_MODE_OUTPUT_OD;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;

    init.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOC, &init);
    init.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &init);
}

static void i2c2_bus_to_af4(void)
{
    GPIO_InitTypeDef init = {0};

    init.Mode = GPIO_MODE_AF_OD;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = GPIO_AF4_I2C2;

    init.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOC, &init);
    init.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &init);
}

static void soft_i2c_delay(void)
{
    volatile uint32_t i;
    for (i = 0u; i < SOFT_I2C_DELAY_NOP; i++) {
        __NOP();
    }
}

static void soft_i2c_scl(uint8_t high)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    soft_i2c_delay();
}

static void soft_i2c_sda(uint8_t high)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    soft_i2c_delay();
}

static uint8_t soft_i2c_sda_read(void)
{
    return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_SET) ? 1u : 0u;
}

static void soft_i2c_start(void)
{
    soft_i2c_sda(1u);
    soft_i2c_scl(1u);
    soft_i2c_sda(0u);
    soft_i2c_scl(0u);
}

static void soft_i2c_stop(void)
{
    soft_i2c_sda(0u);
    soft_i2c_scl(1u);
    soft_i2c_sda(1u);
}

static uint8_t soft_i2c_write_byte(uint8_t byte)
{
    uint8_t bit;

    for (bit = 0u; bit < 8u; bit++) {
        soft_i2c_sda((byte & 0x80u) ? 1u : 0u);
        soft_i2c_scl(1u);
        soft_i2c_scl(0u);
        byte <<= 1u;
    }

    soft_i2c_sda(1u);
    soft_i2c_scl(1u);
    bit = (uint8_t)(soft_i2c_sda_read() == 0u);
    soft_i2c_scl(0u);
    return bit;
}

static bool soft_i2c_probe_addr(uint8_t addr7)
{
    uint8_t ack;

    soft_i2c_start();
    ack = soft_i2c_write_byte((uint8_t)(addr7 << 1u));
    soft_i2c_stop();
    return ack != 0u;
}

static bool soft_i2c_write_packet(uint8_t addr7, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if ((data == 0) || (len == 0u)) {
        return false;
    }

    soft_i2c_start();
    if (soft_i2c_write_byte((uint8_t)(addr7 << 1u)) == 0u) {
        soft_i2c_stop();
        return false;
    }

    for (i = 0u; i < len; i++) {
        if (soft_i2c_write_byte(data[i]) == 0u) {
            soft_i2c_stop();
            return false;
        }
    }

    soft_i2c_stop();
    return true;
}

static void oled_note_i2c_error(I2C_HandleTypeDef *hi2c)
{
    uint32_t err;

    if (hi2c == 0) {
        g_oled_nack_count++;
        return;
    }

    err = HAL_I2C_GetError(hi2c);
    if ((err & HAL_I2C_ERROR_TIMEOUT) != 0u) {
        g_oled_timeout_count++;
    } else {
        g_oled_nack_count++;
    }
}

bool bsp_oled_bus_set_mode(bsp_oled_bus_mode_t mode)
{
    if (mode == g_oled_bus_mode) {
        return true;
    }

    if (mode == BSP_OLED_BUS_SOFT_I2C) {
        (void)HAL_I2C_DeInit(&hi2c2);
        i2c2_bus_to_gpio_od();
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        g_oled_bus_mode = BSP_OLED_BUS_SOFT_I2C;
        return true;
    }

    i2c2_bus_to_af4();
    if (!i2c_retime_and_init(&hi2c2, I2C2_TIMING_100KHZ_16MHZ)) {
        return false;
    }
    g_oled_bus_mode = BSP_OLED_BUS_HW_I2C2;
    return true;
}

bsp_oled_bus_mode_t bsp_oled_bus_get_mode(void)
{
    return g_oled_bus_mode;
}

void bsp_oled_bus_reset_stats(void)
{
    g_oled_nack_count = 0u;
    g_oled_timeout_count = 0u;
}

uint32_t bsp_oled_bus_get_nack_count(void)
{
    return g_oled_nack_count;
}

void bsp_init(void)
{
    g_cap_period_ticks = 0u;
    g_cap_high_ticks = 0u;
    g_cap_have_period = 0u;
    g_cap_have_high = 0u;
    g_cap_valid = 0u;
    g_cap_last_ms = 0u;
    g_key_edge_ms = 0u;
    g_cap_started = 0u;

    g_pwm_hw_active = 0u;
    g_sw_pwm.enabled = 0u;
    g_sw_pwm.level = 0u;
    g_sw_pwm.half_period_ms = 1u;
    g_sw_pwm.next_toggle_ms = 0u;
    g_oled_bus_mode = BSP_OLED_BUS_HW_I2C2;
    bsp_oled_bus_reset_stats();

    g_tim2_clk_hz = tim2_clock_hz();
    g_tim2_tick_hz = g_tim2_clk_hz / ((uint32_t)htim2.Init.Prescaler + 1u);
    if (g_tim2_tick_hz == 0u) {
        g_tim2_tick_hz = 1u;
    }

#if BSP_I2C2_FAST_400K
    if (!i2c_retime_and_init(&hi2c2, I2C2_TIMING_400KHZ_16MHZ)) {
        (void)i2c_retime_and_init(&hi2c2, I2C2_TIMING_100KHZ_16MHZ);
        bsp_debug_log("I2C2 FAST FAIL -> 100K");
    } else {
        bsp_debug_log("I2C2 FAST 400K");
    }
#endif

}

uint32_t bsp_millis(void)
{
    uint32_t now = HAL_GetTick();
    (void)g_key_edge_ms;
    sw_pwm_update(now);
    return now;
}

void bsp_delay_ms(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
    sw_pwm_update(HAL_GetTick());
}

bool bsp_i2c_write(bsp_i2c_bus_t bus, uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *hi2c;
    HAL_StatusTypeDef st;

    if ((data == 0) || (len == 0u)) {
        return false;
    }

    if (bus == BSP_I2C_BUS_OLED) {
        if (g_oled_bus_mode == BSP_OLED_BUS_SOFT_I2C) {
            if (!soft_i2c_write_packet(addr7, data, len)) {
                g_oled_nack_count++;
                return false;
            }
            return true;
        }

        st = HAL_I2C_Master_Transmit(&hi2c2, (uint16_t)(addr7 << 1u), (uint8_t *)data, len, timeout_ms);
        if (st == HAL_OK) {
            return true;
        }

        oled_note_i2c_error(&hi2c2);
        return false;
    }

    hi2c = i2c_handle_from_bus(bus);
    if (hi2c == 0) {
        return false;
    }
    return HAL_I2C_Master_Transmit(hi2c, (uint16_t)(addr7 << 1u), (uint8_t *)data, len, timeout_ms) == HAL_OK;
}

bool bsp_i2c_read(bsp_i2c_bus_t bus, uint8_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *hi2c;

    if ((data == 0) || (len == 0u)) {
        return false;
    }

    if (bus == BSP_I2C_BUS_OLED) {
        if (g_oled_bus_mode == BSP_OLED_BUS_SOFT_I2C) {
            g_oled_nack_count++;
            return false;
        }

        if (HAL_I2C_Master_Receive(&hi2c2, (uint16_t)(addr7 << 1u), data, len, timeout_ms) == HAL_OK) {
            return true;
        }
        oled_note_i2c_error(&hi2c2);
        return false;
    }

    hi2c = i2c_handle_from_bus(bus);
    if (hi2c == 0) {
        return false;
    }

    return HAL_I2C_Master_Receive(hi2c, (uint16_t)(addr7 << 1u), data, len, timeout_ms) == HAL_OK;
}

bool bsp_i2c_probe(bsp_i2c_bus_t bus, uint8_t addr7, uint32_t timeout_ms)
{
    I2C_HandleTypeDef *hi2c;
    uint32_t trials;
    HAL_StatusTypeDef st;

    if (bus == BSP_I2C_BUS_OLED) {
        trials = 5u;

        if (g_oled_bus_mode == BSP_OLED_BUS_SOFT_I2C) {
            if (!soft_i2c_probe_addr(addr7)) {
                g_oled_nack_count++;
                return false;
            }
            return true;
        }

        st = HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr7 << 1u), trials, timeout_ms);
        if (st == HAL_OK) {
            return true;
        }

        oled_note_i2c_error(&hi2c2);
        return false;
    }

    hi2c = i2c_handle_from_bus(bus);

    if (hi2c == 0) {
        return false;
    }

    trials = 2u;
    return HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr7 << 1u), (uint32_t)trials, timeout_ms) == HAL_OK;
}

bool bsp_i2c2_bus_recover(void)
{
    uint8_t i;

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    (void)HAL_I2C_DeInit(&hi2c2);

    i2c2_bus_to_gpio_od();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    i2c2_recover_delay();

    for (i = 0u; i < I2C2_RECOVERY_PULSES; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
        i2c2_recover_delay();
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
        i2c2_recover_delay();
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    i2c2_recover_delay();
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    i2c2_recover_delay();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    i2c2_recover_delay();

    i2c2_bus_to_af4();
    return true;
}

bool bsp_i2c2_reinit_100k(void)
{
    i2c2_bus_to_af4();
    if (!i2c_retime_and_init(&hi2c2, I2C2_TIMING_100KHZ_16MHZ)) {
        return false;
    }
    g_oled_bus_mode = BSP_OLED_BUS_HW_I2C2;
    return true;
}

void bsp_gpio_write(bsp_pin_t pin, bool level)
{
    if (pin >= BSP_PIN_COUNT) {
        return;
    }

    HAL_GPIO_WritePin(k_pin_desc[pin].port, k_pin_desc[pin].pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool bsp_gpio_read(bsp_pin_t pin)
{
    if (pin >= BSP_PIN_COUNT) {
        return false;
    }

    return HAL_GPIO_ReadPin(k_pin_desc[pin].port, k_pin_desc[pin].pin) == GPIO_PIN_SET;
}

bool bsp_pwm_start(bsp_pwm_t pwm, uint32_t freq_hz, uint8_t duty_pct)
{
    TIM_OC_InitTypeDef cfg = {0};
    uint32_t pclk2;
    uint32_t hclk;
    uint32_t tim_clk;
    uint32_t prescaler;
    uint32_t period;

    if ((pwm != BSP_PWM_BEEP) || (freq_hz == 0u)) {
        return false;
    }

    if (duty_pct > 100u) {
        duty_pct = 100u;
    }

    g_pwm_hw_active = 0u;
    sw_pwm_stop();

    pclk2 = HAL_RCC_GetPCLK2Freq();
    hclk = HAL_RCC_GetHCLKFreq();
    tim_clk = (pclk2 == hclk) ? pclk2 : (pclk2 * 2u);

    prescaler = tim_clk / (freq_hz * 65536u);
    if (prescaler > 0xFFFFu) {
        sw_pwm_start(freq_hz);
        return true;
    }

    period = tim_clk / ((prescaler + 1u) * freq_hz);
    if (period == 0u) {
        sw_pwm_start(freq_hz);
        return true;
    }
    period -= 1u;
    if (period > 0xFFFFu) {
        sw_pwm_start(freq_hz);
        return true;
    }

    htim16.Init.Prescaler = prescaler;
    htim16.Init.Period = period;

    if (HAL_TIM_PWM_Init(&htim16) != HAL_OK) {
        sw_pwm_start(freq_hz);
        return true;
    }

    cfg.OCMode = TIM_OCMODE_PWM1;
    cfg.Pulse = ((period + 1u) * duty_pct) / 100u;
    cfg.OCPolarity = TIM_OCPOLARITY_HIGH;
    cfg.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    cfg.OCFastMode = TIM_OCFAST_DISABLE;
    cfg.OCIdleState = TIM_OCIDLESTATE_RESET;
    cfg.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    if (HAL_TIM_PWM_ConfigChannel(&htim16, &cfg, TIM_CHANNEL_1) != HAL_OK) {
        sw_pwm_start(freq_hz);
        return true;
    }

    beep_pin_to_tim16_af();

    if (HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1) != HAL_OK) {
        sw_pwm_start(freq_hz);
        return true;
    }

    g_pwm_hw_active = 1u;
    return true;
}

void bsp_pwm_stop(bsp_pwm_t pwm)
{
    if (pwm != BSP_PWM_BEEP) {
        return;
    }

    if (g_pwm_hw_active) {
        (void)HAL_TIM_PWM_Stop(&htim16, TIM_CHANNEL_1);
        g_pwm_hw_active = 0u;
    }

    sw_pwm_stop();
}

bool bsp_freq_get_capture(bsp_capture_t *capture)
{
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint8_t valid;
    uint32_t last_ms;
    uint64_t period_us;
    uint64_t high_us;

    if (capture == 0) {
        return false;
    }

    __disable_irq();
    period_ticks = g_cap_period_ticks;
    high_ticks = g_cap_high_ticks;
    valid = g_cap_valid;
    last_ms = g_cap_last_ms;
    __enable_irq();

    if (!valid || (g_tim2_tick_hz == 0u)) {
        return false;
    }

    if ((HAL_GetTick() - last_ms) > 500u) {
        return false;
    }

    period_us = ((uint64_t)period_ticks * 1000000ull + (uint64_t)(g_tim2_tick_hz / 2u)) / (uint64_t)g_tim2_tick_hz;
    high_us = ((uint64_t)high_ticks * 1000000ull + (uint64_t)(g_tim2_tick_hz / 2u)) / (uint64_t)g_tim2_tick_hz;

    if ((period_us == 0ull) || (high_us > period_us)) {
        return false;
    }

    capture->period_us = (uint32_t)period_us;
    capture->high_us = (uint32_t)high_us;
    capture->valid = true;
    return true;
}

void bsp_freq_capture_start(void)
{
    if (g_cap_started) {
        return;
    }

    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK) {
        return;
    }
    if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2) != HAL_OK) {
        (void)HAL_TIM_IC_Stop_IT(&htim2, TIM_CHANNEL_1);
        return;
    }

    g_cap_started = 1u;
}

void bsp_debug_log(const char *msg)
{
    (void)msg;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    uint32_t cap;

    if ((htim == 0) || (htim->Instance != TIM2)) {
        return;
    }

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        cap = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        if (cap > 0u) {
            g_cap_period_ticks = cap;
            g_cap_have_period = 1u;
        }
    } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        cap = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
        g_cap_high_ticks = cap;
        g_cap_have_high = 1u;
    }

    if (g_cap_have_period && g_cap_have_high && (g_cap_period_ticks > 0u) && (g_cap_high_ticks <= g_cap_period_ticks)) {
        g_cap_valid = 1u;
        g_cap_last_ms = HAL_GetTick();
    } else if (g_cap_high_ticks > g_cap_period_ticks) {
        g_cap_valid = 0u;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    if (gpio_pin == KEY_Pin) {
        g_key_edge_ms = HAL_GetTick();
    }
}

#endif
