#ifndef APP_HEARTBEAT_H
#define APP_HEARTBEAT_H

#include <stdint.h>

void hb_init(void);
void hb_kick(void);
void hb_set_load_hint(uint16_t load_permille);
void hb_force_fault(uint8_t on);
void hb_on_timer_tick(void);
void hb_irqhandler(void);

#endif
