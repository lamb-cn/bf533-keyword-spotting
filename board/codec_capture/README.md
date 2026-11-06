# BF533 EZ-KIT codec/SPORT0 DMA采集

本分支实现第一阶段上板采集：AD1836-family SPI配置 → SPORT0 RX/DMA1循环采集 → 单声道PCM16 → 抗混叠FIR/3倍抽取 → 16kHz、每20ms320样本。参考本仓库 `例程/Talk_Through_533_I2S`，未修改原例程。

## 采样率结论

AD1836A支持最高96kHz，并非固定96kHz。ADC Control1 bit6选择48/96模式；本例写0xC000，ADC Control3写0xE000（默认256×时钟模式），按标准参考12.288MHz MCLK得到48kHz。原Talk Through也是这些零配置。96kHz模式仅保留主ADC两个通道，本分支不提供未经验证的96kHz开关。

采样频率由codec MCLK及其配置决定，不由SPORT的DMA计数或CPU主频决定。CCLK来自单独的CLKIN/PLL：例程按标准CLKIN27MHz读取PLL_CTL/PLL_DIV/PLL_STAT计算实际CCLK，不修改PLL，不再把历史仿真的513MHz直接当成上板时钟。

初始化命令保留I2S/24bit、0dB ADC增益，DAC静音。SPI PF4片选、DMA5写11个寄存器，轮询DMA完成与SPI移位完成，带超时和错误出口。Flash A PA0控制codec复位，EBIU异步bank配置沿用标准板参考。SPI控制器完成传输不等于读回确认codec已接受配置。

## DMA与数据所有权

- SPORT0使用外部BCLK/LRCLK，24bit、主/次接收线及stereo frame sync，配置与参考一致；只启RX，不占用SPORT TX/DMA2。
- DMA1映射0x1000，32bit、2D autobuffer、X_COUNT=480、Y_COUNT=8、X/Y_MODIFY=4。**DI_SEL=1**使每个内层行完成产生中断；DI_SEL=0则等整个二维缓冲完成。
- 每行120个采样时刻×4个槽，共1920B/2.5ms；8行15360B，总时长20ms。为保留正在写入的一行，主循环服务间隔应小于约17.5ms，不能让main长期持有旧行。
- IRQ9路由至IVG8，保留IVG9/10供后续MemDMA调度。ISR仅清DMA状态、读取当前行、发布完成行和错误；FIR不在ISR运行。外设DMA1与MemDMA stream1不是同一个资源。
- main获取READY行时短暂关中断，标记CPU_HELD；处理完再释放FREE。DMA将进入非FREE行、行序错位、异常延迟或DMA错误时立即停机并报告，不静默覆盖数据。
- 每8行检查20ms累计周期±10%；错误CCLK、MCLK/采样率不匹配会报CAPTURE_CLOCK。周期检查要求CCLK持续计数、不在采集中更改PLL。它不替代示波器测量LRCLK。

## 格式与降采样

默认选择slot0，参考槽顺序为0=L0、1=L1、2=R0、3=R1。`capture_pcm24`从右对齐24bit补码提取PCM16，不依赖高8位是否已经符号扩展；实际板上仍需用左右声道测试确认槽顺序与SPORT数据格式。

63抽头（62阶）对称Q15 FIR，Kaiser beta8、截止6kHz，系数和32768。主循环使用已有 `work/mfcc_ops/dot.asm` 的40bit定点MAC内核，delay跨DMA块保留，每3个48kHz输入产生1个16kHz输出。量化系数的数学频响在8kHz以上最大约-73.49dB；这不是板上ADC信噪比实测。群延迟31个48kHz采样，约0.646ms。

每行产生40个PCM16，8行累计320样本，对接已有增量MFCC即可。不是直接每3点丢2点。

## 构建与接口

在仓库根目录：

```powershell
.\board\codec_capture\build.ps1
.\board\codec_capture\build.ps1 -Test
```

使用已安装CCES2.10、BF533 revision0.6；若板上芯片修订不同，应调整构建参数/工程配置。生成的capture.dxe用于独立采集bring-up，不自动装载到硬件，测试文件均被忽略。

应用接口：

```c
capture_init(actual_cclk_hz, raw_ring, 0);
capture_start();
/* main循环 */
n = capture_read40(out40); /* 0无数据，40有新输出，-1故障 */
```

`capture_stop`后重新启动需要重新init。`capture_test_reset`仅供无外设状态机测试，不应在运行中调用。`main.c`示例更新`pcm_20ms`及`blocks_20ms`，可在CCES画图/观察。它没有在ISR中调用MFCC或CNN。

独立示例把15KB raw ring放在L1 B，适合先验证codec。**不能直接把它与已有NN的L1权重缓冲同时合并**；整机集成应由BSP初始化SDRAM后提供uncached、DMA可访问的raw ring，保持L1供模型使用。驱动不擅自改SDRAM时序，不包含板卡特定SDRAM启动代码。捕获与DSP工作区不能重叠，打开cache时另需对应维护策略。

## 已完成验证

采集例程与测试DXE均编译通过。BF533仿真状态机/数值测试通过，`test_result.json`的错误计数1–6均为0：

