# BF533 英语关键词识别

基于 ADSP-BF533 的离线英语关键词识别课程项目，采用 DS-CNN Small 模型，重点研究 Blackfin 汇编算子优化及实时音频处理。

## 识别目标

13 类模型包含 `four`、`five`、`six`、`seven`、`eight`、`nine`、`go`、`stop`、`left`、`right`、`off`，以及静音和非目标词。

## 计算流程

```text
音频采集 → 16kHz单声道PCM → MFCC → 49×10特征
→ 普通卷积 → 4组depthwise/pointwise卷积
→ 全局平均池化 → 全连接 → 类别得分
```

- MFCC：40ms帧长、20ms帧移，每帧10个系数。
- 输入窗口覆盖约1秒音频，拟每100–200ms更新一次识别结果。
- 模型约266万MAC，权重和层间激活采用INT8；当前汇编内核使用符号展开的INT16数据进行整数运算。

## 开发环境

- 处理器：ADSP-BF533。
- 工具链：CrossCore Embedded Studio 2.10.0。
- 验证：BF533单处理器仿真器

## 仿真运行

在仓库根目录使用 PowerShell 7 执行：

```powershell
.\work\end_to_end\run.ps1
.\work\end_to_end\run.ps1 -Accuracy
```

脚本使用 CCES 2.10 的 BF533 单处理器仿真器，读取仓库内置音频和模型，运行 MFCC 和 CNN，再对照参考结果验证。需安装脚本所用的 Python 环境及 NumPy。

默认端到端入口使用全网微调后的 13 类 INT8 权重。`-Accuracy` 运行本地固定的 37 条样本，逐条核对板级定点卷积、池化和 FC 输出，统计仿真预测。样本来自官方测试划分，每个非静音类别随机抽取三条，另有一条数字静音。完整 5,298 条的训练侧定点评估见 [训练报告](work/kws13/finetune/report13.json)。

历史长音频回放仍使用原 12 类模型；命令见 [流式验证说明](work/streaming/流式验证说明.md)。

## 参考资料

- [Arm ML-KWS-for-MCU](https://github.com/ARM-software/ML-KWS-for-MCU)
- [Hello Edge: Keyword Spotting on Microcontrollers](https://arxiv.org/abs/1711.07128)
- [Google Speech Commands](https://arxiv.org/abs/1804.03209)
