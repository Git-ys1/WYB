# T-1.1.4I Validation Checklist

## T-1.1.13-R2 RES Hotfix
- [ ] `R13R2-KEY-01`: RES 模式 `RIGHT` 短按切档即时反馈（量程标题立即变化）。
- [ ] `R13R2-KEY-02`: `RIGHT` 长按切功能稳定，且不补发短按切档。
- [ ] `R13R2-SETTLE-01`: 量程切换 settle 非阻塞（约100ms），UI 无卡顿。
- [ ] `R13R2-DISP-01`: 不再出现 `Rcalc` 正常但 `Rdisp` 恒 `0/OL/---`。
- [ ] `R13R2-RES-01`: 电阻模式量程内优先显示数值，不误报 `SHORT`。
- [ ] `R13R2-RES-02`: 开路/超量程显示 `OL`；仅近0Ω显示 `SHORT`。
- [ ] `R13R2-DBG-01`: Debug 继续可见 `RAW/MV/Rcalc/Rdisp`（含既有 `RREF/MUX`）。
- [ ] `R13R2-AUTO-01`: AUTO 行为无回归（锁档与显示稳定）。
- [ ] `R13R2-STABLE-01`: 连续 5 分钟无黑屏/花屏，单写者不破坏。

## T-1.1.12-R1 RES AUTO Range
- [ ] `A12-MANUAL-REG`: 手动四档（200/2K/20K/200K）无回归。
- [ ] `A12-AUTO-ENTER`: 可进入 AUTO，页面稳定显示 `AUTO <locked_subrange>`。
- [ ] `A12-AUTO-LOCK-100R`: 100Ω 最终锁到 200。
- [ ] `A12-AUTO-LOCK-1K`: 1kΩ 最终锁到 2K。
- [ ] `A12-AUTO-LOCK-10K`: 10kΩ 最终锁到 20K。
- [ ] `A12-AUTO-LOCK-100K`: 100kΩ 最终锁到 200K。
- [ ] `A12-AUTO-OPEN`: 开路显示 `OL`，并锁在 200K。
- [ ] `A12-AUTO-SHORT`: 短路显示 `SHORT/近0`，并锁在 200。
- [ ] `A12-AUTO-NO-CHATTER`: 分界点附近无来回狂跳（迟滞+投票生效）。
- [ ] `A12-DBG-01`: Debug 可见 `AUTO_RNG` 与 `VOTE(U/D)`。
- [ ] `A12-STABLE-5MIN`: 连续 5 分钟无黑屏/花屏，单写者不破坏。

## T-1.1.11-R1 Rref_eff Alignment + Live Window Gate
- [ ] `R11-RREF-01`: `RREF_NOM/RREF_EFF` 与实测本体值一致（200/2K/20K/200K -> 1k/10k/100k/1M）。
- [ ] `R11-CALC-01`: `2K/20K/200K` 三档下 `R_CALC` 不再稳定偏 10x。
- [ ] `R11-DISP-01`: `R_DISP` 仅承担格式化，不再引入额外 decade 偏差。
- [ ] `R11-GATE-01`: 正式页不再要求先 short/open 一次才显示。
- [ ] `R11-GATE-02`: 当前样本为 `SHORT/OPEN` 时，正式页显示 `R: ----` 与对应状态。
- [ ] `R11-DBG-01`: RES Debug 页同屏可见 `MUX/RN/RE/R_CALC/R_DISP/RAW/MV/VDDA/STAT`。
- [ ] `R11-RANGE-01`: 200 档继续 `EXP`，主验收仍以 `2K/20K/200K` 为准。
- [ ] `R11-STABLE-01`: 连续运行 5 分钟无黑屏/花屏，单写者不破坏。

