#include "bsp_keys.h"

#include <string.h>

#include "../Core/Inc/main.h"
#include "bsp.h"

#define KEY_POLL_MS 1u
#define KEY_DEBOUNCE_DOWN_MS 2u
#define KEY_DEBOUNCE_UP_MS 8u
#define KEY_LONG_MS 600u
#define KEY_QUEUE_SIZE 24u

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} key_pin_desc_t;

typedef struct {
    uint8_t raw_pressed;
    uint8_t stable_pressed;
    uint8_t long_fired;
    uint32_t raw_change_ms;
    uint32_t pressed_since_ms;
} key_state_t;

typedef struct {
    key_event_t events[KEY_QUEUE_SIZE];
    uint8_t w;
    uint8_t r;
} key_queue_t;

static const key_pin_desc_t k_key_desc[KEY_COUNT] = {
    [KEY_OK] = {GPIOC, GPIO_PIN_13},
    [KEY_UP] = {GPIOB, GPIO_PIN_0},
    [KEY_DOWN] = {GPIOB, GPIO_PIN_1},
    [KEY_LEFT] = {GPIOB, GPIO_PIN_2},
    [KEY_RIGHT] = {GPIOB, GPIO_PIN_10},
    [KEY_BACK] = {GPIOB, GPIO_PIN_11}
};

static key_state_t g_keys[KEY_COUNT];
static key_queue_t g_queue;
static uint32_t g_last_poll_ms;

static void queue_push(key_id_t key, key_evt_type_t type, uint32_t now_ms)
{
    uint8_t next;

    if ((key >= KEY_COUNT) || (type == KEY_EVT_NONE)) {
        return;
    }

    next = (uint8_t)((g_queue.w + 1u) % KEY_QUEUE_SIZE);
    if (next == g_queue.r) {
        return;
    }

    g_queue.events[g_queue.w].key = key;
    g_queue.events[g_queue.w].type = type;
    g_queue.events[g_queue.w].ms = now_ms;
    g_queue.w = next;
}

static uint8_t key_read_pressed(key_id_t key)
{
    if (key >= KEY_COUNT) {
        return 0u;
    }
    return (HAL_GPIO_ReadPin(k_key_desc[key].port, k_key_desc[key].pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

void bsp_keys_init(void)
{
    GPIO_InitTypeDef init = {0};
    uint8_t i;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_LOW;

    init.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &init);

    init.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &init);

    memset(g_keys, 0, sizeof(g_keys));
    memset(&g_queue, 0, sizeof(g_queue));
    g_last_poll_ms = 0u;

    for (i = 0u; i < KEY_COUNT; i++) {
        uint8_t pressed = key_read_pressed((key_id_t)i);
        g_keys[i].raw_pressed = pressed;
        g_keys[i].stable_pressed = pressed;
        g_keys[i].raw_change_ms = bsp_millis();
        g_keys[i].pressed_since_ms = bsp_millis();
        g_keys[i].long_fired = 0u;
    }
}

void keys_poll(void)
{
    uint8_t i;
    uint32_t now_ms = bsp_millis();

    if ((now_ms - g_last_poll_ms) < KEY_POLL_MS) {
        return;
    }
    g_last_poll_ms = now_ms;

    for (i = 0u; i < KEY_COUNT; i++) {
        key_state_t *st = &g_keys[i];
        uint8_t raw_pressed = key_read_pressed((key_id_t)i);

        if (raw_pressed != st->raw_pressed) {
            st->raw_pressed = raw_pressed;
            st->raw_change_ms = now_ms;
        }

        if (st->stable_pressed != st->raw_pressed) {
            uint32_t debounce_ms = st->raw_pressed ? KEY_DEBOUNCE_DOWN_MS : KEY_DEBOUNCE_UP_MS;
            if ((now_ms - st->raw_change_ms) < debounce_ms) {
                continue;
            }

            st->stable_pressed = st->raw_pressed;
            if (st->stable_pressed) {
                st->pressed_since_ms = now_ms;
                st->long_fired = 0u;
                queue_push((key_id_t)i, KEY_EVT_DOWN, now_ms);
            } else {
                queue_push((key_id_t)i, KEY_EVT_UP, now_ms);
            }
        }

        if (st->stable_pressed && !st->long_fired && ((now_ms - st->pressed_since_ms) >= KEY_LONG_MS)) {
            st->long_fired = 1u;
            queue_push((key_id_t)i, KEY_EVT_LONG, now_ms);
        }
    }
}

bool keys_get_event(key_event_t *evt)
{
    if (evt == NULL) {
        return false;
    }

    if (g_queue.r == g_queue.w) {
        return false;
    }

    *evt = g_queue.events[g_queue.r];
    g_queue.r = (uint8_t)((g_queue.r + 1u) % KEY_QUEUE_SIZE);
    return true;
}
