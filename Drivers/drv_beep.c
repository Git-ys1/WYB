#include "drv_beep.h"

#include "../BSP/bsp.h"
#include "../Core/Inc/main.h"

#include "stm32g4xx_hal.h"

typedef enum {
    BEEP_STATE_IDLE = 0,
    BEEP_STATE_ONE_SHOT,
    BEEP_STATE_CONTINUOUS,
    BEEP_STATE_PATTERN
} beep_state_t;

#define BEEP_BACKEND_ACTIVE 1u
#define BEEP_ACTIVE_LOW 1u

#if !BEEP_BACKEND_ACTIVE
static uint32_t g_freq_hz = 2700u;
#endif
static uint32_t g_until_ms;
static uint32_t g_next_ms;
static uint8_t g_step;
static beep_state_t g_state;

static void beep_hw_init(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    init.Pin = BEEP_Pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_Port, &init);
}

static void beep_hw_set(bool on)
{
#if BEEP_BACKEND_ACTIVE
    GPIO_PinState ps;

    if (BEEP_ACTIVE_LOW) {
        ps = on ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        ps = on ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, ps);
#else
    if (on) {
        (void)bsp_pwm_start(BSP_PWM_BEEP, g_freq_hz, 50u);
    } else {
        bsp_pwm_stop(BSP_PWM_BEEP);
    }
#endif
}

void beep_init(uint32_t freq_hz)
{
#if !BEEP_BACKEND_ACTIVE
    if (freq_hz > 0u) {
        g_freq_hz = freq_hz;
    }
#else
    (void)freq_hz;
#endif
    g_until_ms = 0u;
    g_next_ms = 0u;
    g_step = 0u;
    g_state = BEEP_STATE_IDLE;
    beep_hw_init();
    beep_hw_set(false);
}

void beep_once(uint32_t duration_ms)
{
    if (duration_ms == 0u) {
        return;
    }

    g_state = BEEP_STATE_ONE_SHOT;
    g_until_ms = bsp_millis() + duration_ms;
    beep_hw_set(true);
}

void beep_continuous(bool on)
{
    if (on) {
        g_state = BEEP_STATE_CONTINUOUS;
        beep_hw_set(true);
    } else {
        g_state = BEEP_STATE_IDLE;
        beep_hw_set(false);
    }
}

void beep_pattern(beep_pattern_t pattern)
{
    uint32_t now = bsp_millis();

    g_state = BEEP_STATE_PATTERN;
    g_step = 0u;
    g_next_ms = now;

    if (pattern == BEEP_PATTERN_OK) {
        g_until_ms = 1u;
    } else if (pattern == BEEP_PATTERN_ALERT) {
        g_until_ms = 2u;
    } else {
        g_state = BEEP_STATE_IDLE;
    }
}

void beep_tick(void)
{
    uint32_t now = bsp_millis();

    if (g_state == BEEP_STATE_ONE_SHOT) {
        if ((int32_t)(now - g_until_ms) >= 0) {
            g_state = BEEP_STATE_IDLE;
            beep_hw_set(false);
        }
        return;
    }

    if (g_state == BEEP_STATE_CONTINUOUS) {
        return;
    }

    if (g_state == BEEP_STATE_PATTERN) {
        if ((int32_t)(now - g_next_ms) < 0) {
            return;
        }

        if (g_until_ms == 1u) {
            if (g_step == 0u) {
                beep_hw_set(true);
                g_next_ms = now + 80u;
                g_step = 1u;
            } else {
                beep_hw_set(false);
                g_state = BEEP_STATE_IDLE;
            }
        } else if (g_until_ms == 2u) {
            if ((g_step % 2u) == 0u) {
                beep_hw_set(true);
                g_next_ms = now + 120u;
            } else {
                beep_hw_set(false);
                g_next_ms = now + 80u;
            }
            g_step++;
            if (g_step >= 6u) {
                g_state = BEEP_STATE_IDLE;
                beep_hw_set(false);
            }
        }
        return;
    }

    beep_hw_set(false);
}

