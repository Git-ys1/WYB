# T-1.5.5 提交态最小验收清单

## Build / Boot
- [ ] `S55-BUILD-01`: Headless clean build 通过（0 error, 0 warning）。
- [ ] `S55-BOOT-01`: 上电先显示 2 秒开机中文画面，然后进入主 UI。
- [ ] `S55-BOOT-02`: 运行 5 分钟无黑屏/花屏/卡死。

## 输入与蜂鸣器单链
- [ ] `S55-KEY-01`: 正式固件只使用 RIGHT（功能）和 LEFT（调试开关）。
- [ ] `S55-KEY-02`: 关闭 LEFT 编译开关后，固件仍可正常运行（无第二输入链依赖）。
- [ ] `S55-BEEP-01`: 蜂鸣器仅由 PB1 active-low 控制；非 CONT 模式不持续鸣叫。

## 模式契约
- [ ] `S55-MODE-01`: 模式映射固定 `CH0=VOLTAGE, CH1=RES/CONT, CH2=DIODE, CH3=FREQ`。
- [ ] `S55-MODE-02`: 代码/页面/文档不再出现 `MUX_MODE_AC` 旧命名。

## FREQ 主链（PA0/TIM2）
- [ ] `S55-FREQ-01`: FREQ 模式 debug 可见 `MODE/IRQ/CCR/P/H/CLK/F/D/ERR`。
- [ ] `S55-FREQ-02`: 1kHz/50% 输入下 IRQ 与 CCR 计数递增，P/H/CLK 有效。
- [ ] `S55-FREQ-03`: 开路时显示 `NO SIG`，且超时行为受档位 profile 控制。
- [ ] `S55-FREQ-04`: 高频可通过 `accum_cycles` 做稳定性调参，不需改主循环。

## 回归
- [ ] `S55-REG-RES-01`: RES 行为无回归。
- [ ] `S55-REG-DIODE-01`: DIODE 行为无回归。
- [ ] `S55-REG-CONT-01`: CONT 行为无回归。

## 已知问题备注
- [ ] `S55-KNOWN-VDC20`: VDC 20V 档问题已在 README 标注为已知问题，未在本轮混修。

## T-1.6.0-R1 Hotfix（开机位图 + FREQ 单链）
- [ ] `SPLASH-01`: 上电开机页显示 2 秒，`23291043` 完整可见且不贴边。
- [ ] `FREQ-01`: `1kHz/50%` 输入下 `IRQ/CH1/CH2` 计数递增，`CCR1/CCR2` 持续变化。
- [ ] `FREQ-02`: `P/H/CLK` 有效，主页面可显示 `F:xxxxHz` 与 `D:xx%`。
- [ ] `FREQ-03`: 开路状态恢复 `NO SIG`，不出现假值常驻。
- [ ] `FREQ-04`: 五档切换无卡死，错误只在页面可诊断（不触发 fault）。
- [ ] `REG-01`: `RES/DIODE/CONT` 行为无回归。
- [ ] `REG-02`: 非 CONT 模式无持续蜂鸣。

## T-1.6.1-R2 Hotfix（位图微调 + RES 默认AUTO + FREQ自动挡/越档约束）
- [ ] `R2-SPLASH-01`: 上电 2 秒位图三行完整显示，`23291043` 无贴边/裁切。
- [ ] `R2-RES-01`: 开机进入 RES 首屏默认 `AUTO`。
- [ ] `R2-FREQ-01`: FREQ 默认档为 `AUTO`，调试页可见当前选择档与激活档。
- [ ] `R2-FREQ-02`: 输入 `10Hz/100Hz/1kHz/10kHz/100kHz` 时，AUTO 可锁到最小可容纳档。
- [ ] `R2-FREQ-03`: 手动档越档显示 `OVER`，并保留上次稳定频率/占空比。
- [ ] `R2-FREQ-04`: `20Hz` 档不再显示 `10kHz` 为有效值。
- [ ] `R2-FREQ-05`: 重点频点 `10k/11k/12k/15k` 下不再卡死，错误状态可控。
- [ ] `R2-FREQ-06`: 开路仍可恢复 `NO SIG`。
- [ ] `R2-REG-01`: `RES/DIODE/CONT` 行为无回归，非 CONT 模式无持续蜂鸣。

