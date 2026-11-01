# Execution ledger — 2026-10-29 CNN ASM

- Scope approved: three DS-CNN Small convolution kinds; expanded INT16 route.
- Not a Git checkout; no repository initialized. Artifacts isolated in work/cnn_asm.
- RED: empty assembly stubs fail both hand-derived signed smoke cases (2 errors each).
- GREEN smoke: implemented matvec and depthwise pass the same cases in BF533 simulator.
- Compiler initially exceeded L1 A because default linker reserves 15KB stack+heap. Local wrapper LDF sets minimum 4KB stack + 1KB heap; all workspace buffers fit L1.
- Performance tests use -O C baseline and independent Python direct-convolution oracle. Host oracle lives in external simulator memory only for validation, outside timed operator buffers.
- Full initial C-oracle run replaced with host-oracle comparison to reduce instruction-level simulator validation cost.
- Runner quirks observed: data dumps sharing one trigger overwrite; execution stops on first breakpoint. Use one contiguous result/completion block at _exit; oracle input is loaded before execution, with no trigger.
- Read-only reviewer found no blocking arithmetic/layout/ABI/bounds issue. Minor findings addressed: verify result/completion symbol distance; check guards after full-layer call; TARGET_ORACLE final-reference branch.
- Full-size validation and measurements pending.

- Full validation complete: 15 cases, all raw/final/guard errors zero. User requires FIR-only performance comparison; C timing removed from future bench.

2026-10-30 tail operators complete: strengthened tests and guards pass, three fused convolution types pass; FIR-only performance comparison. See 后续算子报告.md.
