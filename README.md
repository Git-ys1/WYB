# WYB 实验固件（T-1.6.6-170MHz）

当前分支为提交态冻结版本，目标是保持单一主链路：`HAL + superloop + 单写者显示 + 单一输入/蜂鸣器/FREQ链路`。

## 构建方式（推荐 headless）
```powershell
F:\CodeForge\STM32CubeIDE_2.1.0\STM32CubeIDE\stm32cubeidec.exe --launcher.suppressErrors -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild -data F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3_headless -import F:\CodeForge\STM32CubeIDE_2.1.0\WorkSpace3\WYB -cleanBuild WYB/Debug
```

## 提交态冻结口径
- 显示链路：`app -> presenter -> app_display_service -> oled_smoke`（唯一 flush 写者）。
- 频率链路：`PA0(TIM2_CH1) -> TIM2 IC -> bsp_capture(ticks) -> drv_freq_ic -> app`。
- 模式映射：`MODE CH0=VOLTAGE, CH1=RES/CONT, CH2=DIODE, CH3=FREQ/CAP`。
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
- `CAP`：新增 RC 充电计时 + ADC 阈值法（三档：`20nF/2uF/200uF`），模式接入后复用现有页面风格显示主值/单位/量程/状态。

## T-1.7.5-R1（CAP 基础版，RC+ADC 阈值法）
- 硬件控制冻结：
  - `PE3=CAP_CHG_20N`（R=100k）
  - `PE4=CAP_CHG_2U`（R=10k）
  - `PE5=CAP_CHG_200U`（R=1k）
  - `PE6=DISCH`（高=放电导通，低=释放）
- 阈值固定：`ADC=2587/4095`，常数 `k=0.9989824477`。
- 核心公式：`C = cycles / (SystemCoreClock * R * k)`，计时基准 `DWT->CYCCNT`。
- 超时策略：`20nF=5ms`、`2uF=50ms`、`200uF=500ms`，超时显示 `OL`，不中断主循环。
- 页面策略：仅新增 CAP 分支，不重构 `presenter/display_service` 主结构。

## T-1.7.9-R1（CAP 严格状态机）
- CAP 测量从“阻塞单次”升级为“循环状态机”：`IDLE_SAFE -> PRE_DISCHARGE -> WAIT_EMPTY -> ARM_CHARGE -> CHARGING -> CAPTURED -> HOLD_RESULT -> PRE_DISCHARGE`，含 `TIMEOUT/ERROR` 恢复分支。
- 安全态冻结：`DISCH=高`、三路 `CAP_CHG_*` 全部 Hi-Z；退出 CAP 或异常时强制回安全态。
- 判定口径固定：
  - 放空：`ADC<=16` 连续 3 次
  - 达阈值：`ADC>=2587` 连续 2 次
  - 常数：`k=0.9989824477`
  - 公式：`C = cycles / (SystemCoreClock * R * k)`
- 分档超时：
  - 放空超时：`10ms/50ms/500ms`（20nF/2uF/200uF）
  - 充电超时：`5ms/50ms/500ms`（20nF/2uF/200uF）
  - HOLD：`50ms/100ms/200ms`
- 显示策略：主页面保留上一次有效值，不因 timeout/error 清空主值；状态显示 `DISCH/MEAS/READY/OL/ERR`。

## T-1.7.10-R1（200R 低阻校正 + CONT ON/OFF 修正）
- 仅对 `RES 200Ω` 档新增低阻校正，`2k/20k/200k` 完全不改。
- 校正公式（仅 200Ω 档）：
  - `raw<=20Ω`: `bias=8.05Ω`
  - `20<raw<100Ω`: `bias=8.05*(100-raw)/80`
  - `raw>=100Ω`: `bias=0`
  - `corr=max(raw-bias, 0)`
- `CONT` 继续复用 `RES 200Ω` 测量链，蜂鸣判定改为基于校正后阻值，并采用迟滞+投票：
  - 进入 ON：`<=12Ω`
  - 退出 ON：`>=18Ω`
  - 连续命中 `3` 次才切换
- `CONT` 主界面改为显示当前阻值与 `ON/OFF`，不再显示 `BEEP` 文案。

## T-1.7.6-R1（主界面显示减法微调）
- 本轮仅调整主界面文本输出，不改测量、模式切换、按键与 debug 页面逻辑。
- 主界面底部隐藏 `L:DBG` 提示（LEFT/debug 代码保留）。
- 主界面字段收敛：
  - `RES`：`FUNC / RANGE / R / STAT`
  - `VDC`：`FUNC / RANGE / V / STAT`
  - `CAP`：`FUNC / RANGE / C / STAT`
  - `FREQ`：`FUNC / RANGE / F / D / STAT`（`D` 保留）
  - `DIODE`：`FUNC / 主值 / STAT`（去掉 `MV/RAW` 辅助行）
  - `CONT`：保持现状，不改显示内容

## T-1.6.5-R1：VDC 根因锁定
- 当前 `20V` 漂移首要根因已锁定为：`2V` 支路（`R10 + VIN_2V + 钳位`）对 `20V` 支路污染。
- 已排除作为首要根因的项：`170MHz` 主频不足、`20V` 软件公式本身。
- 关键实验结论：保持 `20V` 支路不变，断开 `R10=22k` 后，`20V` 档可恢复 `2V~20V` 的稳定显示。
- 主线策略：本轮保留 `2V+AUTO`，将问题拆分为 `20V correctness` 与 `2V restore`，后续独立处理，不与 FREQ 混修。
- 保护口径：VDC 异常仅页面化（`OK/OL/MUX BAD/ADC BAD/ERR`），不进入 `Error_Handler`、不触发 `bootdiag fault`。

