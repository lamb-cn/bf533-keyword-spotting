from pathlib import Path
import json,struct
P=Path(__file__).parent
load=lambda name:json.loads((P/name).read_text(encoding='utf-8'))
full=load('q31_go_report.json');old=load('q15_report.json')
async_=load('target_async.report.json');serial=load('target_serial.report.json')
tile4=load('target_activation.report.json');tile16=load('target_activation16.report.json')
pc=load('pc_results.json')
wait_gain=100*(serial['weight_wait_cycles']-async_['weight_wait_cycles'])/serial['weight_wait_cycles']
time_gain=100*(serial['total_cycles']-async_['total_cycles'])/serial['total_cycles']
lines=['# 真实音频端到端与DMA仿真报告（2026-10-31）','',
'## 结论与覆盖范围','',
f"一条完整BF533端到端测试通过：`{full['audio']}`，标签go，输出最高得分类别为 **{full['host_predicted']}**。路径为WAV PCM→49帧MFCC→9层卷积→池化→64到12全连接。目标程序仍仅输出12个得分，argmax只在PC报告中执行。",'',
'另外完成4次固定同一PC浮点MFCC输入的BF533网络回放：串行权重DMA、异步权重预取、256B输出DMA、1024B输出DMA。每次均与独立PC整数网络逐项对比9层激活、池化和FC，共72,076个INT8值，全相等。所有DMA错误、缓冲guard错误为0。',
'本次不是60条音频全部在BF533端到端运行。PC浮点MFCC参考对本地60条得到45/60；这是非随机小样本且数据集版本与原权重训练版本可能不同，不能代表完整测试集或论文准确率。','',
'## 先发现并修复了前端精度问题','',
f"原Q15快速前端在这条go音频上误判为{old['host_predicted']}，MFCC相对浮点参考平均绝对差{old['mfcc_vs_float_mae']:.6f}、最大{old['mfcc_vs_float_max']}（均以最终INT8单位计）。虽然其网络算术与golden一致，输入特征已不兼容。失败导出保留为q15_target.bin、q15_report.json。",'',
f"端到端路径改用CCES Q31 FFT、Q31 Hann和保留小数的幅度/Mel累加，避免Q15 FFT及整数Mel截断损失经log放大。保留已验证的Q15 DCT和汇编Q1量化。BF533实测MFCC平均绝对差降为 **{full['mfcc_vs_float_mae']:.6f}**，最大 **{full['mfcc_vs_float_max']}**；490个特征中只有4个相差1。不是逐位等同浮点前端。",'',
f"最终12类得分（Silence、Unknown、yes、no、up、down、left、right、on、off、stop、go）：`{full['scores']}`。这些是有符号量化得分，不是概率。",'',
'## 音频与权重异步搬运','',
f"完整运行收到 **{full['audio_irqs']}次MDMA1音频完成中断**、**{full['weight_irqs']}次MDMA0权重完成中断**。音频640样本双缓冲预取下一帧；权重两份8KB缓冲预取下一层。缓冲所有权和guard检查通过。",'',
'- IRQ路由：SPORT0 RX预留IVG8（未启用实际采集），MDMA1音频/激活IVG9，MDMA0权重IVG10。ISR只处理完成/错误、标志和统计。',
'- 不能把上述路由配置当成已验证真实SPORT采集或ISR嵌套抢占。MDMA1实际为内存流，与外设SPORT使用的DMA1通道不是同一通道。',
'- 记录DMA发起、完成ISR和当前层CPU处理起止：异步回放9次后续权重搬运的区间均与当前层CPU处理交叠，串行对照均为0。',
'- 完成时间戳取自ISR，含中断响应延迟，不是DMA总线忙时间的精密测量。默认版本的“层CPU处理”包含准备、MAC、后处理；激活DMA实验把权重预取放在准备完成后，使其与MAC/后处理阶段交叠。','',
'## 相同输入的串行/异步对照','',
'| 调度 | CPU权重等待cycles | 网络回放总cycles | 输出DMA中断数 |',
'|---|---:|---:|---:|']
for name,r in [('串行搬权重再计算',serial),('预取下一层权重（默认）',async_),('预取权重＋256B激活输出DMA',tile4),('预取权重＋1024B激活输出DMA',tile16)]:
 lines.append(f"| {name} | {r['weight_wait_cycles']:,} | {r['total_cycles']:,} | {r.get('output_irqs',0)} |")