## T-1.6.2-R1 Hotfix（位图重画 + VDC默认AUTO + FREQ OL/自恢复）
- [ ] `R3-SPLASH-01`: 开机位图三行重新居中，`23291043` 无裁切，2 秒后进入主 UI。
- [ ] `R3-VDC-01`: 切入 VDC 默认即 `AUTO`，主页面显示 `RNG: AUTO <2V/20V>`。
- [ ] `R3-VDC-02`: `AUTO` 在 `|Vin|>1800mV` 连续命中后升到 `20V`，`|Vin|<1500mV` 连续命中后降到 `2V`。
- [ ] `R3-VDC-03`: VDC Debug 可见当前生效档位（2V/20V）。
- [ ] `R3-FREQ-01`: 手动档越档时主值直接显示 `OL`，不再显示频率数值+OVER 并存。
- [ ] `R3-FREQ-02`: FREQ AUTO 超过 `200kHz` 时显示 `OL`，不死机不卡主循环。
- [ ] `R3-FREQ-03`: 高频异常场景下可触发一级显示链恢复；恢复失败时每次进入 FREQ 最多软复位 1 次。
- [ ] `R3-FREQ-04`: 开路仍显示 `NO SIG`，正常信号下频率/占空比可恢复显示。
- [ ] `R3-REG-01`: `RES/DIODE/CONT` 行为无回归，非 CONT 模式无持续蜂鸣。

## T-1.6.2-R3 Hotfix（FREQ 高频减负 + 主界面显示死区）
- [ ] `R3F-BASE-01`: `SystemClock_Config` 维持 `HSI 16MHz + PLL_NONE`，I2C2 16MHz 口径不变。
- [ ] `R3F-CAP-01`: TIM2 仅 `CH1` 走 `HAL_TIM_IC_Start_IT`，`CH2` 走 `HAL_TIM_IC_Start`。
- [ ] `R3F-CAP-02`: `HAL_TIM_IC_CaptureCallback` 仅在 `HAL_TIM_ACTIVE_CHANNEL_1` 分支处理。
- [ ] `R3F-PROFILE-01`: `accum_cycles` 分档生效：`20Hz=4, 200Hz=2, 2k/20k/200k=1`。
- [ ] `R3F-DBG-01`: 1kHz/50% 下 `IRQ/C1/C2/P/H/CLK` 可诊断且 `invalid_h_gt_p_count` 不持续飙升。
- [ ] `R3F-STAB-01`: 10k/11k/12k/15k 输入下主界面不假死，心跳不被拖垮。
- [ ] `R3F-DEADBAND-01`: 主界面抖动下降，Debug 页仍实时显示原始值。
- [ ] `R3F-DEADBAND-02`: 档位变化、AUTO 切档、`NO SIG/OL/OVER` 进出可立即刷新。
- [ ] `R3F-REG-01`: `RES/DIODE/CONT/VDC` 无回归，非 CONT 模式无持续蜂鸣。

## T-1.6.5-R1（VDC 根因锁定 + FREQ 高频收尾）
- [ ] `R5-VDC-ROOT-01`: README/Checklist 明确写入 20V 漂移根因为 2V 支路污染（R10 + VIN_2V + 钳位）。
- [ ] `R5-VDC-ROOT-02`: README/Checklist 明确写入非根因：非 170MHz 不足、非 20V 公式首要问题。
- [ ] `R5-VDC-ROOT-03`: README/Checklist 记录关键证据：断开 R10 后 20V 稳定恢复。
- [ ] `R5-VDC-PROTECT-01`: VDC 异常仅页面化（OK/OL/MUX BAD/ADC BAD/ERR），不触发 PB12 fault。
- [ ] `R5-FREQ-CHAIN-01`: 1kHz/50% 下 `IRQ/CCR/P/H/CLK/F/D` 连续有效。
- [ ] `R5-FREQ-HOP-01`: 直接跳频（1k->10k/20k）不长时间卡在错误档位，主界面不假死。
- [ ] `R5-FREQ-SWEEP-01`: 缓慢扫频上限不退化，至少维持当前可测能力。
- [ ] `R5-FREQ-OPEN-01`: 开路可回 `NO SIG`，无假值粘连。
- [ ] `R5-REG-01`: `RES/DIODE/CONT` 行为无回归，非 CONT 模式无持续蜂鸣。

