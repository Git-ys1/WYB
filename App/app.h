#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum {
    BOOT_STAGE_10_GPIO_OK = 10,
    BOOT_STAGE_20_I2C2_PROBE_OK = 20,
    BOOT_STAGE_30_OLED_INIT_OK = 30,
    BOOT_STAGE_40_OLED_FLUSH_OK = 40,
    BOOT_STAGE_EX1_I2C2_PROBE_FAIL = 101,
    BOOT_STAGE_EX2_OLED_INIT_FAIL = 102,
    BOOT_STAGE_EX3_OLED_FLUSH_FAIL = 103,
    BOOT_STAGE_EX4_OLED_RECOVER_FAIL = 104
} boot_stage_t;

void app_init(void);
void app_poll_button(void);
void app_measure_tick(void);
void app_ui_tick(void);
void app_beep_tick(void);

boot_stage_t bootdiag_get_stage(void);
int bootdiag_get_err(void);
uint32_t bootdiag_get_ms(void);

#endif
