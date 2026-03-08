# WYB (STM32G474 万用表固件)

当前版本基于 `STM32G474VET6`，主循环架构为 `HAL + Superloop`。

## STM32CubeIDE 编译方式（GUI + Headless）
### GUI（一键 Build）
1. 打开工程目录：`F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3\WYB`
2. 在 STM32CubeIDE 里执行 `Project -> Build All`
3. 产物默认位于：`Debug/WYB.elf`、`Debug/WYB.hex`

### Headless（Codex 默认方式）
```powershell
F:\CodeForge\STM32CubeIDE_2.1.0\STM32CubeIDE\stm32cubeidec.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3_headless -import F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3\WYB -cleanBuild WYB/Debug
```

说明：
- 如果 GUI 正在占用同一个 workspace，会出现 `Workspace already in use` 锁冲突。
- 解决方式是使用独立 headless workspace（如 `WorkSpace3_headless`）。

## 关键约束（当前）
- 禁止使用 ADS1110 外部 ADC。
- 采样链路已切换为片内 ADC：`ADC1 + PC0 (ADC12_IN6)`。
- 电阻测量链路：`MUX -> ADC1(PC0) -> Vred -> Rx`。
- `I2C3` 初始化暂时保留但不参与测量，后续可在 `.ioc` 清理。
- OLED 统一显示后端固定为 `oled_smoke_*` 封装（`App/app_display_service.*`）。
- 主流程默认不再依赖 smoke 编译分流，统一走 `bsp_init + app_init + superloop`。
- 本阶段不恢复 Soft-I2C / recover 状态机 / dirty flush / 多页面并发刷新。

## T-1.1.5G-R1 启动脊柱（当前口径）
- 启动阶段统一由 `App/app_bootdiag.*` 上报：
  - `BOOT_RESET`
  - `BOOT_HAL`
  - `BOOT_CLOCK`
  - `BOOT_MX`
  - `BOOT_BSP`
  - `BOOT_APP_INIT`
  - `BOOT_DISPLAY_INIT`
  - `BOOT_RUN`
  - `BOOT_FAULT`
- 故障码固定：
  - `1`: HAL/Clock/MX
  - `2`: BSP
  - `3`: Display init
  - `4`: UI flush
  - `5`: Unknown
- `PB12` 现在是正式启动心跳/故障灯：
  - Boot 快闪（100ms）
  - Run 慢闪（500ms）
  - Fault 按码闪烁（150ms on/off + 800ms 间隔）
- 详细流程见 [docs/boot_flow_r1.md](/F:/CodeForge/STM32CubeIDE_2.1.0/WorkSpace3/WYB/docs/boot_flow_r1.md)

## 当前实现范围
- 统一显示链路：`app -> presenter -> app_display_service -> oled_smoke`（单写者）。
- 6 键菜单输入（运行时代码初始化，不依赖 `.ioc` 按键配置）。
- 菜单树（浏览态）：
  - `UI_DIAG`
  - `UI_MAIN_MENU`
  - `UI_DEBUG_MENU`
  - `UI_DEBUG_ADC`
  - `UI_BOOT_INFO`
  - `UI_MEASURE_MENU`
  - `UI_RES_RANGE`
  - `UI_RES_READY / UI_VDC_READY / UI_FREQ_READY / UI_CONT_READY / UI_DIODE_READY`
- 所有功能页当前为 READY 占位，`RUN` 未启用。
- 片内 ADC 驱动：
  - `adc1_init`
  - `adc1_read_raw_u16`
  - `adc1_read_mv`
  - `adc1_read_filtered`（16 点 trimmed mean）
  - `adc1_read_vdda_mv`（VREFINT 估算 VDDA，失败回退 3300mV）

## T-1.5.1A 历史 Smoke 基线（保留说明）
- 已验证的 OLED 底层参数：`SSD1315 + HW I2C2 + 100k + page mode + 16B chunk flush`。
- 该 smoke 路径已作为底层后端能力保留，不再作为主流程默认入口。

