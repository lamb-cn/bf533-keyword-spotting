"""Create a Chinese measurement report from the verified simulator CSV."""
from pathlib import Path
import csv
root = Path(__file__).parent
rows = list(csv.DictReader((root/'results.csv').open(encoding='utf-8-sig')))
assert len(rows) == 15
assert all(int(r['macs'])>0 for r in rows), 'Requires all 15 cases; partial runs cannot regenerate full report'
assert [int(r['macs']) for r in rows[:3]] == [320000,72000,512000], 'Not a full benchmark run'
assert all(int(r[k]) == 0 for r in rows for k in ('raw_errors','final_errors','guard_errors'))
names = ['10×4 普通卷积','3×3 depthwise','1×1 pointwise']
def v(r,k): return int(r[k])
lines = ['# BF533 CNN 汇编算子实测（2026-10-30）', '',
'已实现 DS-CNN Small 使用的三类卷积，INT8 数值先符号扩展为 INT16 存储。汇编输出精确 INT32 整数累加；C 完成偏置、舍入、右移、INT8 饱和与可选 ReLU。', '',
'## 测量条件', '',
'- CCES 2.10.0，ADSP-BF533 单核指令级仿真器，silicon revision 0.6；C 使用 `-O`。',
'- 输入/patch 放在 L1 SRAM A，预展开权重放在 L1 SRAM B，汇编在内部指令 SRAM；不代表外部 SDRAM 或上板 DMA 并发性能。',
'- 输入是确定性合成数据，采用上游 Small 网络尺寸和九层卷积的缩放配置；没有验证预训练权重或识别正确率。',
'- cycles 为 CYCLES 寄存器差值，包括函数调用和计时附近开销，未扣空计时。分段计时含每个像素的读计数开销。',
'性能参照为 FIR 实测 2144 cycles / 2080 MAC = 1.03077 cycles/MAC。C 仅作 golden，不参与性能比较。历史 CSV 的 c_cycles 字段不再使用。', '', '## 三类代表层', '',
'| 算子 | MAC/层 | FIR cycles/MAC | ASM 内核 cycles | ASM cycles/MAC | 相对 FIR MAC 吞吐 | ASM 完整层 cycles |',
'|---|---:|---:|---:|---:|---:|---:|']
for r in rows[:3]:
    lines += [f"| {names[v(r,'type')]} | {v(r,'macs'):,} | 1.031 | {v(r,'core_cycles'):,} | {v(r,'core_cycles')/v(r,'macs'):.3f} | {(2144/2080)/(v(r,'core_cycles')/v(r,'macs')):.2f}× | {v(r,'layer_cycles'):,} |"]
lines += ['', '完整层包括输入展开/padding、普通卷积 patch 整理、汇编调用、偏置/舍入/饱和/ReLU 和输出写入；不包含一次性权重打包。完整层单独连续计时，不能用分段耗时相加代替。', '',
'## 九层卷积配置', '',
'| 层 | 类型 | bias 左移 | 输出右移 | 输入整理 cycles | 内核 cycles | 量化 cycles | 完整层 cycles | 权重打包 cycles（一次性） |',
'|---|---|---:|---:|---:|---:|---:|---:|---:|']
for i,r in enumerate(rows[:9]):
    label = 'conv1' if i==0 else f"conv{(i+1)//2+1}_{'dw' if i%2 else 'pw'}"
    lines += [f"| {label} | {names[v(r,'type')]} | {r['bl']} | {r['rs']} | {v(r,'prep_cycles'):,} | {v(r,'core_cycles'):,} | {v(r,'post_cycles'):,} | {v(r,'layer_cycles'):,} | {v(r,'weight_cycles'):,} |"]
