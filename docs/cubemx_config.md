# CubeMX Configuration Notes

## Clock
- Use stable system clock suitable for I2C and timer capture.
- Keep timer clock high enough for period/high-time resolution.

## Peripherals
- I2C2: `PC4(SCL), PA8(SDA)` for OLED.
- I2C3: `PC8(SCL), PC9(SDA)` for ADS1110.
- GPIO output: `PD5, PD6, PD7` (RES MUX A/B/C).
- GPIO output: `PB4, PB5, PB6` (MODE MUX A/B/C).
- GPIO output: `PB13, PB14` (VOLT MUX A/B).
- GPIO input + EXTI: `PC13` for KEY.
- TIM16 CH1 PWM: `PB8` for BEEP.
- Timer input capture channel on `PA0` for frequency and duty.

## NVIC and callbacks
- Enable EXTI interrupt for key debounce pipeline.
- Enable timer capture interrupt for PA0 capture updates.
- In HAL callbacks, store capture values used by `bsp_freq_get_capture()`.

## Electrical rules
- OLED and ADS1110 I2C pull-up to 3.3V only.
- Do not pull I2C lines to 5V.
