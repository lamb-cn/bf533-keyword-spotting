# 完整定点前端接口

> 2026-10-31真实音频更新：这里的Q15快速链路虽通过自身定点golden检查，但一条go音频出现较大浮点特征偏差并误识别。端到端应用已改用 `work/end_to_end/frontend32.c` 的Q31 FFT及保留小数的Mel路径，该样例恢复正确；实际应用应参考 `work/end_to_end/端到端仿真报告.md`，不能把本文的算子测试当作原模型输入兼容性验证。

初始化一次：`fe_init(&workspace)`。

每个640样本PCM16帧调用：`fe_mfcc640(pcm, mfcc_out, &workspace)`，输出10个Q1有符号INT8特征，返回FFT block exponent。工作区由调用者分配，单工作区不可并发复用，输入/输出不能与工作区重叠。

链路为Hann640 → 补零1024 → CCES定点FFT → 整数幅度谱 → Mel40 → log/尺度补偿 → Q8 DCT10 → Q1舍入/INT8饱和。这里的log使用CCES `logf`，并非硬件单条指令。

`fe_log40` 接收当前整数Mel能量，正数按 ln(E)+(FFT_exponent−15)ln2 补偿，零能量使用 ln(FLT_MIN)。自然对数乘256后采用最近舍入，恰好半数时远离零。`fe_quantize10`（`quantize.asm`）将DCT的Q8转为模型Q1，即除128、同样舍入，然后饱和到[-128,127]，支持全部INT32输入而不发生INT_MIN取负溢出。使用硬件循环、逻辑右移及MIN/MAX；偏置加法按无符号幅度处理INT_MIN，最后恢复符号。

该链路具有明确的定点算术约定，但尚不等同于原浮点前端逐位结果。FFT Q15量化、整数开方、Mel整数截断、Q8 log和Q15 DCT都会引入误差。尤其低能量Mel可能截断为0；预训练模型兼容性仍需用真实音频与原浮点MFCC对比，不能仅凭算子测试认定识别准确率。

argmax按用户既有要求未实现。无新增C性能基线。前端阶段使用必要的CCES库调用和C调度/整数辅助函数，CNN已有自编汇编内核保持不变。

## 最终验证与计时

CCES2.10、BF533 rev0.6仿真，内部SRAM，`-O`，CYCLES差值含调用和计数附近开销，无C性能基线。

| 项目 | cycles |
|---|---:|
| 40路log与尺度补偿（正整数能量样例） | 52,893 |
| 10路Q8→Q1汇编量化 | 161 |
| 完整640样本帧：静音 | 125,429 |
| 完整640样本帧：直流12000 | 148,558 |
| 完整640样本帧：确定性变化PCM | 175,468 |

完整帧计时包含加窗/补零、FFT、幅度谱、Mel、log、DCT、量化，不含一次性twiddle初始化。假设513MHz，上述三帧约0.245–0.342ms；是测试输入范围，不是任意输入的最坏耗时保证。

实际运行 `.\work\mfcc_ops\run_complete.ps1 -Focused`：

- 840个log输出覆盖零值、多个能量数量级和FFT exponent 0/5/10；对照C数学golden允许1个Q8 LSB误差。
- 40,010个连续量化输入，含半数舍入、正负数、饱和；另检查INT_MIN/INT_MAX。
- 静音、直流、确定性变化PCM三个完整帧，比较120个中间Mel能量及30个最终特征。完整帧golden从已单独验证的FFT输出独立重算下游阶段，最终允许1个Q1 LSB误差，不能称为原浮点MFCC的端到端准确率验证。
- 完成标记正确，log/量化/帧/输出保护区错误数均为0。结果见 `complete_result.json`，构建无警告，日志为 `complete_build.log`、`complete_sim.log`。
- 扩大log覆盖的非Focused模式也保留，但其BF533浮点golden计算耗时较长。最初扩大覆盖的运行未完成，不作为通过证据。

复现已完成测试：`.\work\mfcc_ops\run_complete.ps1 -Focused`。原先的 `.\work\mfcc_ops\run.ps1` 保留为独立算子回归测试。