## T-1.1.10-R1 RES 10x Error Localization
- [ ] `R10-TITLE-01`: `RES/VDC` 第一行标题稳定正确（不反置）。
- [ ] `R10-KEY-01`: RIGHT 短按/长按体验正常，LEFT 短按进退 Debug 正常。
- [ ] `R10-DBG-01`: RES Debug 页同屏可见 `MUX/RREF/R_CALC/R_DISP/RAW/MV/VDDA/STAT`。
- [ ] `R10-UNIT-01`: 单位审计完成（raw=count, mv/vdda=mV, rref/r_calc=Ω）。
- [ ] `R10-BIND-01`: 档位绑定表统一 `title+mux+rref+formatter`，无分散 switch 错位。
- [ ] `R10-ADC-01`: RES采样执行 `dummy-first`，量程切换后执行 settle（默认 200us）。
- [ ] `R10-RES-2K`: 2K 档可记录并判断 `R_CALC` 与 `R_DISP` 是否一致。
- [ ] `R10-RES-20K`: 20K 档可记录并判断 `R_CALC` 与 `R_DISP` 是否一致。
- [ ] `R10-RES-200K`: 200K 档可记录并判断 `R_CALC` 与 `R_DISP` 是否一致。
- [ ] `R10-AFE-01`: 正式页端点不过时固定 `R: ---- / STAT: AFE BAD`。
- [ ] `R10-RANGE-01`: 200 档继续标记 `EXP`，不作为本轮主验收。
- [ ] `R10-STABLE-01`: 连续运行 5 分钟无黑屏/花屏，单写者未破坏。

## T-1.1.9-R1 Competition-Key + OPAMP1 RES Chain
- [ ] `R9-TITLE-01`: `RES` 与 `VDC` 第一行标题正确，无反置。
- [ ] `R9-KEY-01`: `RIGHT` 短按在 `UP` 触发切档，长按触发切功能，体验无迟钝/断触。
- [ ] `R9-KEY-02`: `LEFT` 短按（UP确认）稳定进/退 Debug。
- [ ] `R9-CHAIN-01`: RES 采样链为 `PA1 -> OPAMP1 -> ADC(VOPAMP1)`，不再依赖 `TL072->PC0`。
- [ ] `R9-DEBUG-01`: Debug 页显示 `RAW/MV/VDDA/STAT`，并标注 `AFE:OP1`。
- [ ] `R9-AFE-01`: 端点门控生效：未通过时固定 `R: ---- / STAT: AFE BAD`。
- [ ] `R9-AFE-02`: `2K/20K/200K` 三档完成 SHORT/OPEN/100Ω/1kΩ/10kΩ/100kΩ 记录。
- [ ] `R9-RANGE-01`: `200` 档明确为 `EXP`，不作为主验收档。
- [ ] `R9-STABLE-01`: 连续 5 分钟无黑屏/花屏。
- [ ] `R9-WRITER-01`: 无第二个 flush 调用点。

## T-1.1.8-R1 Competition-Key UI + RES AFE Gate
- [ ] `R8-KEY-01`: `RIGHT` 短按切档位，`RIGHT` 长按切功能。
- [ ] `R8-KEY-02`: `LEFT` 短按可进入/退出 Debug 页。
- [ ] `R8-RES-01`: AFE 未通过时固定显示 `R: ----` 与 `STAT: AFE BAD`。
- [ ] `R8-RES-02`: AFE 通过后才显示电阻值（非伪结果）。
- [ ] `R8-RES-03`: 200 档显示 `200 EXP`。
- [ ] `R8-AFE-01`: 2K/20K/200K 三档完成 `SHORT_SEEN/OPEN_SEEN` 端点记录。
- [ ] `R8-STABLE-01`: 连续 5 分钟无黑屏/花屏。
- [ ] `R8-WRITER-01`: 无第二个 flush 调用点。

