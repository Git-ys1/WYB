#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp_config.h"

typedef enum {
    BSP_I2C_BUS_OLED = 2,
    BSP_I2C_BUS_ADS = 3
} bsp_i2c_bus_t;

typedef enum {
    BSP_OLED_BUS_HW_I2C2 = 0,
    BSP_OLED_BUS_SOFT_I2C = 1
} bsp_oled_bus_mode_t;

typedef enum {
    BSP_PIN_RES_A = 0,
    BSP_PIN_RES_B,
    BSP_PIN_RES_C,
    BSP_PIN_MODE_A,
    BSP_PIN_MODE_B,
    BSP_PIN_MODE_C,
    BSP_PIN_VOLT_A,
    BSP_PIN_VOLT_B,
    BSP_PIN_KEY,
    BSP_PIN_BEEP,
    BSP_PIN_COUNT
} bsp_pin_t;

typedef enum {
    BSP_PWM_BEEP = 0
} bsp_pwm_t;

typedef struct {
    uint32_t period_ticks;
    uint32_t high_ticks;
    uint32_t tim_clk_hz;
    uint32_t last_capture_ms;
    bool valid;
} bsp_capture_t;

typedef enum {
    BSP_FREQ_PROFILE_20HZ = 0,
    BSP_FREQ_PROFILE_200HZ,
    BSP_FREQ_PROFILE_2KHZ,
    BSP_FREQ_PROFILE_20KHZ,
    BSP_FREQ_PROFILE_200KHZ,
    BSP_FREQ_PROFILE_COUNT
} bsp_freq_profile_t;

void bsp_init(void);
uint32_t bsp_millis(void);
void bsp_delay_ms(uint32_t delay_ms);

bool bsp_i2c_write(bsp_i2c_bus_t bus, uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
bool bsp_i2c_read(bsp_i2c_bus_t bus, uint8_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms);
bool bsp_i2c_probe(bsp_i2c_bus_t bus, uint8_t addr7, uint32_t timeout_ms);
bool bsp_i2c2_bus_recover(void);
bool bsp_i2c2_reinit_100k(void);
bool bsp_oled_bus_set_mode(bsp_oled_bus_mode_t mode);
bsp_oled_bus_mode_t bsp_oled_bus_get_mode(void);
void bsp_oled_bus_reset_stats(void);
uint32_t bsp_oled_bus_get_nack_count(void);

void bsp_gpio_write(bsp_pin_t pin, bool level);
bool bsp_gpio_read(bsp_pin_t pin);

bool bsp_pwm_start(bsp_pwm_t pwm, uint32_t freq_hz, uint8_t duty_pct);
void bsp_pwm_stop(bsp_pwm_t pwm);

bool bsp_freq_get_capture(bsp_capture_t *capture);
void bsp_freq_capture_start(void);
void bsp_freq_capture_set_profile(bsp_freq_profile_t profile);

void bsp_debug_log(const char *msg);

#endif
