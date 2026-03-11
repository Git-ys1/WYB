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