## T-1.1.7-R1 4-Key + RES Manual Live
- [ ] `R7-KEY-01`: 代码中不再产生 `KEY_UP/KEY_DOWN` 事件。
- [ ] `R7-KEY-02`: 仅用 `LEFT/RIGHT/OK/BACK` 可完整浏览菜单树。
- [ ] `R7-PATH-03`: 可进入 `MEASURE -> RES -> RANGE -> READY -> RUN`。
- [ ] `R7-RUN-04`: `UI_RES_RUN` 实时显示 `R/MV/RAW/STAT`。
- [ ] `R7-TREND-05`: 更换电阻后 `R` 趋势正确（2K/20K/200K 各至少一次）。
- [ ] `R7-200-06`: 200 档显示实验标识（`EXP`）。
- [ ] `R7-GATE-07`: 非 `UI_RES_RUN` 页面 `app_measure_tick()` 不执行电阻采样。
- [ ] `R7-STABLE-08`: 连续 5 分钟无黑屏/花屏。
- [ ] `R7-SPINE-09`: 断开 OLED 时 PB12 诊断行为保持。
- [ ] `R7-SINGLE-WRITER-10`: 正式路径仍只有 display service 的 flush 调用点。

## T-1.1.6A-R1 Menu Tree Browsing (Run Disabled)
- [ ] `A6-BOOT-DIAG`: 上电进入诊断页，连续 5 分钟无黑屏/花屏。
- [ ] `A6-DIAG-GATE`: 3 秒后自动解锁菜单；3 秒内按 `OK` 可立即进菜单。
- [ ] `A6-TREE`: 主菜单 / DEBUG菜单 / ADC DEBUG / BOOT INFO / MEASURE菜单 / RES RANGE / 各READY页 可完整浏览。
- [ ] `A6-KEYS`: `UP/DOWN/OK/BACK` 行为正确；主菜单 `LEFT/RIGHT` 可横向切模块。
- [ ] `A6-ADC-DEBUG`: `UI_DEBUG_ADC` 持续更新 `RAW/MV/VDDA/STAT`。
- [ ] `A6-MEAS-GATE`: READY页与浏览页均不启动真实测量，`RES_RUN` 不可达。
- [ ] `A6-SINGLE-WRITER`: 仍只有 `app_display_service.c` 为正式 flush 调用点。
- [ ] `A6-BOOT-SPINE`: 断开 OLED 仍有 PB12 fault 码（预期3闪），boot spine 未被破坏。

## T-1.1.5G-R1 Boot Spine Diagnostics
- [ ] `G1-BOOT-LED`: PB12 在 Boot 快闪、Run 慢闪、Fault 按码闪烁。
- [ ] `G1-FAULT-CODE`: 断 OLED / 强制显示初始化失败时，fault code 可区分（3 或 4）。
- [ ] `G1-ERR-HANDLER`: `Error_Handler()` 进入前已写入 `BOOT_FAULT + fault code`。
- [ ] `G1-DIAG-PAGE`: 显示 ready 后最小诊断页可见（`BOOT OK/STAGE/FAULT/RAW/MV`）。
- [ ] `G1-DIAG-STABLE-5MIN`: 最小诊断页连续 5 分钟无黑屏/花屏。
- [ ] `G1-MENU-GATE`: 诊断页未稳定 10 秒前不进入菜单壳。
- [ ] `G1-SINGLE-WRITER`: 正式路径唯一 flush 调用点仍在 `app_display_service.c`。

## T-1.1.5F-R1 Unified Display + Menu Shell
- [ ] `F1-BOOT`: 关闭 smoke 主流程分流后，上电直接显示 `MAIN MENU`。
- [ ] `F1-DEBUG`: `DEBUG/ADC` 页稳定显示 `RAW/MV/VDDA/STAT`。
- [ ] `F1-MENU`: `L1/L2/L3/L4(RES READY)` 可进入、返回、切换。
- [ ] `F1-STABLE-5MIN`: 连续 5 分钟无黑屏/花屏。
- [ ] `F1-SINGLE-WRITER`: 正式路径只有一个底层 flush 调用点（`app_display_service`）。
- [ ] `F1-MEAS-GATE`: 非 `MENU_L4_RES_RUN` 时 `app_measure_tick()` 不执行真实测量。