- 24bit正/负极值及-1转换。
- 32行循环回绕，1280个FIR输出匹配独立64bit C golden，跨行滤波状态不重置。
- 四个槽分别注入，额外160个输出正确。
- 缓冲满时停机、非法行序拒绝、时钟周期偏半/倍拒绝，以及PLL时钟换算。

仿真测试通过`capture_publish_row`注入行完成事件，没有伪称模拟了实际codec串行输入。SPI寄存器写入、电气连接、真实SPORT DMA请求、IRQ响应时刻与模拟链路音质**尚未上板实测**。

## 首次上板步骤

1. 核对标准EZ-KIT及codec丝印/板卡修订。按板级手册将SW12置ON；I2S时核对SW9的5/6接通帧同步和时钟连接，避免沿用不匹配的TDM跳线。
2. 加载capture.dxe，检查`measured_cclk_hz`、`init_failed`。先接主ADC左声道线性音频输入，观察raw四槽和PCM；不要把SPI本地完成当作codec读回验证。
3. 测量MCLK/BCLK/LRCLK，确认LRCLK约48kHz；`capture_stats.irqs`约400次/秒、`blocks_20ms`约50次/秒，`fault/overruns`保持0。
4. 输入1kHz测试音，检查16kHz PCM的周期、幅度及正负值，再测高于8kHz的输入抑制。分别验证左右声道槽。
5. 采集稳定后把每8行320样本送入既有49行特征环，每5个20ms块触发一次网络。整机CNN/SDRAM/采集并发、物理实时性和长语音事件验收属于下一阶段，不把本例程编译通过当成这些项目已经通过。

## 依据

- [AD1836A数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/ad1836a.pdf)：Functional Overview、时钟、Tables17–19。
- [BF533 EZ-KIT手册rev3.2](https://www.analog.com/media/en/technical-documentation/user-guides/ADSP-BF533_ezkit_man_rev.3.2.pdf)：Audio Interface、SW9/SW12、板级连接。
- [ADI关于AD1836采样时钟的FAQ](https://ez.analog.com/other-products/a/documents/c/ad1836-faq/DO20331/ad1836-sampling-at-22-05khz-23512745)：MCLK/时钟模式共同决定实际采样率。
- 本地`软硬件手册/ADSP-BF533_hwr_rev3.6.pdf`：DMA CONFIG图9-3（PDF页368）确认DI_SEL=1为行中断；PLL章节说明CCLK换算与bypass。

## 实板关键词识别

在 BF533 EZ-KIT Lite 接好 USB Debug Agent，并将 Windows 音频输出连到板卡输入后，在仓库根目录运行：

```powershell
.\board\codec_capture\run_recognize.ps1
.\board\codec_capture\run_recognize.ps1 -Wav 'C:\path\to\test.wav'
.\board\codec_capture\run_recognize.ps1 -BuildOnly
```

脚本经 CCES 将 DXE 加载到板上 RAM。默认从本地测试集生成并循环播放覆盖 11 个目标词的录音。构建依赖 Python 与 NumPy，可用 `-Python` 指定解释器。板卡从当前接线实测有信号的 slot 2 采集，运行 35 秒。测试结束时脚本读回板上事件并校验。默认录音要求结果覆盖至少一轮，且所有输出词与循环顺序相符。自定义 WAV 时检查是否检测到关键词。

上板程序使用调试会话配置的 SDRAM，环形缓冲区位于 0x01600000。SPORT0 DMA 在 MFCC、CNN 和判决运行时继续接收。首次尝试中权重 MemDMA 未完成，因此本测试版本用 CPU 复制权重和恢复 FFT 表，异步权重搬运需单独上板验证。采集期间将事件记录到 RAM，结束后统一读回，以免调试器 I/O 打断音频采集。

### LED 动作

LED4–LED9 由 Flash A 的 Port B PB0–PB5 驱动。初始全灭，选中位置为 LED4。静音和非目标词不改变灯状态。

| 命令 | 动作 |
| --- | --- |
| four、five、six、seven、eight、nine | 翻转对应的 LED4–LED9，并记住该灯位置 |
| left、right | 翻转上次选中灯的左邻或右邻，并选中该灯。两端循环相接 |
| go | 从选中灯开始单灯向右循环，每 120 ms 前进一格 |
| stop | 停止流水灯，恢复流水灯启动前的手动灯状态 |
| off | 停止流水灯并熄灭所有灯 |

数字或方向命令也会结束流水灯，并在手动灯状态上执行翻转。流水灯按音频块计时，更新时不阻塞识别。


测试脚本在实板上逐事件核对 LED 输出寄存器。

### 手动播放与持续识别

```powershell
.\board\codec_capture\run_listen.ps1
```

等待 CCES 连接并启动板卡，然后从 Windows 的耳机输出手动播放任意音频。脚本不播放录音，板卡持续采集并用 LED 显示最后一个识别结果。终端会保持调试会话，按 Ctrl+C 退出。该模式没有 35 秒限制，板上只保留最近 32 个事件。

持续模式每次识别后也会在终端打印采集时间、单词和 LED 掩码。CCES 调试输出期间会短暂停采，打印结束后自动恢复。显示的时间按已采集的音频块累计。
