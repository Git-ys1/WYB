# T-1.1.5G-R1 Boot Flow

## 1) 启动顺序（文字版）
1. `BOOT_RESET`：复位入口，清 fault。
2. `BOOT_HAL`：`HAL_Init()`
3. `BOOT_CLOCK`：`SystemClock_Config()`
4. `BOOT_MX`：`MX_GPIO/I2C2/I2C3/TIM2/TIM16`
5. `BOOT_BSP`：`bsp_init()`
6. `BOOT_APP_INIT`：`app_init()`
7. `BOOT_DISPLAY_INIT`：`app_display_init_once()`
8. `BOOT_RUN`：进入 superloop（`app_*_tick`）
9. 任意失败：`BOOT_FAULT` + fault code

## 2) Display Ownership
- 能 `init` 显示：`app_display_service.c`（`app_display_init_once()`）
- 能 `flush` 显示：`app_display_service.c` 私有 `flush_lines()`
- 只能组装文本帧：`app.c`
- 只能转发：`app_ui_presenter.c`
- 不能碰显示底层：`main.c`、`app_measure_tick()`、其它业务模块

## 3) Fault Code
- `1`：HAL/Clock/MX fault
- `2`：BSP fault
- `3`：Display init fault
- `4`：UI flush fault
- `5`：Unknown fault

## 4) PB12 行为
- Boot：100ms 快闪
- Run：500ms 慢闪
- Fault：`N` 次短闪（150ms on/off）+ 800ms 停顿循环