## T-1.1.5E-R1 Main Recovery + Experiment Isolation
- [ ] `E1-REVERT`: `main` 已执行 `revert c9ca804`，固定 smoke 页恢复。
- [ ] `E1-BUILD`: Clean + Build 通过，产出 `Debug/WYB.hex`。
- [ ] `E1-BASELINE`: 上电显示固定 ADC 页（`RAW / MV / VDDA / STAT`）。
- [ ] `E1-STABLE-5MIN`: 连续 5 分钟无黑屏/花屏。
- [ ] `E1-ADC-2PT`: `PC0->GND/3.3V` 两点趋势正确。
- [ ] `E1-FALLBACK`: 实验路径 presenter 失败时可见 fallback 页面，不永久黑屏。
- [ ] `E1-DEFAULT`: `main` 默认 `APP_MENU_REINTEGRATION_EXPERIMENT=0`（不开菜单实验）。
- [ ] `E1-BRANCH`: 菜单重构迁入 `exp/menu-shell-r2b` 并已推送。

## T-1.1.5D-R0 Rollback Smoke Baseline
- [ ] `D0-BOOT`: 固定文本页 `OLED TXT OK / RAW / MV / VDDA / STAT` 恢复显示。
- [ ] `D0-CHAR`: `V/W/X/Y/Z` 显示完整无缺字。
- [ ] `D0-ADC-GND`: `PC0 -> GND` 时 RAW/MV 接近低端。
- [ ] `D0-ADC-3V3`: `PC0 -> 3.3V` 时 RAW/MV 接近高端。
- [ ] `D0-STABLE-5MIN`: 连续 5 分钟无花屏/黑屏。

## T-1.1.5C-R1 Character + Menu Shell
- [ ] `T15C-TXT-01`: `VWXYZ` 与大写字母页显示完整（允许行尾裁剪，但不得缺字）。
- [ ] `T15C-TXT-02`: `OLED TXT OK / RAW / MV / VDDA / STAT` 固定文本页完整显示。
- [ ] `T15C-MENU-01`: `L1/L2/L3/L4(RES READY)` 可进入、返回、切换稳定。
- [ ] `T15C-MENU-02`: `UP/DOWN/LEFT/RIGHT/OK/BACK` 行为符合定义。
- [ ] `T15C-STABLE-03`: 菜单首页连续运行 5 分钟无花屏/黑屏/卡死。
- [ ] `T15C-GATE-04`: 未进入真实运行页前，`app_measure_tick()` 不执行测量刷新。
- [ ] `T15C-GATE-05`: `L4` 仅显示占位页，不触发高频 ADC/RES 运行。

## T-1.5.1A Fixed Text + ADC1 Bring-up
- [ ] `T151A-BUILD-01`: headless clean build 通过，`init_b` unused warning 消失。
- [ ] `T151A-OLED-TXT-01`: 固定文本页（`OLED TXT OK / ADC INIT...`）连续 5 分钟无黑屏/花屏。
- [ ] `T151A-ADC-INIT-02`: `adc1_init()` 成功，`STAT: OK`。
- [ ] `T151A-ADC-GND-03`: `PC0 -> GND` 时 `RAW≈0`、`MV≈0`。
- [ ] `T151A-ADC-3V3-04`: `PC0 -> 3.3V` 时 `RAW` 接近满量程、`MV` 接近 3300。
- [ ] `T151A-ADC-RED-05`: `PC0 -> RED` 时 `RAW/MV` 随节点变化。
- [ ] `T151A-ADC-ERR-06`: ADC 异常时 OLED 仍刷新且显示 `STAT: ERRn`。

