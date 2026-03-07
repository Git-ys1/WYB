#ifndef DRV_BUTTON_H
#define DRV_BUTTON_H

#include <stdbool.h>

typedef enum {
    BTN_EVT_NONE = 0,
    BTN_SHORT,
    BTN_LONG
} button_event_t;

void button_init(bool active_low);
void button_poll(void);
button_event_t button_get_event(void);

#endif