total = sum(v(r,'layer_cycles') for r in rows[:9])
lines += ['', f'九层完整卷积耗时相加：**{total:,} cycles**。假设 513MHz，折合 **{total/513000:.3f}ms**；这是九次独立算子测试之和，不是连接后的整网实测，不含 MFCC、池化、全连接、采集或模型切换。', '',
'## 正确性', '',
'- 9 个随机完整层配置，加 3 类算子的全零、正负极值，共 15 个用例。',
'- 120,000 个原始 INT32 累加逐项对比独立 Python 直接卷积，120,000 个量化结果对比独立整数除法参考。',
'- 另外执行并检查 120,000 个连续计时整层输出，参考为 Python 生成的最终 INT8。',
'- 最小手算测试：含 -128 与 127，检查双输出通道排列、整数模式无额外 ×2、最短 K=2。',
'- 包含上/下/左/右 padding，conv1 的 25 行输出按原部署定义执行（底部隐式多补一行）。', '',
'## 汇编设计', '',
'`cnn_matvec` 用于普通卷积和 pointwise：权重按 `[输出通道对][K][2]` 打包，A0/A1 共享输入并计算两个输出。内层每条 64-bit 发射包包含双整数 MAC、一个 16-bit 输入读取和一个 32-bit 权重读取。`LSETUP` 执行 K−1 次稳态迭代，最后一轮只计算、不预读。I0/B0/L0 环形寻址使输入在每组输出通道后自动回到开头。', '',
'`cnn_dw3` 直接读取 padded HWC 中相邻的两个通道，使用两个 32-bit 读取及双整数 MAC；9 个 tap 展开，M0 表示像素跨度，M2 表示第三列到下一行的跨度。通道对使用硬件循环。所有 L 寄存器返回时为零，保留 C ABI 要求的寄存器。', '',
'两个内核计算段理论上每周期 2 MAC，但短向量的初始化、地址建立、循环建立、累加器导出和存储均有成本，整层不能按纯计算段峰值估算。', '',
'## 与现有例程的关系', '',
'- FIR 的双 MAC、`计算 || 取数 || 取数`、首轮预装载与末轮排空、I/B/L 环形寻址，直接用于矩阵内核。',
'- exp4 FFT 的配对半字运算、计算与访存调度、地址修改寄存器和 C ABI 保存方式，作为通道配对与二维跨度寻址的参考。',
'- CNN 整数累加使用 `(IS)` 和 `A0.W/A1.W`，不同于 FIR 的小数乘法及高半字舍入输出；不能原样复制 FIR 的数值缩放。',
'- 双取数使用 I0 与 I2 两组 DAG 路径，并将输入和权重分布在不同 L1 数据 bank，避免只写并行语法却受同一数据区争用限制。', '',
'## 内存及接口', '',
'- depthwise padded 激活：27×7×64×2 = 24,192B；pointwise 展开激活：16,000B；普通卷积 patch：80B（接口统一预留128B）。',
'- 单层展开权重：普通 5,120B、DW 1,152B、PW 8,192B；输出8000B；原始单点 sums256B。',
'- 基准复用工作区，逐层打包权重；没有声称全部预展开权重和两个完整激活张量同时常驻 L1。',
'- 各层权重可离线打包；真实模型部署仍需决定常驻/分层加载与双激活缓冲方案。',
'- 公共接口见 `cnn_ops.h`；汇编约束见 `cnn.h`。接口针对此 Small 网络尺寸，不是任意尺寸 CNN 库。', '',
'## 复现', '',
'在项目根目录执行：', '', '```powershell', '.\\work\\cnn_asm\\run.ps1 -Timeout 600000',
"& 'C:\\Users\\lenovo\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe' .\\work\\cnn_asm\\report.py", '```', '',
'`run.ps1 -Smoke -Timeout 15000` 执行最小手算用例；会覆盖测试产物，所以完整报告须从完整用例生成。详细原始数据见 `results.csv`，构建/仿真日志、ELF 符号及反汇编保留在同目录。', '',
'上游尺寸依据： https://github.com/ARM-software/ML-KWS-for-MCU/blob/master/Deployment/Source/NN/DS_CNN/ds_cnn.h',
]
(root/'性能报告.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(f'九层独立卷积合计 {total:,} cycles; 513MHz 假设下 {total/513000:.3f} ms')