## T-1.1.4M-R3 OLED Bottom-Layer Check
- [ ] `T114M-R3-01`: 冷上电（断电>=2s）后 probe 是否改善（是否仍 1 blink）。
- [ ] `T114M-R3-02`: 按住RST上电再释放，与冷上电结果对比已记录。
- [ ] `T114M-R3-03`: 命令链路 `A5 -> A4` 可见。
- [ ] `T114M-R3-04`: 图案链路 `BLACK -> WHITE -> STRIPE -> CHECKER` 顺序正确且规整。
- [ ] `T114M-R3-05`: 若 3 blink，已记录失败落点 `stage/page/chunk/HAL status`。
- [ ] `T114M-R3-06`: 分包后花屏是否减少的结论已填写。
- [ ] `T114M-R3-07`: 若仍完全一致失败，已执行“暂停主工程修补，转模块交叉验证”决策。

## T-1.1.4L OLED Smoke (R1+R2)
- [ ] `T114L-PH1-01`: `A5 -> A4` 命令链路可见。
- [ ] `T114L-PH2-02`: `BLACK -> WHITE -> AA55 -> CHECKER` 图案链路可见且稳定。
- [ ] `T114L-PH3-03`: 仅在 PH2 正常后显示 `OLED OK / PROF / ADDR / CNT`。
- [ ] `T114L-PROF-04`: 自动顺序 `1315 -> 1306 -> 1106` 观察结果已记录。
- [ ] `T114L-ERRLED-05`: PB12 错误码 `1/2/3/4` 行为与定义一致。
- [ ] `T114L-CONCLUDE-06`: 完成“底层链路 vs 架构污染”判定结论。

## T114I-BUILD-01
- [ ] `clean + build` 0 error，产出 `Debug/WYB.hex`。

## T114I-REC-00
- [ ] 不按住复位连续下载 3 次，均可连接并运行。

## T114I-BOOT-01
- [ ] 上电 1 秒内显示固定 BOOT 两行文本（`BOOT OK` + 版本）。

## T114I-BLACK-LOCATE-04
- [ ] 断开 OLED 时，`bootdiag stage` 停在失败段（`EX1~EX4`），系统不死机。

## T114I-I2C-HW-02
- [ ] OLED 正常连线时优先 `HW I2C2`，稳定显示 5 分钟。

## T114I-I2C-FALLBACK-03
- [ ] 人为让 HW 探测失败后，状态机可切到 `SW` 路径并继续尝试点亮。
- [ ] DEBUG 页 `OLED NACK` 计数可见且随异常增长。

## T114I-RGB-06
- [ ] 可见 `蓝->红->绿` 上电自检序列。
- [ ] 后续状态满足：`BOOT红 / RUN绿闪 / FAULT红`。
- [ ] 若 RGB 仍无响应，DEBUG 仍可显示 `RGB:UNK` 且不影响 OLED 诊断。

## T114I-HB-07
- [ ] 暂停 `hb_kick` 约 1 秒后进入 FAULT。

## T-REC (先恢复可调试)
- [ ] `Connect Under Reset` 可连接并完成一次下载。
- [ ] 不按住 `RST` 也可重复连接/下载 3 次以上。
- [ ] 上电后 5 分钟内无复位风暴/死机。

## T-POWER
- [ ] 测量 MCU `3V3` 是否稳定。
- [ ] 测量 `5V`（如使用）是否正常。
- [ ] 确认 I2C 上拉电阻仅接 `3.3V`。
- [ ] 确认 ADC 输入节点（PC0）电压始终在 `0~3.3V`。

## T-I2C2 (OLED)
- [ ] 扫描到 OLED 地址 `0x3C` 或 `0x3D`。
- [ ] 可稳定显示 DEBUG 页面与菜单页面。
- [ ] 连续刷新无花屏/卡死。
- [ ] OLED 初始化失败后可自动执行 I2C2 总线恢复并重新点亮。

## T-RGB-HB (Heartbeat)
- [ ] 上电初始化阶段红灯常亮，绿灯灭。
- [ ] 正常运行时绿灯闪烁（可见慢/中/快三档变化）。
- [ ] 停止 `hb_kick()` 约 1s 后进入 FAULT（红常亮、绿灭）。
- [ ] OLED 连续恢复失败达到阈值后进入 FAULT，恢复后可退出 FAULT。

