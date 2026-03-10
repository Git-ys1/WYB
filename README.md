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
- RES 正式采样链路已迁移为：`RED -> PA1(OPAMP1 VINP) -> OPAMP1 follower -> ADC1(VOPAMP1)`。
- `PC0` 退出 RES 正式采样链，仅保留调试用途。
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
- 比赛态输入：`RIGHT` 短按切档、`RIGHT` 长按切功能；开发后门：`LEFT` 短按进/退 Debug。
- 按键判定语义：短按在 `KEY_EVT_UP` 确认，长按在 `KEY_EVT_LONG` 立即触发；长按后不补发短按。
- 本阶段正式 UI 不再依赖多级菜单树，收敛为：
  - `RUN_MAIN`（比赛页）
  - `RUN_DEBUG`（诊断页）
- `RES` 页面先做 AFE 端点健康检查，再决定是否显示电阻值：
  - `AFE_OK=0`：`R: ----`, `STAT: AFE BAD`
  - `AFE_OK=1`：按 3.5 位格式显示电阻
- 200 档标记为实验档（`200 EXP`），本轮主验收档位为 `2K/20K/200K`。
- `VDC/RES/CONT/DIODE` 已进入正式测量页；`FREQ` 仍为 READY 占位。
- RES 当前使用 OPAMP1 内部跟随器采样链路（`PA1 -> OPAMP1 -> ADC VOPAMP1`），不再走 TL072->PC0 正式路径。
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

## T-1.1.7-R1 范围声明
- 在不改显示底层的前提下保留完整菜单浏览树。
- 输入从 6 键收缩为 4 键：`LEFT/RIGHT/OK/BACK`。
- 启用 `UI_RES_RUN`（手动电阻 live），非运行页测量继续门控。

## T-1.1.8-R1 范围声明
- 正式交互改为“比赛态单键逻辑 + 开发后门”：
  - `RIGHT short`: 档位切换
  - `RIGHT long`: 功能切换
  - `LEFT short`: Debug页切换
- `RES` 计算链路拆分为：
  - `res_acquire_sample`
  - `res_check_afe_health`
  - `res_estimate_rx`
  - `res_format_display`
- 该版本为 OPAMP 迁移前的软件收口基线。

## T-1.1.9-R1 范围声明
- 修复 `RES/VDC` 标题反置：标题/档位/测量调度统一由 mode descriptor 管理。
- 右键体验修复：`RIGHT` 短按在 `UP` 确认，`RIGHT` 长按即时切功能，长按后不补发短按。
- RES 正式采样迁移：`RED -> PA1(OPAMP1 follower) -> ADC VOPAMP1`，`TL072->PC0` 退出正式测量链。
- 端点门控：`SHORT(<50mV)` 与 `OPEN(>0.9*VDDA)` 均通过后才显示电阻值，否则固定 `AFE BAD`。

## T-1.1.10-R1 范围声明
- 目标聚焦 `2K/20K/200K` 的 10x 误差定位：先分离 `R_CALC(公式值, Ω)` 与 `R_DISP(显示串)`，再判断错误落点。
- RES 档位统一改为单表驱动绑定：`档位名 + MUX通道 + Rref(nom/eff) + formatter`，避免多处 switch 错位。
- RES 采样链继续执行 G4 workaround：`dummy-first`（丢首样取次样）+ 量程/通道切换后 settle（默认 `200us`）。
- Debug(RES) 页固定用于诊断：`MUX / RREF / R_CALC / R_DISP / RAW / MV / VDDA / STAT`。
- 正式页仍保留 AFE 门控：端点不过只显示 `R: ----` 与 `STAT: AFE BAD`；200 档继续 `EXP`。

## T-1.1.11-R1 范围声明
- `Rref_eff` 对齐断电实测值（本体阻值，不用上电等效阻值）：
  - `200 -> 1k`
  - `2K -> 10k`
  - `20K -> 100k`
  - `200K -> 1M`
- AFE 判定分离为两套：
  - 历史端点（`short_seen/open_seen`）仅用于 Debug 记录；
  - 正式页显示许可改为“当前采样窗口判定”（`SHORT/OPEN/MID`），不再要求先短接/开路一次。
