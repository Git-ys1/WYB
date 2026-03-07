#include "bsp_rgb.h"

#include "../Core/Inc/main.h"

#define RGB_ACTIVE_LOW 1u

#define RGB_B_PORT GPIOE
#define RGB_B_PIN GPIO_PIN_3
#define RGB_R_PORT GPIOE
#define RGB_R_PIN GPIO_PIN_4
#define RGB_G_PORT GPIOE
#define RGB_G_PIN GPIO_PIN_5

static GPIO_PinState rgb_level_to_pin(uint8_t on)
{
#if RGB_ACTIVE_LOW
    return on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#else
    return on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#endif
}

void bsp_rgb_init(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    init.Pin = RGB_B_PIN | RGB_R_PIN | RGB_G_PIN;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &init);

    bsp_rgb_off();
}

void bsp_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN, rgb_level_to_pin((uint8_t)(r != 0u)));
    HAL_GPIO_WritePin(RGB_G_PORT, RGB_G_PIN, rgb_level_to_pin((uint8_t)(g != 0u)));
    HAL_GPIO_WritePin(RGB_B_PORT, RGB_B_PIN, rgb_level_to_pin((uint8_t)(b != 0u)));
}

void bsp_rgb_off(void)
{
    bsp_rgb_set(0u, 0u, 0u);
}