## T-ADC-BRINGUP (Internal ADC1 / PC0)
- [ ] `adc1_init()` 成功。
- [ ] `ADC raw` 与 `Vred` 有效更新（非固定 0/满量程）。
- [ ] `ADC stat` 为 `OK`（无持续 `TIMEOUT/OVR`）。

## T-ADC-VDDA (VREFINT)
- [ ] `adc1_read_vdda_mv()` 可返回合理 VDDA（接近 3300mV）。
- [ ] VREFINT 失败时能自动回退 `3300mV`，系统不死机。

## T-MUX (CD4051)
- [ ] 翻转 RES 地址线 `PD5/PD6/PD7`，位序正确。
- [ ] 翻转 MODE 地址线 `PB4/PB5/PB6`，位序正确。
- [ ] 翻转 VOLT 地址线 `PB13/PB14`，位序正确。

## T-RES-MANUAL (2K/20K/200K)
- [ ] 三档切换后 `ADC raw`/`Vred` 有可见变化。
- [ ] 接入 `10k/100k/1M` 时 `Rx` 单调递增，趋势正确。
- [ ] 开路时显示 `OVERRANGE/OPEN`。
- [ ] `AUTO/200` 档显示 `NOT READY` 且系统不中断。

## T-IC (PA0 input capture)
- [ ] 注入 `1kHz, 30%` 方波。
- [ ] 频率显示稳定。
- [ ] 占空比显示稳定（约 1% 级）。

## T-ERR (Error and recovery)
- [ ] 拔 OLED：系统保持运行，可恢复。
- [ ] 模拟 ADC 异常：UI 显示 `TIMEOUT/OVR`，系统不崩溃。
- [ ] 断开 PA0 输入：显示 `NO_SIGNAL` 状态。

## T-1.1.4I Run Record
- Date: 2026-03-07
- Firmware: T1.1.4I-20260307
- ADC path: internal ADC1 on PC0 (ADS1110 removed)

| Case | Input/Condition | Expected | Result | Notes |
|---|---|---|---|---|
| T-REC-01 | STLink attach after power-on | Connect without holding RST | PENDING | Verify SWD retention |
| T-REC-02 | Download 3 times continuously | No lock-up after run | PENDING | Record pass/fail |
| T-OLED-01 | Power on then wait <=1s | BOOT/DEBUG text visible | PENDING | Confirm full-feature startup path |
| T-OLED-02 | Run 5 minutes continuously | No static mosaic / no freeze | PENDING | Verify dirty flush recover path |
| T-OLED-03 | Force I2C2 disturbance then recover | Auto recover and redraw | PENDING | Verify bus recover + reinit |
| T-HB-01 | BOOT state | Red on / Green off | PENDING | Check active-low polarity |
| T-HB-02 | RUN state with load hints | Green blink speed changes | PENDING | Debug/menu/res pages |
| T-HB-03 | No kick for ~1s | Enter FAULT (Red on) | PENDING | Comment hb_kick test |
| T-I2C2 | OLED on I2C2, addr 0x3C/0x3D | Probe + refresh pass | PENDING | Fill after bench test |
| T-ADC-BRINGUP | PC0 connected to RED node | raw/mV updates, ADC stat OK | PENDING | Fill raw/mV samples |
| T-ADC-VDDA | Read VREFINT-derived VDDA | Around actual 3V3 rail | PENDING | Fill measured VDDA |
| T-MUX | Toggle PB4/5/6 + PD5/6/7 + PB13/14 | Levels flip and ADC trend changes | PENDING | Fill channel map |
| T-RES-MANUAL | 2K/20K/200K with known resistors | Rx monotonic with resistance | PENDING | Fill measured values |
| T-IC | PA0 inject 1kHz/30% square | Stable Freq + Duty | PENDING | Fill measured Hz/% |
| T-NO-ADS | ADS1110 not connected | System still runs normally | PENDING | Confirm no ADS scan text |
| SW-BUILD | CubeIDE one-click build | No compile/link error | DONE | T-1.1.4 migration build passes |
