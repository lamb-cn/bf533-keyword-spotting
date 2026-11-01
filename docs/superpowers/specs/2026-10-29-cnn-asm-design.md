# BF533 convolution design

Approved scope: all three DS-CNN Small convolution kinds, using signed INT8 values expanded to INT16. Benchmark synthetic deterministic tensors at upstream dimensions; this is not trained-network accuracy validation.

Regular 10x4 (49x10x1 -> 25x5x64, stride 2, pad 4/1) and pointwise (25x5x64 -> same) use a dual-output matrix/vector assembly kernel. Weights are packed [output_pair][input][2], input uses B0/I0/L0 circular addressing. Each inner cycle issues two integer MACs and two loads. Depthwise 3x3 uses adjacent HWC channels, a padded input and tap-major weights; unrolled nine taps and DAG strides avoid im2col replication.

Assembly returns raw INT32 sums. C adds bias, rounding, arithmetic right shift, saturation and optional ReLU. INT16 storage must contain only [-128,127]; no Q15 fractional scaling. C callable ABI, no speculative reads beyond buffers, clear L registers on return. Four-byte aligned buffers; positive K and even output channels. Model dimensions satisfy constraints.

Compare every output with independent scalar convolution with explicit boundary checks. Include signed extremes, zero tensors, edge padding, biased rounding and saturation. Report optimized C baseline, assembly core, preprocessing, postprocessing, full layer cycles. One-time weight conversion is separate. L1 placement and simulator-only scope must be reported.

Alternatives: compact INT8 needs repeated unpacking; selected INT16 uses more memory but exposes dual MAC. Depthwise im2col would reuse the matrix kernel but adds avoidable traffic, so use direct depthwise.