## T-1.6.6-R1（170MHz FREQ 实验线）
- [ ] `R6E-CLOCK-01`: 170MHz 启动正常（Range1 Boost + Flash Latency 7），OLED 正常。
- [ ] `R6E-I2C-01`: I2C2 在 170MHz 下稳定（内核 HSI16 + timing 常量口径），无总线异常。
- [ ] `R6E-FREQ-01`: 1kHz/50% 下 `IRQ/CCR/P/H/CLK/F/D` 连续有效。
- [ ] `R6E-FREQ-02`: 高频点 `10k/20k/50k` 可测，直接跳频不长时间卡屏。
- [ ] `R6E-FREQ-03`: 手动档越档立即 `OL`，且不会沿用旧平滑历史。
- [ ] `R6E-FREQ-04`: AUTO 对大步跳频可快速升档，不长时间停留错误档位。
- [ ] `R6E-FREQ-05`: 高范围无效捕获时可快速丢弃旧窗口并恢复。
- [ ] `R6E-REG-01`: `RES/DIODE/CONT/VDC` 行为无回归，非 CONT 模式无持续蜂鸣。

## T-1.6.7-R1（170MHz：VDC 20V 分段校准）
- [ ] `R7-VDC20-01`: 20V 档在 `2.0/4.5/7.5/8.0/9.0/10.0/11.5/13.0/15.0/18.0/19.0V` 点位误差大多数收敛到 ±0.1V 内。
- [ ] `R7-VDC20-02`: 高压端不再出现先前明显偏大，且接近满量程不引入新异常。
- [ ] `R7-VDC20-03`: AUTO 进入 20V 路时主界面正确显示修正后的数值。
- [ ] `R7-VDC20-04`: VDC Debug 同屏可见 `VRAW20`（修正前）与 `VCOR20`（修正后）。
- [ ] `R7-REG-01`: 2V 档行为不变；`RES/DIODE/FREQ/CONT` 无回归。

## T-1.7.1-R1（FREQ 单位优化 + 不做波形识别）
- [ ] `R71-FMT-01`: `999Hz` 显示 `999 Hz`。
- [ ] `R71-FMT-02`: `1000Hz` 显示 `1.000 kHz`。
- [ ] `R71-FMT-03`: `1234Hz` 显示 `1.234 kHz`。
- [ ] `R71-FMT-04`: `10013Hz` 显示 `10.01 kHz`（按两位小数规则）。
- [ ] `R71-FMT-05`: `47250Hz` 显示 `47.25 kHz`。
- [ ] `R71-FMT-06`: `156300Hz` 显示 `156.3 kHz`。
- [ ] `R71-ERR-01`: `OL/NO SIG/OVER` 显示语义不回归，不做单位切换。
- [ ] `R71-FREEZE-01`: 不出现方波/正弦波自动识别标签或判定输出。

## T-1.7.5-R1（CAP：RC+ADC 阈值法基础版）
- [ ] `R75-CAP-01`: 进入 CAP 模式后，MODE MUX 走 `MUX_MODE_CAP`，并可在 `20nF/2uF/200uF` 三档切换。
- [ ] `R75-CAP-02`: `20nF` 档测 `203`（约 20nF）可出稳定量级结果。
- [ ] `R75-CAP-03`: `2uF` 档测 `205`（约 2uF）可出稳定量级结果。
- [ ] `R75-CAP-04`: `200uF` 档测电解（正接 CAP、负接 GND）可出量级结果。
- [ ] `R75-CAP-05`: 超时/无电容时显示 `OL`，不死机、不阻塞模式切换。
- [ ] `R75-CAP-06`: CAP 调试页可见 `ADC/THR/CYC/STAT`，可用于阈值与计时核对。
- [ ] `R75-CAP-07`: 连续切换 `RES->DIODE->CAP->FREQ->RES` 无黑屏、无卡死。
- [ ] `R75-REG-01`: `RES/DIODE/CONT/FREQ/VDC` 行为无回归。
