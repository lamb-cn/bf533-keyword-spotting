# 本地关键词测试音频小样本

来源：Google Speech Commands 官方独立测试集 v0.02。
下载地址：https://storage.googleapis.com/download.tensorflow.org/data/speech_commands_test_set_v0.02.tar.gz

每类选取压缩包顺序中的前5条，共12类、60条：yes/no/up/down/left/right/on/off/stop/go、_silence_、_unknown_。

格式：16kHz、单声道、16bit PCM WAV。manifest.csv记录逐文件标签、采样点数、时长、字节数及SHA256；source.json记录来源和选取规则。

用途：MFCC与端到端推理功能测试、播放演示。不是随机代表性抽样，不能用60条小样本准确率代替完整测试集指标。当前历史预训练权重的数据版本尚待确认，不应把v0.02结果直接当作论文V1结果。

已下载音频，尚未运行BF533模型识别。数据原始发布采用CC BY 4.0，来源/许可说明见Google Speech Commands数据集论文：https://arxiv.org/abs/1804.03209 。本次未保留完整压缩包。
