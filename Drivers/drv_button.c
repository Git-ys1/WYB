#include "drv_button.h"

#include "../BSP/bsp.h"

#define BTN_SHORT_MS 300u
#define BTN_LONG_MS 800u
#define BTN_QUEUE_SIZE 8u

typedef struct {
    button_event_t evt[BTN_QUEUE_SIZE];
    uint8_t w;
    uint8_t r;
} button_queue_t;

static bool g_active_low = true;
static bool g_pressed;
static bool g_long_fired;
static uint32_t g_press_t0;
static button_queue_t g_queue;

static void queue_push(button_event_t evt)
{
    uint8_t next;

    if (evt == BTN_EVT_NONE) {
        return;
    }

    next = (uint8_t)((g_queue.w + 1u) % BTN_QUEUE_SIZE);
    if (next == g_queue.r) {
        return;
    }

    g_queue.evt[g_queue.w] = evt;
    g_queue.w = next;
}

static bool button_level_pressed(void)
{
    bool raw = bsp_gpio_read(BSP_PIN_KEY);
    return g_active_low ? !raw : raw;
}

void button_init(bool active_low)
{
    g_active_low = active_low;
    g_pressed = false;
    g_long_fired = false;
    g_press_t0 = 0u;
    g_queue.w = 0u;
    g_queue.r = 0u;
}

void button_poll(void)
{
    bool now_pressed = button_level_pressed();
    uint32_t now_ms = bsp_millis();

    if (!g_pressed && now_pressed) {
        g_pressed = true;
        g_long_fired = false;
        g_press_t0 = now_ms;
        return;
    }

    if (g_pressed && now_pressed) {
        uint32_t held_ms = now_ms - g_press_t0;
        if (!g_long_fired && held_ms > BTN_LONG_MS) {
            g_long_fired = true;
            queue_push(BTN_LONG);
        }
        return;
    }

    if (g_pressed && !now_pressed) {
        uint32_t held_ms = now_ms - g_press_t0;
        if (!g_long_fired && (held_ms > 30u) && (held_ms < BTN_SHORT_MS)) {
            queue_push(BTN_SHORT);
        }
        g_pressed = false;
        g_long_fired = false;
    }
}

button_event_t button_get_event(void)
{
    button_event_t evt;

    if (g_queue.r == g_queue.w) {
        return BTN_EVT_NONE;
    }

    evt = g_queue.evt[g_queue.r];
    g_queue.r = (uint8_t)((g_queue.r + 1u) % BTN_QUEUE_SIZE);
    return evt;
}

