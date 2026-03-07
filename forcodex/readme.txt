1）项目目标

实现一个 3½ 位数字万用表，包含：

必做：

直流电压：2000mV / 20V 两档
电阻：200Ω / 2kΩ / 20kΩ / 200kΩ 四档
频率 & 占空比：20Hz / 200Hz / 2kHz / 20kHz / 200kHz 五档；占空比分辨力 1%
通断：表笔间电阻 < 10Ω 连续蜂鸣
二极管：显示极性 + 导通压降
单按键：短按切档位，长按切功能

选做：

交流电压：2000mV / 20V；20Hz–20kHz
电容：20nF / 2µF / 200µF
自动量程（以及你想做的“自动识别 R/C/二极管”）

2）硬件“文字版规格”（按你发的开源方案重构）
2.1 MCU 引脚

- I2C3（外置 ADC ADS1110）
- PC8 = I2C3_SCL
- PC9 = I2C3_SDA
- I2C2（OLED 显示）

PC4 = I2C2_SCL
PA8 = I2C2_SDA

电阻档 CD4051（U4）地址线
PD5 = RES_MODE_A
PD6 = RES_MODE_B
PD7 = RES_MODE_C

模式选择 CD4051（U9）地址线
PB4 = CHANNLE_SELEC_A
PB5 = CHANNLE_SELEC_B
PB6 = CHANNLE_SELEC_C

电压量程 CD4051（U11）地址线
PB13 = VOLTAGE_MODE_A
PB14 = VOLTAGE_MODE_B

频率/占空比输入捕获
PA0 = TIMx_INPUTCAPTURE（实际用哪个 TIM 由 CubeMX 映射，名字不重要）

补充：KEY/BEEP（你需要新增）
PC13 = KEY（EXTI）
PB8 = BEEP（GPIO/PWM）

2.2 总体信号流（最关键的一句话）

红表笔 RED 是模拟输入节点；
不同测量前端（电压/电阻/二极管/通断等）各自产生一个“模拟输出网络名”（如 VOLTAGE / RES / DIODE / ONOFF…）；
这些模拟输出进入 U9（模式选择 CD4051），由 MCU 选一路送到 ADS1110 的 VIN+；
MCU 通过 I2C3 读取 ADS1110 数值，再换算成物理量显示。

2.3 电阻测量模块（U4 + Rref 网络 + 缓冲）

U4（CD4051）把 不同参考电阻 Rref 接到 3V3，与被测电阻形成分压
RED 节点电压经 TL072 电压跟随器缓冲后送到 ADC（最终被 U9 选到 ADS1110）

核心公式（电阻测量）：
设 Vred 为 RED 节点电压，Vexc = 3.3V
Rx = Rref * Vred / (Vexc - Vred)
需要根据量程选择不同 Rref（注意：开源图里 10k×4 只是占位，你要换成适配 200/2k/20k/200k 的参考电阻）

2.4 直流电压测量模块（分压 + 缓冲 + U11 量程选择）

20V 档：用大电阻分压（示例 900k/100k）把 0–20V 映射到 0–2V 左右，再缓冲/送入 ADC
2000mV 档：走“直通/低分压”路径
U11（CD4051）用于在不同量程路径间切换

核心公式（直流电压）：

20V 档：Vin = Vadc * (Rtop + Rbot) / Rbot（900k/100k 时约等于 Vin ≈ 10 * Vadc）
2V 档：按直通比例（理想 Vin ≈ Vadc，实际要标定）

2.5 频率/占空比模块（比较器整形 → 输入捕获）

模拟输入经运放/比较器整形（TL072 + LM393 + 滞回）得到数字方波
LM393 输出通过上拉电阻到 3.3V，送到 PA0 的输入捕获
MCU 用定时器捕获周期 T 和高电平时间 Th：

f = 1 / T
duty = Th / T

2.6 通断/二极管（逻辑层面）

通断：直接复用“电阻测量结果”，若 Rx < 10Ω 并且持续满足一定时间（去抖/迟滞），则蜂鸣器连续鸣叫
二极管：硬件提供 DIODE 模拟通道供 U9 选择；MCU读取电压换算压降，并结合测试方向判断极性
若你硬件只提供单向测试：只能显示“导通压降 + 当前接法极性”
若硬件支持正反向切换（通过 ONOFF/开关）：可做“自动识别极性”

2.7 I2C 与供电约束（必须写进固件/接线检查）

OLED 与 ADS1110 的 SCL/SDA 必须上拉到 3.3V
若任何模块上拉到 5V，会把 MCU I2C 拉到 5V（风险极高）

3）固件任务表（Codex 直接照这个做）
3.1 工程骨架

生成 CubeMX 工程（HAL），启用：

I2C2（OLED）
I2C3（ADS1110）
TIM 输入捕获（PA0）
GPIO：PD5/6/7、PB4/5/6、PB13/14、PC13(KEY)、PB8(BEEP)
主循环 Superloop：while(1){ poll_button(); measure(); ui_update(); }

3.2 驱动层（Drivers）

drv_oled_ssd1306_i2c2.*
oled_init() / oled_clear() / oled_printf() / oled_flush()

drv_ads1110_i2c3.*
ads1110_init()
ads1110_read_raw()
ads1110_read_mv()（把 raw 转成 mV，量程/增益按配置）

drv_mux4051.*
mux_set_res_range(idx) → 控 PD5/6/7
mux_set_mode(idx) → 控 PB4/5/6
mux_set_volt_range(idx) → 控 PB13/14
统一用枚举定义 idx 对应哪一档/哪一路（必须写清）

drv_freq_ic.*
freq_start()
freq_get_hz()
freq_get_duty()（做多周期平均/中位数滤波）

drv_button.*
产生事件：BTN_SHORT / BTN_LONG

drv_beep.*
beep_once(ms) / beep_continuous(on) / beep_pattern(...)

3.3 测量算法层（Measurements）

meas_voltage_dc(range)
选择 U11 档位 → 选择 U9 模式到 VOLTAGE → 读 ADS1110 → 套比例系数 → 显示

meas_resistance(range)
选择 U4 参考电阻档 → 选择 U9 模式到 RES → 读 ADS1110 → Rx = Rref * V / (Vexc - V)

meas_continuity()
调 meas_resistance(200Ω档) → <10Ω 持续满足则蜂鸣

meas_diode()
选择 U9 模式到 DIODE → 读 ADS1110 → 显示 Vf 与极性（按硬件能力）

meas_freq_duty(range)
直接读 freq_get_hz/duty，根据档位改变“门控时间/平均次数/显示单位”

3.4 UI 与状态机（单按键）

长按：切功能（VDC → R → F/Duty → Continuity → Diode → …）
短按：切该功能的档位（例如 VDC：2V/20V；R：4档；Freq：5档）
OLED 显示：大数字 + 单位 + 档位 + 状态图标（AC/DC、蜂鸣、超量程）

3.5 标定参数（先常量，后可写 Flash）

创建 calib.h / calib.c：

每个量程一组 scale、offset
电阻每档一个 Rref
电压分压比 k_div
并预留：

calib_load()（默认从常量装载）
calib_save()（以后做一键标定再实现）
