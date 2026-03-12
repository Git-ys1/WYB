# WYB 提交态固件（T-1.6.2-R1）

当前分支为提交态冻结版本，目标是保持单一主链路：`HAL + superloop + 单写者显示 + 单一输入/蜂鸣器/FREQ链路`。

## 构建方式（推荐 headless）
```powershell
F:\CodeForge\STM32CubeIDE_2.1.0\STM32CubeIDE\stm32cubeidec.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3_headless -import F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3\WYB -cleanBuild WYB/Debug
```

## 提交态冻结口径
- 显示链路：`app -> presenter -> app_display_service -> oled_smoke`（唯一 flush 写者）。
- 频率链路：`PA0(TIM2_CH1) -> TIM2 IC -> bsp_capture(ticks) -> drv_freq_ic -> app`。
- 模式映射：`MODE CH0=VOLTAGE, CH1=RES/CONT, CH2=DIODE, CH3=FREQ`。
- 蜂鸣器：`PB1`，active-low（`Low=ON`, `High=OFF`），正式链路仅此一条。
- 二极管激励：`PB0`，`ON=推挽高`，`OFF=Analog/Hi-Z`。
- 输入：`RIGHT` 为正式功能键，`LEFT` 为调试页开关（可通过编译开关关闭）。

## 开机画面
- OLED 初始化成功后先显示 2 秒开机位图（128x64），内容居中：
  - 第一行：`数字万用表`
  - 第二行：`李浩天`
  - 第三行：`23291043`
- 2 秒后清屏并进入主 UI。
- T-1.6.2-R1 微调：启动位图整图重画，三行重新居中排版，第三行学号改小字模避免底部裁切。

## 当前功能状态
- `RES`：可用（手动四档 + AUTO），开机默认档位为 `AUTO`。
- `CONT`：可用（仅 CONT 模式持续鸣叫）。
- `DIODE`：可用（方案 A 单向激励）。
- `FREQ`：主链已冻结，默认档位为 `AUTO`；手动档越档主值直接显示 `OL`，并增加卡屏两级恢复（先重置显示链，失败后单次软复位）。
- `VDC`：新增 `AUTO`（默认档），在 `AUTO/2000mV/20V` 之间可切换，自动切档采用迟滞与投票；`20V` 档精度问题仍为已知项。

## FREQ 热修框架位（已预留）
`bsp_freq_capture_set_profile()` 对每档位支持：
- `icpsc`
- `no_sig_timeout_ms`
- `accum_cycles`

后续频率优化仅允许调整 profile 与 `drv_freq_ic` 模块内判定，不再改主循环或重走新测量路线。

## T-1.6.2-R1 热修范围
- 仅包含：`boot_splash_bitmap` 重画 + VDC 默认 `AUTO` + FREQ 越档 `OL` 与两级恢复。
- FREQ 仍固定：`PA0(TIM2_CH1) -> TIM2 capture -> BSP ticks -> drv_freq_ic -> app`。
- 未改主循环、未新增第二显示链、未新增第二频率链。

## 文档入口
- 提交态真相冻结：`docs/T-1.5.5_truth_freeze.md`
- 本轮清理报告：`docs/T-1.5.5_cleanup_report.md`
- 交稿前最小验收：`docs/validation_checklist.md`
- 历史阶段文档：`docs/archive/`（如有）与 `forcodex/plan/`
