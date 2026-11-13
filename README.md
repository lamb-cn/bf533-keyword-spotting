# BF533 英语关键词识别

基于 ADSP-BF533 的离线英语关键词识别课程项目，采用 DS-CNN Small 模型，重点研究 Blackfin 汇编算子优化及实时音频处理。

## 识别目标

上板模型识别 `four`、`five`、`six`、`seven`、`eight`、`nine`、`go`、`stop`、`left`、`right`、`off`，另包含静音和非目标词，共 13 类。

## 实时上板

运行 `.\board\codec_capture\run_listen.ps1` 启动持续识别。启动脚本从 13 类 INT8 权重生成板上镜像。灯型和测试命令见 [上板说明](board/codec_capture/README.md)。

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

## 项目说明文档

- [项目交接与开发状态](BF533语音识别项目交接.md)
- [完整计算流程](DSP语音识别完整计算流程.md)
- [实时采集、识别与命令动作](board/codec_capture/README.md)
- [流水灯与关键词动作](board/codec_capture/流水灯与关键词动作.md)
- [测试音频数据说明](data/speech_commands_test_small/README.md)
- [端到端仿真报告](work/end_to_end/端到端仿真报告.md)
- [数据搬运与调度设计](work/end_to_end/数据搬运与调度设计.md)
- [CNN 汇编性能报告](work/cnn_asm/性能报告.md)
- [CNN 后续算子报告](work/cnn_asm/后续算子报告.md)
- [MFCC 实现与验证](work/mfcc_ops/实现与验证.md)
- [MFCC 完整帧 API](work/mfcc_ops/complete_api.md)
- [流式验证说明](work/streaming/流式验证说明.md)
- [长语音流式仿真历史](work/streaming/当前进展报告.md)
- [流式测试停止时记录](work/streaming/停止时进展.md)
- [CNN 汇编设计计划](docs/superpowers/plans/2026-10-29-cnn-asm.md)
- [尾部算子设计计划](docs/superpowers/plans/2026-10-30-tail-ops.md)
- [CNN 汇编设计规格](docs/superpowers/specs/2026-10-29-cnn-asm-design.md)
- [后续算子设计规格](docs/superpowers/specs/2026-10-30-remaining-ops-design.md)
## 参考资料

- [Arm ML-KWS-for-MCU](https://github.com/ARM-software/ML-KWS-for-MCU)
- [Hello Edge: Keyword Spotting on Microcontrollers](https://arxiv.org/abs/1711.07128)
- [Google Speech Commands](https://arxiv.org/abs/1804.03209)
