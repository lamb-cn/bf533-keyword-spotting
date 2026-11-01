# DS-CNN remaining operators — proposed design

Continue the approved BF533 integer ASM route, preserving deployed model arithmetic.

1. Fused bias + rounding + arithmetic shift + INT8 saturation + optional ReLU. Bias/rounding constants prepared once per layer; assembly processes vectors with hardware loops. Replace the current per-element C postprocessing after correctness checks.
2. Global average pool: HWC 25x5x64 -> 64, retaining upstream average-pool output shift=2 and exact signed division/truncation semantics. Assembly channel-pair sums and exact integer normalization; no approximate reciprocal without exhaustive proof over possible sums.
3. Fully connected: 64 -> 12, reuse existing dual-output matvec assembly, pack six output channel pairs; bias left shift2 and output right shift7, no ReLU.
4. Argmax of 12 scores as an optional result utility. Original run_nn has no softmax; do not add probability conversion or MFCC in this change.

Validation: independent C golden only, signed extremes, zero, tie handling for argmax, quantization boundaries, all pooling sums where practical, plus full-size random vectors. No C performance baseline. MAC-bearing FC compares cycles/MAC to FIR 2144/2080; pooling/postprocessing report cycles/output and total cycles because they are not MAC operators.

Alternative: keep C pooling/postprocessing and only ASM FC; rejected as recommended route because existing measurements show postprocessing is a significant cost. Prefer ASM all remaining network operators and fuse postprocessing into convolution layer wrappers.