lines += ['',f'权重等待减少 **{wait_gain:.2f}%**，但网络回放总耗时仅改善 **{time_gain:.2f}%**。搬运量相对计算较小，不能把等待降幅宣传为整网加速幅度。', '',
f"两种激活DMA方案都准确搬完72,000B输出，但分别比默认方案慢 **{100*(tile4['total_cycles']/async_['total_cycles']-1):.2f}%**、**{100*(tile16['total_cycles']/async_['total_cycles']-1):.2f}%**。1024B减少中断次数，但配置、完成同步和存储区争用仍抵消收益。因此默认不启用激活DMA；保留实验开关供真实板卡测试。",'',
'回放冻结的是同一条音频的PC浮点参考MFCC，避免重复MFCC耗时污染权重调度对照。它与完整BF533音频前端测试是分开的证据。所有方案最终网络结果完全相同。','',
'## 完整音频流程周期','',
'| 区域 | cycles | 假设513MHz时间 |','|---|---:|---:|']
for name,key in [('49帧MFCC（含音频调度）','mfcc_cycles'),('9层卷积区域（含调试journal/printf）','conv_region_cycles'),('池化＋输入展开','pool_cycles'),('总流程（含初始化、层快照和跟踪）','total_cycles')]:
 v=full[key];lines.append(f'| {name} | {v:,} | {v/513000:.3f}ms |')
lines += ['',f"完整流程权重等待{full['weight_wait_cycles']} cycles，音频等待{full['audio_wait_cycles']} cycles。总流程约{full['total_cycles']/513000:.2f}ms是已有1秒音频的离线批处理计算时间，不是声音从开始到识别的总延时，也不是上板实测。",'',
'外部激活使卷积比先前全L1算子测试更慢；池化区域还包含8000个INT8到INT16的展开，不能与独立池化内核5468 cycles直接比较。调试journal每层额外复制8000B，发布版应删除。','',
'## 实现与内存','',
'`pipeline.c`：双DMA驱动、ISR、双缓冲所有权、整网调度和验证journal。`frontend32.c`：实际端到端使用的高精度前端。`model.h`/`image.bin`：离线打包权重、bias及音频fixture；原始权重22016B，展开权重44032B置于外部内存。',
'L1 A共用24192B工作区（MFCC与CNN不同时运行），128B patch，2560B Hann；L1 B权重16400B（含guard）、音频2576B（含guard）、特征和小型元数据。最终链接通过，stack约4644B，未做板上最坏栈高水位测试。',
'两个8000B激活在外部内存。激活DMA实验在CNN阶段复用音频缓冲为输出tile，只适用于该阶段互斥安排；不能让真实SPORT持续写这些同名缓冲同时又拿它们输出激活。',
'完整中断、状态机、地址和上板计划见 `数据搬运与调度设计.md`。','',
'## 仿真与上板边界','',
'- 这是CCES BF533指令级仿真器的实际MemDMA寄存器与中断测试，不是用memcpy冒充DMA。L1→L1及外部内存→L1的MDMA0、MDMA1微测试均通过。',
'- 外部地址由runner装入；本例没有配置实际板卡SDRAM时序或SCLK/CCLK。仿真DMA吞吐不能当作板上SDRAM带宽保证。',
'- CPU中断优先级不等于DMA总线仲裁优先级。真实SPORT、两路MemDMA、CPU并发的总线竞争和中断延迟需上板测。',
'- 当前L1 DMA缓冲使用SRAM、未启用数据cache；实际项目启用外部数据cache/CPLB后必须设计相应clean/invalidate和同步策略。',
'- 实时版应使用外部音频环形缓冲、320样本块序号/overrun检查、增量MFCC与490B推理快照。ISR不执行MFCC/CNN，联合工作区只在任务边界交接。48kHz采集还需抗混叠FIR及3倍降采样。',
'- argmax、拒识/平滑、LED、实际音频采集仍未加入目标程序。','',
'## 复现','',
'Python路径：`C:\\Users\\lenovo\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe`。在项目根目录执行：','',
'```powershell',
"& 'C:\\Users\\lenovo\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe' work\\end_to_end\\prepare.py",
'.\\work\\end_to_end\\run.ps1',
"& 'C:\\Users\\lenovo\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe' work\\end_to_end\\prepare_replay.py",
'.\\work\\end_to_end\\run_replay.ps1 -Serial',
'.\\work\\end_to_end\\run_replay.ps1',
'.\\work\\end_to_end\\run_replay.ps1 -ActivationDMA',
'.\\work\\end_to_end\\run_replay.ps1 -ActivationDMA -TilePixels 16','```','',
'真实端到端仿真明显慢于DSP的等效运行时间。`prepare.py --audio 相对路径`可换文件；切换音频后须重新跑full/replay，不得把不同fixture的报告直接作时间对照。`target_audio.json`记录音频/权重/image SHA256，`q31_go_*`保留本次完整运行的导出、报告、元数据和DXE快照。',
]
(P/'端到端仿真报告.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print('Report written; default wait reduction %.2f%%, overall %.2f%%'%(wait_gain,time_gain))