- 正式页行为：
  - `SHORT`/`OPEN`：`R: ----`，显示对应状态；
  - `MID`：允许显示 `R_CALC -> R_DISP`；
  - 采样错误：`STAT: ERR`。

## T-1.1.12-R1 范围声明
- RES 新增 `AUTO` 挡状态机（锁定+迟滞+settle），不做全量程轮询扫描。
- 进入 AUTO 时优先继承上次锁定子量程；无历史时默认从 `20K` 起步。
- 自动切档采用投票与迟滞边界（连续 3 次后切换），切换后固定 `120ms` 稳定等待。
- AUTO 正式页显示：
  - 第1行 `RES`
  - 第2行 `AUTO <locked_subrange>`（例如 `AUTO 20K`，200档仍标 `EXP`）
  - 第3行主值（正常值/`OL`/`----`）
  - 第4行状态（`OK/AUTO/OPEN/SHORT/ERR`）
- AUTO 边界策略：
  - 最高档仍 OPEN：锁定 `200K`，显示 `OL`
  - 最低档仍 SHORT：锁定 `200`，显示 `SHORT`

## T-1.2.0-R1 范围声明（RES止血 + CONT v1）
- 本轮从 `58a2237 (T-1.1.12-R1)` 基线重开，RES 仅恢复到可用基线，不继续做 `T-1.1.13` polish。
- 新增 `MODE_CONT` v1：
  - 固定复用 RES 低档采样链（内部固定 `RES_RANGE_SEL_200`），不启 AUTO。
  - 状态机：`OPEN -> BEEP_ON -> BEEP_OFF_WAIT`，带迟滞、连续投票和 settle。
  - 默认阈值：`enter=10.0Ω`，`exit=13.0Ω`，`vote=3`，`settle=120ms`。
- 蜂鸣器后端改为 active-buzzer 语义：
  - 当前硬件为 **PNP 高边驱动**，`PB1` 低电平有效。
  - `beep_continuous(true)` -> 响；`beep_continuous(false)` -> 停。
  - 离开 `MODE_CONT` 必须立即关蜂鸣。

## T-1.2.1-R1 启动热修（蜂鸣器迁移）
- 蜂鸣器控制脚从 `PB8` 迁移到 `PB1`，`PB8` 退出蜂鸣器控制链。
- 驱动极性固定为 active-low（PNP 高边）：
  - `PB1=Low` -> 响
  - `PB1=High` -> 静音
- `bsp_init + beep_init` 均确保上电默认静音（高电平），避免启动期误鸣叫。

## T-1.2.2-R1 CONT 体验收口
- 只优化 CONT 响应速度与页面简化，不改 OLED/RES/AUTO 主逻辑。
- CONT 参数收敛：
  - `vote=2`
  - `settle=40ms`
  - 阈值保持 `enter=10Ω / exit=13Ω`
- CONT 测量节拍独立提速到 `25ms`（其他模式保持 `40ms`）。
- CONT 正式页不再显示 `R` 数值，仅显示 `PROBE/BEEP/OPEN` 与状态。
- 进入 Debug 且当前为 CONT 时立即静音；离开 CONT 继续保持立即静音。

## T-1.3.1-R1 二极管测量（方案A）
- 从 `feature/t1.2-cont-r1` 开分支实现方案A单向激励，不做自动双向判极性（方案B）。
- 正式链路复用：`RED -> PA1(OPAMP1 follower) -> ADC1(VOPAMP1)`，并切 `MUX_MODE_DIODE`。
- 新增 `PB0 = DIODE_DRV`：
  - `diode_drv_on()`：`PB0` 推挽输出高（激励打开）
  - `diode_drv_off()`：`PB0` 配置为 `Analog`（Hi-Z，激励关闭）
- 判定阈值：
  - `mv < 50mV` -> `SHORT`
  - `mv > 0.95*VDDA` -> `OL / REV-OPEN`
- 其余 -> `OK`，显示 `A=RED K=BLK` 与 `Vf=x.xxxV`
- 离开 DIODE 模式时 `PB0` 立即回高阻，避免激励在其它模式误保持。

