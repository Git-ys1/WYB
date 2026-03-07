# T-0.1.12 / T-1.1.4I Debug UI 说明

版本：`T1.1.4I-20260307`  
目标板：`STM32G474VET6`  
显示器：`SSD1306 128x64 I2C (I2C2: PC4/PA8)`  
采样链路：`ADC1(PC0)`（已移除 ADS1110）

## 0. 启动模式说明（T-1.1.4I）

当前固件保留 `APP_BOOT_SAFE_MODE`，但默认 `0`（全功能启动）。  
分阶段开关位于 `App/app.c`：

- `APP_STAGE_ENABLE_OLED`
- `APP_STAGE_ENABLE_I2C_SCAN`
- `APP_STAGE_ENABLE_ADC1`
- `APP_STAGE_ENABLE_RES_RUN`

默认值全部为 `1`（跟随全功能路径）。仅在排故时临时打开 safe mode。

## 1. 屏幕布局（默认进入 DEBUG 页）

- `L0`：版本 + 心跳 + tick(ms)
- `L1`：I2C2 扫描摘要（OLED 地址扫描结果）
- `L2`：ADC 状态摘要（`raw + Vred + stat`）
- `L3`：模块状态压缩串：`2OATKBM`
- `L4`：`BOOT STAGE: xx`
- `L5`：`OLED ERR: Ex` + 总线模式（`HW/SW`）
- `L6`：`NACK:<count> RGB:UNK`
- `L7`：最近日志（最多 1 条）

说明：
- 心跳字符循环 `| / - \`，快速确认程序仍在运行。
- `APP_OLED_RESCUE_MODE=1` 下 UI 默认走全屏 `oled_flush()`（约 5Hz），优先稳定性。
- 连续 flush 失败 3 次会进入统一恢复状态机（不允许并发恢复）。
- OLED 恢复固定链路：`I2C2总线解锁 -> I2C2重初始化(100k) -> HW探测/初始化 -> SW探测/初始化 -> 重试调度`。
- 启动后前 2 秒保留 BOOT 固定页，不立即切换复杂菜单。
- OLED 总线切换权仅在 `App/app.c` 状态机；`BSP` 底层不再自动 HW/SW 来回切换。

## 1.1 RGB 心跳灯（新增）

引脚与极性：
- `RGB_B = PE3`
- `RGB_R = PE4`
- `RGB_G = PE5`
- `Active-Low`

状态规则：
- `BOOT/FAULT`：红灯常亮，绿灯灭。
- `RUN`：红灯灭，绿灯按负载闪烁。
- 连续约 `1s` 未喂心跳（`hb_kick`）自动转 `FAULT`。

## 1.2 Bootdiag 观测（T-1.1.4I）

公开接口（`App/app.h`）：
- `bootdiag_get_stage()`
- `bootdiag_get_err()`
- `bootdiag_get_ms()`

阶段定义：
- `10`：`BOOT_STAGE_10_GPIO_OK`
- `20`：`BOOT_STAGE_20_I2C2_PROBE_OK`
- `30`：`BOOT_STAGE_30_OLED_INIT_OK`
- `40`：`BOOT_STAGE_40_OLED_FLUSH_OK`
- `101`：`BOOT_STAGE_EX1_I2C2_PROBE_FAIL`
- `102`：`BOOT_STAGE_EX2_OLED_INIT_FAIL`
- `103`：`BOOT_STAGE_EX3_OLED_FLUSH_FAIL`
- `104`：`BOOT_STAGE_EX4_OLED_RECOVER_FAIL`

典型定位：
- `101`：通常卡在 I2C 探测（含地址/上拉/总线占用）
- `102`：探测通过但初始化序列失败
- `103`：初始化通过但整屏 flush 失败
- `104`：一次完整 HW+SW 路径都失败，进入 FAIL_WAIT 重试

## 2. 状态位定义

`L3` 格式：`2OATKBM:XXXXXXX`

字符含义：
- `2`：`MOD_I2C2`
- `O`：`MOD_OLED`
- `A`：`MOD_ADC1`
- `T`：`MOD_TIM2IC`
- `K`：`MOD_KEY`
- `B`：`MOD_BEEP`
- `M`：`MOD_MUX`

状态字符：
- `+`：OK
- `-`：FAIL
- `?`：UNKNOWN

## 3. 日志格式

每条日志显示为：

`tttt L message`

- `tttt`：时间戳低 4 位（ms）
- `L`：级别（`I/W/E`）
- `message`：内部最大 24 字符，OLED 再按行宽截断

日志队列：
- 环形缓冲 `N=32`
- DEBUG 页刷新周期 `200ms`

## 4. 错误码与 ADC 状态

来自 `Drivers/drv_error.h`：
- `0` `ERR_OK`
- `1` `ERR_I2C_NACK`
- `2` `ERR_ADC_TIMEOUT`
- `3` `ERR_OVERRANGE`
- `4` `ERR_NO_SIGNAL`
- `5` `ERR_INVALID_ARG`
- `6` `ERR_NOT_IMPL`
- `7` `ERR_HW_FAIL`

RES 运行页 ADC 行：
- `ADC stat: OK` -> `ERR_OK`
- `ADC stat: TIMEOUT` -> `ERR_ADC_TIMEOUT`
- `ADC stat: OVR` -> `ERR_HW_FAIL`（片内 ADC overrun/硬件失败）
- 其他 -> `ERR`

## 5. 按键行为（当前实现）

- 短按触发点：`KEY_EVT_DOWN`（按下稳定即执行）
- 消抖（按下/释放）：`2ms / 8ms`
- 长按阈值：`600ms`
- `MEAS` 层长按 `OK`：快速返回一级菜单

## 6. 典型故障排查

1. OLED 无显示/黑屏
- 看 `MOD_I2C2` 与 `MOD_OLED` 是否为 `-`
- 确认地址为 `0x3C/0x3D`
- 确认上拉到 `3.3V`
- 读 `BOOT STAGE/OLED ERR` 或 Live Watch 的 `bootdiag_get_*`，定位卡在 Probe / Init / Flush / Recover 哪一步
- 观察 `L6` 的 `BUS:HW/SW` 与 `NACK` 计数是否持续增长
- 黑屏时直接用 Live Watch 读 `bootdiag_get_*`，确认当前停在 `101/102/103/104` 哪一段

2. ADC 无效
- 看 `L2` 是否长期 `ADC ... TIMEOUT/OVR`
- 检查 `PC0` 是否接到测量节点
- 检查 ADC 输入电压是否超出 `0~3.3V`

3. 频率不显示
- 看 `MOD_TIM2IC` 是否为 `-`
- 检查 PA0 输入（建议先 `1kHz/30%`）
- 注意：TIM2 捕获会在 `BOOT_STAGE_40` 之后才启动（启动期延后策略）

4. 系统是否运行
- 看 `L0` 心跳与 tick 是否持续变化

5. 烧录后 STLink 只能按住复位连接
- 检查 `WYB.ioc` 是否为 `SYS.Debug=Serial Wire`
- 检查 `PA13/PA14` 是否保留为 `SYS_JTMS-SWDIO / SYS_JTCK-SWCLK`
- 默认应为 `APP_BOOT_SAFE_MODE=0`（全功能）；仅排故时改为 `1`
