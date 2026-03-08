#include "app_bootdiag.h"

#include "../Core/Inc/main.h"
#include "stm32g4xx_hal.h"

#define BOOT_BLINK_FAST_MS 100u
#define BOOT_BLINK_SLOW_MS 500u
#define BOOT_FAULT_BLINK_MS 150u
#define BOOT_FAULT_GAP_MS 800u

typedef enum {
    LED_MODE_BOOT = 0,
    LED_MODE_RUN,
    LED_MODE_FAULT
} bootdiag_led_mode_t;

static volatile bootdiag_stage_t s_stage = BOOT_RESET;
static volatile uint8_t s_fault_code = BOOT_FAULT_NONE;
static volatile uint32_t s_stage_ms = 0u;
static uint8_t s_led_inited = 0u;
static uint8_t s_led_on = 0u;
static uint32_t s_led_deadline_ms = 0u;
static uint8_t s_fault_phase = 0u;
static uint8_t s_fault_blink_count = 0u;
static uint8_t s_fault_code_cached = BOOT_FAULT_NONE;

static void led_write(uint8_t on)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bootdiag_led_mode_t current_led_mode(void)
{
    if ((s_fault_code != BOOT_FAULT_NONE) || (s_stage == BOOT_FAULT)) {
        return LED_MODE_FAULT;
    }
    if (s_stage == BOOT_RUN) {
        return LED_MODE_RUN;
    }
    return LED_MODE_BOOT;
}

static void update_boot_led(uint32_t now)
{
    if (now < s_led_deadline_ms) {
        return;
    }

    s_led_on ^= 1u;
    led_write(s_led_on);
    s_led_deadline_ms = now + BOOT_BLINK_FAST_MS;
}

static void update_run_led(uint32_t now)
{
    if (now < s_led_deadline_ms) {
        return;
    }

    s_led_on ^= 1u;
    led_write(s_led_on);
    s_led_deadline_ms = now + BOOT_BLINK_SLOW_MS;
}

static void reset_fault_sequence(uint32_t now, uint8_t fault_code)
{
    s_fault_code_cached = fault_code;
    s_fault_phase = 0u;
    s_fault_blink_count = 0u;
    s_led_on = 0u;
    led_write(0u);
    s_led_deadline_ms = now;
}

static void update_fault_led(uint32_t now)
{
    uint8_t fault_code = (s_fault_code == BOOT_FAULT_NONE) ? BOOT_FAULT_UNKNOWN : s_fault_code;

    if ((fault_code != s_fault_code_cached) && (s_led_inited != 0u)) {
        reset_fault_sequence(now, fault_code);
    }

    if (now < s_led_deadline_ms) {
        return;
    }

    if (s_fault_phase == 0u) {
        if (s_fault_blink_count < fault_code) {
            s_led_on = 1u;
            led_write(1u);
            s_led_deadline_ms = now + BOOT_FAULT_BLINK_MS;
            s_fault_phase = 1u;
        } else {
            s_led_on = 0u;
            led_write(0u);
            s_led_deadline_ms = now + BOOT_FAULT_GAP_MS;
            s_fault_phase = 2u;
        }
    } else if (s_fault_phase == 1u) {
        s_led_on = 0u;
        led_write(0u);
        s_led_deadline_ms = now + BOOT_FAULT_BLINK_MS;
        s_fault_blink_count++;
        s_fault_phase = 0u;
    } else {
        s_fault_blink_count = 0u;
        s_fault_phase = 0u;
        s_led_deadline_ms = now;
    }
}

void bootdiag_led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    s_led_inited = 1u;
    s_led_on = 0u;
    s_led_deadline_ms = HAL_GetTick();
    s_fault_phase = 0u;
    s_fault_blink_count = 0u;
    s_fault_code_cached = BOOT_FAULT_NONE;
    led_write(0u);
}

void bootdiag_set_stage(bootdiag_stage_t stage)
{
    s_stage = stage;
    s_stage_ms = HAL_GetTick();

    if (stage != BOOT_FAULT) {
        s_fault_phase = 0u;
        s_fault_blink_count = 0u;
        s_fault_code_cached = BOOT_FAULT_NONE;
    }
}

void bootdiag_set_fault(uint8_t code)
{
    s_fault_code = code;
    if (code == BOOT_FAULT_NONE) {
        s_fault_phase = 0u;
        s_fault_blink_count = 0u;
        s_fault_code_cached = BOOT_FAULT_NONE;
    }
}

bootdiag_stage_t bootdiag_get_stage(void)
{
    return s_stage;
}

uint8_t bootdiag_get_fault(void)
{
    return s_fault_code;
}

uint32_t bootdiag_get_ms(void)
{
    return s_stage_ms;
}

uint8_t bootdiag_fault_from_stage(bootdiag_stage_t stage)
{
    if ((stage == BOOT_HAL) || (stage == BOOT_CLOCK) || (stage == BOOT_MX) || (stage == BOOT_RESET)) {
        return BOOT_FAULT_HAL_CLOCK_MX;
    }
    if (stage == BOOT_BSP) {
        return BOOT_FAULT_BSP;
    }
    if (stage == BOOT_DISPLAY_INIT) {
        return BOOT_FAULT_DISPLAY_INIT;
    }
    if ((stage == BOOT_APP_INIT) || (stage == BOOT_RUN)) {
        return BOOT_FAULT_UI_FLUSH;
    }
    return BOOT_FAULT_UNKNOWN;
}

void bootdiag_heartbeat_tick(void)
{
    bootdiag_led_mode_t mode;
    uint32_t now;

    if (s_led_inited == 0u) {
        return;
    }

    now = HAL_GetTick();
    mode = current_led_mode();

    if (mode == LED_MODE_BOOT) {
        update_boot_led(now);
    } else if (mode == LED_MODE_RUN) {
        update_run_led(now);
    } else {
        update_fault_led(now);
    }
}
