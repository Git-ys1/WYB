#include "app_heartbeat.h"

#include <stdbool.h>

#include "../BSP/bsp_rgb.h"
#include "../Core/Inc/main.h"

#define HB_TICK_MS 50u
#define HB_STALL_TIMEOUT_TICKS 20u
#define HB_PERIOD_SLOW_MS 1000u
#define HB_PERIOD_MID_MS 400u
#define HB_PERIOD_FAST_MS 150u
#define HB_SELFTEST_STEP_TICKS 2u

typedef enum {
    HB_STATE_BOOT = 0,
    HB_STATE_RUN,
    HB_STATE_FAULT
} hb_state_t;

static TIM_HandleTypeDef s_htim6;
static volatile uint32_t s_kick_counter;
static volatile uint32_t s_kick_last;
static volatile uint16_t s_stall_ticks;
static volatile uint8_t s_force_fault;
static volatile uint8_t s_green_on;
static volatile uint16_t s_blink_period_ticks;
static volatile uint16_t s_blink_ticks;
static volatile hb_state_t s_state;
static volatile uint8_t s_selftest_step;
static volatile uint8_t s_selftest_done;
static volatile uint8_t s_selftest_tick_acc;
static uint8_t s_inited;

static uint16_t ms_to_ticks(uint16_t period_ms)
{
    uint32_t ticks = ((uint32_t)period_ms + (HB_TICK_MS - 1u)) / HB_TICK_MS;
    if (ticks == 0u) {
        ticks = 1u;
    }
    return (uint16_t)ticks;
}

static void hb_apply_led(void)
{
    if (s_state == HB_STATE_RUN) {
        bsp_rgb_set(0u, s_green_on, 0u);
        return;
    }

    bsp_rgb_set(1u, 0u, 0u);
}

static void hb_run_selftest_tick(void)
{
    if (s_selftest_done) {
        return;
    }

    if (s_selftest_step == 0u) {
        bsp_rgb_set(0u, 0u, 1u);
    } else if (s_selftest_step == 1u) {
        bsp_rgb_set(1u, 0u, 0u);
    } else if (s_selftest_step == 2u) {
        bsp_rgb_set(0u, 1u, 0u);
    } else {
        s_selftest_done = 1u;
        hb_apply_led();
        return;
    }

    s_selftest_tick_acc++;
    if (s_selftest_tick_acc >= HB_SELFTEST_STEP_TICKS) {
        s_selftest_tick_acc = 0u;
        s_selftest_step++;
        if (s_selftest_step > 2u) {
            s_selftest_done = 1u;
            hb_apply_led();
        }
    }
}

static void hb_set_run_state(void)
{
    if (s_force_fault) {
        s_state = HB_STATE_FAULT;
        s_green_on = 0u;
    } else {
        s_state = HB_STATE_RUN;
    }
    hb_apply_led();
}

void hb_init(void)
{
    s_kick_counter = 0u;
    s_kick_last = 0u;
    s_stall_ticks = 0u;
    s_force_fault = 0u;
    s_green_on = 0u;
    s_blink_period_ticks = ms_to_ticks(HB_PERIOD_SLOW_MS);
    s_blink_ticks = 0u;
    s_state = HB_STATE_BOOT;
    s_selftest_step = 0u;
    s_selftest_done = 0u;
    s_selftest_tick_acc = 0u;
    s_inited = 0u;

    bsp_rgb_init();
    hb_apply_led();

    __HAL_RCC_TIM6_CLK_ENABLE();

    s_htim6.Instance = TIM6;
    s_htim6.Init.Prescaler = 15999u;
    s_htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim6.Init.Period = 49u;
    s_htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&s_htim6) != HAL_OK) {
        s_state = HB_STATE_FAULT;
        hb_apply_led();
        return;
    }

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 1u, 0u);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    if (HAL_TIM_Base_Start_IT(&s_htim6) != HAL_OK) {
        s_state = HB_STATE_FAULT;
        hb_apply_led();
        return;
    }

    s_inited = 1u;
}

void hb_kick(void)
{
    if (!s_inited) {
        return;
    }
    s_kick_counter++;
}

void hb_set_load_hint(uint16_t load_permille)
{
    uint16_t period_ms;

    if (load_permille <= 200u) {
        period_ms = HB_PERIOD_SLOW_MS;
    } else if (load_permille <= 700u) {
        period_ms = HB_PERIOD_MID_MS;
    } else {
        period_ms = HB_PERIOD_FAST_MS;
    }

    s_blink_period_ticks = ms_to_ticks(period_ms);
}

void hb_force_fault(uint8_t on)
{
    if (!s_inited) {
        return;
    }

    if (on) {
        s_force_fault = 1u;
        s_state = HB_STATE_FAULT;
        s_green_on = 0u;
        s_selftest_done = 1u;
        hb_apply_led();
        return;
    }

    s_force_fault = 0u;
    s_stall_ticks = 0u;
    s_kick_last = s_kick_counter;
    s_selftest_done = 1u;
    hb_set_run_state();
}

void hb_on_timer_tick(void)
{
    if (!s_inited) {
        return;
    }

    if (!s_selftest_done) {
        hb_run_selftest_tick();
        return;
    }

    if (s_force_fault) {
        s_state = HB_STATE_FAULT;
    } else if (s_kick_counter == s_kick_last) {
        if (s_stall_ticks < 0xFFFFu) {
            s_stall_ticks++;
        }
        if (s_stall_ticks >= HB_STALL_TIMEOUT_TICKS) {
            s_state = HB_STATE_FAULT;
            s_green_on = 0u;
        }
    } else {
        s_stall_ticks = 0u;
        s_kick_last = s_kick_counter;
        if (s_state == HB_STATE_BOOT || s_state == HB_STATE_FAULT) {
            s_state = HB_STATE_RUN;
        }
    }

    if (s_state == HB_STATE_RUN) {
        s_blink_ticks++;
        if (s_blink_ticks >= s_blink_period_ticks) {
            s_blink_ticks = 0u;
            s_green_on ^= 1u;
        }
    } else {
        s_green_on = 0u;
    }

    hb_apply_led();
}

void hb_irqhandler(void)
{
    if (!s_inited) {
        return;
    }
    HAL_TIM_IRQHandler(&s_htim6);
}
