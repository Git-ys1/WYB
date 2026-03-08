#ifndef BSP_KEYS_H
#define BSP_KEYS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    KEY_OK = 0,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_BACK,
    KEY_COUNT
} key_id_t;

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_DOWN,
    KEY_EVT_UP,
    KEY_EVT_LONG,
    KEY_EVT_REPEAT
} key_evt_type_t;

typedef struct {
    key_id_t key;
    key_evt_type_t type;
    uint32_t ms;
} key_event_t;

void bsp_keys_init(void);
void keys_poll(void);
bool keys_get_event(key_event_t *evt);

#endif
