# T-1.1.2A/T-1.1.4：6键菜单 + RES 手动档说明

版本：`T1.1.4-20260306`

## 1. 按键引脚（运行时代码初始化）

- `OK`: `PC13`
- `UP`: `PB0`
- `DOWN`: `PB1`
- `LEFT`: `PB2`
- `RIGHT`: `PB10`
- `BACK`: `PB11`

按键接法：引脚到 GND，GPIO 上拉，按下为 `0`（有效低）。

## 2. 按键事件模型

轮询参数：
- 轮询周期：`1ms`
- 消抖（按下/释放）：`2ms / 8ms`
- 长按阈值：`600ms`

事件：
- `KEY_EVT_DOWN`
- `KEY_EVT_UP`
- `KEY_EVT_LONG`
- `KEY_EVT_REPEAT`（预留，当前不产生）

短按动作触发：`KEY_EVT_DOWN`（按下稳定即执行），`KEY_EVT_UP` 仅作释放事件保留。
交互优先策略：按键事件发生后，测量任务延后约 `60ms`，优先刷新菜单页面。

UI 刷新策略：
- `L1/L2/L3` 菜单页改为事件驱动（按键后立即刷新，不再固定 120ms 周期轮询重绘）
- `L4` 运行页按测量变化驱动，并限制最小刷新周期约 `80ms`
- OLED 使用脏页刷新（仅传变化页），减少 I2C 阻塞

## 3. 菜单层级

- `L1_MODULE`：模块选择（`DEBUG` / `MEAS`）
- `L2_DEBUG_PAGE`：调试页面（沿用 T-0.1.12）
- `L2_MEAS_FUNC`：测量功能（当前仅 `RES`）
- `L3_RES_RANGE`：量程选择（`AUTO/200/2K/20K/200K`）
- `L4_RES_RUN`：电阻手动测量运行页

按键逻辑：
- 短按 `UP/DOWN`：选择项移动
- 短按 `LEFT/RIGHT`：一级切模块；在量程页/运行页切换量程
- 短按 `OK`：进入下一级
- 短按 `BACK`：返回上一级
- 长按 `OK`（MEAS 层）：快速回一级菜单

## 4. RES 手动档实现（片内 ADC 版本）

本轮可用档位：
- `2K` -> `CH1` -> `Rref_eff = 10k`
- `20K` -> `CH2` -> `Rref_eff = 100k`
- `200K` -> `CH3` -> `Rref_eff = 1M`

占位档位：
- `AUTO` -> `NOT READY`
- `200` -> `NOT READY`

测量流程：
1. `mux_set_mode(MUX_MODE_RES)`
2. `mux_set_res_range(...)`
3. `ADC1(PC0)` 读取滤波值（`N=16` trimmed mean）并换算 `Vred`
4. 计算：
   - `Rx = Rref_eff * Vred / (Vref - Vred)`
   - `Vref` 优先取 `VREFINT` 估算 `VDDA`，失败回退 `3.300V`
5. `Vref - Vred <= 0.02V` 判开路/超量程（`ERR_OVERRANGE`）

## 5. 运行页显示

`L4_RES_RUN` OLED 显示：
- 行1：`RES MANUAL`
- 行2：`RANGE: ...`
- 行3：`ADC raw: xxxx`
- 行4：`Vred: x.xxx V`
- 行5：`ADC stat: OK/TIMEOUT/OVR`
- 行6：`Rx: ... OHM/KOHM/MOHM`（可用档）
- 行7：`err:x`

## 6. 错误与状态位

- ADC 读取失败：记录 `LOGE("RES ADC E%d")`，`MOD_ADC1` 置 `FAIL`
- ADC 恢复成功：记录 `LOGI("RES ADC RECOVER")`，`MOD_ADC1` 置 `OK`
- 占位档进入时写一次 `LOGW` 提示
- 不再扫描 ADS 地址（外部 ADS1110 已移除）
