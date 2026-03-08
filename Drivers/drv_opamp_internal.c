#include "drv_opamp_internal.h"

#include "../Core/Inc/main.h"

static bool g_opamp1_ready = false;
static app_err_t g_opamp1_status = ERR_NOT_IMPL;

static void opamp1_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
}

app_err_t opamp1_init(void)
{
    uint32_t csr;

    opamp1_gpio_init();

    /* Ensure OPAMP1 is disabled before reconfiguration. */
    CLEAR_BIT(OPAMP1->CSR, OPAMP_CSR_OPAMPxEN);

    /* Follower mode + VINP0(PA1) + internal output to ADC + factory trimming. */
    csr = OPAMP1->CSR;
    CLEAR_BIT(csr, OPAMP_CSR_FORCEVP |
                   OPAMP_CSR_VPSEL |
                   OPAMP_CSR_VMSEL |
                   OPAMP_CSR_USERTRIM |
                   OPAMP_CSR_HIGHSPEEDEN |
                   OPAMP_CSR_OPAMPINTEN |
                   OPAMP_CSR_CALON |
                   OPAMP_CSR_CALSEL |
                   OPAMP_CSR_PGGAIN |
                   OPAMP_CSR_TRIMOFFSETP |
                   OPAMP_CSR_TRIMOFFSETN);
    csr |= OPAMP_CSR_VMSEL;      /* Follower mode */
    csr |= OPAMP_CSR_OPAMPINTEN; /* Internal output path to ADC */
    OPAMP1->CSR = csr;

    /* Keep factory trimming (USERTRIM=0). */
    SET_BIT(OPAMP1->CSR, OPAMP_CSR_OPAMPxEN);
    HAL_Delay(1u);

    if ((OPAMP1->CSR & OPAMP_CSR_OPAMPxEN) == 0u) {
        g_opamp1_ready = false;
        g_opamp1_status = ERR_HW_FAIL;
        return g_opamp1_status;
    }

    g_opamp1_ready = true;
    g_opamp1_status = ERR_OK;
    return ERR_OK;
}

bool opamp1_ready(void)
{
    return g_opamp1_ready;
}

app_err_t opamp1_last_status(void)
{
    return g_opamp1_status;
}