## T-1.4.2-R1 VDC 重做（U11 + U9 接管 PA1）
- 本轮从 `37fc54c` 重开 `feature/t1.4-vdc-r2`，先恢复 RES，再做 VDC bring-up。
- 新硬件映射冻结：
  - `U11 = VOLT_RANGE_MUX`：`2000mV -> CH0`，`20V -> CH1`
  - `U9 = MODE_MUX`：`VDC -> CH0`，`RES/CONT/DIODE -> CH1`（本轮临时共享）
  - `PA1` 仅接 `U9 pin3`，不允许再把模拟前端直并到 PA1。
- VDC 采样流程固定：
  - `mux_set_mode(MUX_MODE_VOLTAGE)`
  - `mux_set_volt_range(current_range)`
  - `adc1_mark_input_path_changed()`
  - `settle(1ms)` + filtered read（内部含 dummy-first）
- 换算口径：
  - `2000mV`：`vin_mv = mv_sense + off_2v`（`off_2v=0`）
  - `20V`：`vin_mv = (mv_sense * 800 + 60) / 120 + off_20v`（`off_20v=0`）
- VDC 异常只显示页面状态（`OL/ERR/MUX BAD/ADC BAD`），不得触发 `Error_Handler` 或 bootdiag fault。

## T-1.4.4-R1 VDC 路径拆分热修（PA1 回归 RES，VDC 改走 PC0）
- 本轮目标是先恢复 RES/CONT/DIODE 的 PA1 正式链稳定性，再把 VDC 从 PA1/U9 共享链剥离。
- 路径冻结：
  - `RES/CONT/DIODE`：继续 `PA1 -> OPAMP1 -> ADC1(VOPAMP1)`。
  - `VDC`：改为 `PC0(ADC12_IN6)` 采样，`U11` 仅负责 `2000mV/20V` 量程选择。
  - `U9` 本轮不参与 VDC 正常路径。
- VDC 软件口径：
  - 采样接口使用 `adc1_read_filtered()`（PC0）。
  - 保留 `adc1_mark_input_path_changed()` 与现有换算公式，不做 20V 补偿改动。
  - `vdc_path_ok()` 仅校验 `VOLT_CH`，不再依赖 `MODE_CH`。
- VDC 异常继续页面化显示（`OK/OL/ADC BAD/ERR`，兼容 `MUX BAD`），不得触发 `Error_Handler` / bootdiag fault。

## T-1.1.5E-R1 历史说明
- `T-1.1.5E-R1` 的 smoke 主分流策略已被 `T-1.1.5F-R1` 统一显示架构替代。
- 当前实验分支：`exp/ui-unify-r1`。

## 冻结引脚映射
- OLED I2C2：`PC4(SCL), PA8(SDA)`
- RES 输入：`PA1 (OPAMP1_VINP0)` -> `OPAMP1 internal output` -> `ADC1(VOPAMP1)`
- 调试 ADC 输入：`PC0 (ADC12_IN6)`（非 RES 正式链）
- RES CD4051：`PD5/PD6/PD7`
- MODE CD4051（U9）：`PB4/PB5/PB6`
- VOLT CD4051（U11）：`PB13/PB14`
- 频率输入捕获：`PA0 (TIM2_CH1)`
- 蜂鸣器：`PB1`（GPIO，active-low，PNP 高边）
- 二极管激励：`PB0`（DIODE_DRV，高=激励，Analog=高阻关闭）
- RGB 心跳灯（Active-Low）：
  - `蓝=PE3`
  - `红=PE4`
  - `绿=PE5`
- 四键：
  - `OK=PC13`
  - `LEFT=PB2`
  - `RIGHT=PB10`
  - `BACK=PB11`
- 正式运行逻辑仅消费 `LEFT/RIGHT`；`OK/BACK` 保留为 dev-only。

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
- 当前硬件主线：外部 TL072 已退出 RES 正式路线，采用 `STM32G474 OPAMP1 VINP -> OPAMP follower -> ADC`。

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
