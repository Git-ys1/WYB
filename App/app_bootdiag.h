#ifndef APP_BOOTDIAG_H
#define APP_BOOTDIAG_H

#include <stdint.h>

typedef enum {
    BOOT_RESET = 0,
    BOOT_HAL,
    BOOT_CLOCK,
    BOOT_MX,
    BOOT_BSP,
    BOOT_APP_INIT,
    BOOT_DISPLAY_INIT,
    BOOT_RUN,
    BOOT_FAULT
} bootdiag_stage_t;

enum {
    BOOT_FAULT_NONE = 0,
    BOOT_FAULT_HAL_CLOCK_MX = 1,
    BOOT_FAULT_BSP = 2,
    BOOT_FAULT_DISPLAY_INIT = 3,
    BOOT_FAULT_UI_FLUSH = 4,
    BOOT_FAULT_UNKNOWN = 5
};

void bootdiag_led_init(void);
void bootdiag_set_stage(bootdiag_stage_t stage);
void bootdiag_set_fault(uint8_t code);
bootdiag_stage_t bootdiag_get_stage(void);
uint8_t bootdiag_get_fault(void);
uint32_t bootdiag_get_ms(void);
void bootdiag_heartbeat_tick(void);
uint8_t bootdiag_fault_from_stage(bootdiag_stage_t stage);

#endif
