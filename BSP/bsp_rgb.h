#ifndef BSP_RGB_H
#define BSP_RGB_H

#include <stdint.h>

void bsp_rgb_init(void);
void bsp_rgb_set(uint8_t r, uint8_t g, uint8_t b);
void bsp_rgb_off(void);

#endif