## T-1.1.5F-R1 统一显示架构（当前主线）
- `oled_smoke_*` 已提升为正式显示后端，由 `App/app_display_service.*` 统一封装。
- 主流程显示不再依赖 `APP_SMOKE_OLED_TEST` 编译分流，默认直接进入：
  - `HAL_Init -> SystemClock_Config -> MX_* -> bsp_init -> app_init -> superloop`
- 单写者规则：
  - 正式路径仅允许 `app_display_service` 调用底层 flush
  - `app.c` 只组织菜单/调试文本，不直接操作 OLED 底层
- 当前菜单恢复范围：`L1_MODULE / L2_DEBUG_PAGE / L2_MEAS_FUNC / L3_RES_RANGE / L4_RES_READY`
- 本轮仍保持测量懒启动：非 `RES_RUN` 页面不启动真实测量。

## T-1.1.6A-R1 范围声明
- 在不改显示底层的前提下恢复完整菜单浏览树。
- 上电先进入诊断页，3 秒后自动解锁菜单；`OK` 可立即进入菜单。
- 本轮不启用任何真实测量运行，保持 `RUN DISABLED` 占位。

## T-1.1.5E-R1 历史说明
- `T-1.1.5E-R1` 的 smoke 主分流策略已被 `T-1.1.5F-R1` 统一显示架构替代。
- 当前实验分支：`exp/ui-unify-r1`。

## 冻结引脚映射
- OLED I2C2：`PC4(SCL), PA8(SDA)`
- ADC 输入：`PC0 (ADC12_IN6)`
- RES CD4051：`PD5/PD6/PD7`
- MODE CD4051：`PB4/PB5/PB6`
- VOLT CD4051：`PB13/PB14`
- 频率输入捕获：`PA0 (TIM2_CH1)`
- 蜂鸣器：`PB8`（优先 `TIM16_CH1`）
- RGB 心跳灯（Active-Low）：
  - `蓝=PE3`
  - `红=PE4`
  - `绿=PE5`
- 六键：
  - `OK=PC13`
  - `UP=PB0`
  - `DOWN=PB1`
  - `LEFT=PB2`
  - `RIGHT=PB10`
  - `BACK=PB11`

## 主循环（保持不变）
```c
while (1) {
    app_poll_button();
    app_measure_tick();
    app_ui_tick();
    app_beep_tick();
}
```

## 心跳/故障可视化（当前）
- 当前主诊断信号为 `PB12`（bootdiag 控制），用于区分 Boot/Run/Fault。
- 历史 `TIM6 + RGB` 路径保留代码但不作为本轮诊断主路径。

## 目录说明
- `App/`：菜单状态机、调试 UI、日志联动。
- `Drivers/`：OLED/MUX/FREQ/BEEP/ADC1 等驱动。
- `Measurements/`：VDC/RES/CONT/DIODE/FREQ 算法与路由。
- `Calib/`：标定参数与加载接口。
- `BSP/`：HAL 端口与板级抽象。
- `docs/`：调试页面、测试清单、阶段记录。

## 注意事项
- I2C 上拉必须接 `3.3V`。
- ADC 输入点必须限制在 `0~3.3V`。
- 高源阻抗档位需使用长采样时间（当前已按长采样配置）。
- OLED 黑屏时优先看 `bootdiag_get_stage()/bootdiag_get_fault()`（STLink Live Watch）。

## Git 工作纪律（强制执行）
- GitHub 账号：`Git-ys1`
- Git 邮箱：`3091964993@qq.com`
- 仓库：`WYB`（优先 `private`）

### 每轮开始前（必须）
```powershell
git status
git add .
git commit -m "checkpoint: before <TASK_ID>"
git push
```

### 每轮结束后（必须）
```powershell
git status
git add .
git commit -m "<TASK_ID>: <summary>"
git push
```

### 回退优先级（固定）
1. 未提交改坏：`git restore .` 或 `git restore <file>`
2. 已提交安全撤销：`git revert HEAD`
3. 本地实验硬退：`git reset --hard HEAD~1`

详细流程见 [docs/git_workflow.md](/F:/CodeForge/STM32CubeIDE_2.1.0/WorkSpace3/WYB/docs/git_workflow.md)。
