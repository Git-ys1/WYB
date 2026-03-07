# T-1.1.4I Validation Checklist

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