## T-1.6.6-R1 实验线（170MHz + FREQ 高频收尾）
- 本分支用于 170MHz 频率实验，不回灌主线，主线仍维持 16MHz 冻结口径。
- 时钟迁移：`SYSCLK=170MHz`（HSI->PLL，Range1 Boost，Flash Latency 7）。
- I2C2：内核时钟强制 `HSI16`，I2C timing 走 `HSI16` 常量口径，避免随 SYSCLK 变化失配。
- FREQ 主链不变：`PA0(TIM2_CH1) -> TIM2 capture -> bsp_capture(ticks) -> drv_freq_ic -> app`。
- FREQ 收尾重点：
  - 高频档减负：`accum_cycles` 高频保持 1；
  - 大步跳频：AUTO 允许快速升档；
  - 手动越档：立即 `OL` 且清空历史窗口；
  - 无效捕获：高频段连续无效时快速丢弃旧窗口，等待新样本。

## T-1.6.7-R1（170MHz：20V 分段校准）
- 仅对 170MHz 版本的 `VDC 20V` 档启用分段一次校准，`2V` 档与其他模式不改。
- 20V 修正采用 7.4V 分段：
  - `raw<=7400mV`: `(9983*raw + 1100000 + 5000)/10000`
  - `raw>7400mV`: `(9319*raw + 5920000 + 5000)/10000`
- 主界面显示修正后值；Debug 页同时给出 `VRAW20`（修正前）与 `VCOR20`（修正后）。
- 修正后做边界保护：`<0` 钳位到 `0`，并保持原有 `OL` 逻辑不被破坏（20V 档含 `19990mV` 守门）。

## T-1.7.1-R1（FREQ 单位优化 + 识别冻结）
- 频率主界面按阈值自动切换单位：
  - `<1000Hz` 显示 `Hz`
  - `1.000~9.999kHz` 显示 3 位小数
  - `10.00~99.99kHz` 显示 2 位小数
  - `100.0~200.0kHz` 显示 1 位小数
- 内部频率数据仍统一使用 `Hz`，仅在 UI 文本格式化层切换单位。
- `OL/NO SIG/OVER` 逻辑不变。
- 本版本不实现正式的方波/正弦波自动识别功能，避免误判。

## T-1.6.2-R3 高频减负口径
- 时钟维持 `HSI 16MHz + PLL_NONE`，不做 170MHz 升频。
- TIM2 捕获减负：`CH1=IC中断`、`CH2=IC非中断轮询读取`，回调只处理 CH1。
- 频率 profile 累计收敛：
  - `20Hz=4`、`200Hz=2`、`2k/20k/200k=1`。
- 主界面新增显示死区（Debug 页仍实时）：
  - `FREQ`: `max(0.2%, 1Hz)`
  - `Duty`: `1%`
  - `VDC`: `1 LSD`
  - `RES`: `max(0.2%, 1 LSD)`
- 立即刷新条件：档位变化、AUTO 切档、状态/错误变化、`NO SIG/OL/OVER` 进出。

## T-1.6.5-R1：FREQ 高频收尾口径
- 主链不变：`PA0(TIM2_CH1) -> TIM2 capture -> bsp_capture(ticks) -> drv_freq_ic -> app`。
- 仅模块内热修：`profile` 参数、`drv_freq_ic` 判定与 AUTO 跳频收敛，不改主循环。
- profile 参数更新：
  - `20Hz`: `no_sig_timeout_ms=600`, `accum=4`
  - `200Hz`: `no_sig_timeout_ms=300`, `accum=2`
  - `2kHz`: `120`, `1`
  - `20kHz`: `60`, `1`
  - `200kHz`: `30`, `1`
- AUTO 在“明显越当前活动档”时允许快速升档，减少大步跳频时卡在错误档位的停留时间。
- 无效/无信号判定按当前活动档消费不同 invalid 门限，不再统一固定阈值。

## FREQ 热修框架位（已预留）
`bsp_freq_capture_set_profile()` 对每档位支持：
- `icpsc`
- `no_sig_timeout_ms`
- `accum_cycles`

后续频率优化仅允许调整 profile 与 `drv_freq_ic` 模块内判定，不再改主循环或重走新测量路线。

## T-1.6.2-R1/R2/R3 热修范围
- 仅包含：`boot_splash_bitmap` 重画 + VDC 默认 `AUTO` + FREQ 越档 `OL` 与两级恢复。
- FREQ 仍固定：`PA0(TIM2_CH1) -> TIM2 capture -> BSP ticks -> drv_freq_ic -> app`。
- R3 仅增加 TIM2 捕获减负与主界面死区，不改主循环、不中断主链。

## 文档入口
- 提交态真相冻结：`docs/T-1.5.5_truth_freeze.md`
- 本轮清理报告：`docs/T-1.5.5_cleanup_report.md`
- 交稿前最小验收：`docs/validation_checklist.md`
- 历史阶段文档：`docs/archive/`（如有）与 `forcodex/plan/`
