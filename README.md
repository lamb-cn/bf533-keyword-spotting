# BF533 英语关键词识别

基于 ADSP-BF533 的离线英语关键词识别课程项目，采用 DS-CNN Small 模型，重点研究 Blackfin 汇编算子优化及实时音频处理。

## 识别目标

10个英语关键词：`yes`、`no`、`up`、`down`、`left`、`right`、`on`、`off`、`stop`、`go`，另包含 `silence` 和 `unknown`，共12类输出。

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

## 参考资料

- [Arm ML-KWS-for-MCU](https://github.com/ARM-software/ML-KWS-for-MCU)
- [Hello Edge: Keyword Spotting on Microcontrollers](https://arxiv.org/abs/1711.07128)
- [Google Speech Commands](https://arxiv.org/abs/1804.03209)
