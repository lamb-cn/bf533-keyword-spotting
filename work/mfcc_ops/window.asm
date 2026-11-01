.section program;
.global _fe_window_asm;
.extern _fe_hann;
.align 8;
_fe_window_asm:
    I0 = R0;
    P1 = R1;
    R2.L = LO(_fe_hann);
    R2.H = HI(_fe_hann);
    I2 = R2;
    P2 = 640;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    R3 = 0;
    LSETUP(WIN_START, WIN_END) LC0 = P2;
WIN_START:
    R0.L = W[I0++] || R1.L = W[I2++];
    A0 = R0.L * R1.L (IS);
    A0 = A0 >>> 15;
    R0 = A0.W;
    W[P1++] = R0;
WIN_END:
    W[P1++] = R3;
    P2 = 384;
    LSETUP(ZERO_END,ZERO_END) LC0 = P2;
ZERO_END:
    [P1++] = R3;
    RTS;
_fe_window_asm.end:

