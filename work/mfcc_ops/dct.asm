.section program;
.global _fe_dct_matvec;

.align 8;
_fe_dct_matvec:
    /* R0 -> {x,w,out,k,pairs}. No callee-saved registers touched. */
    P0 = R0;
    R1 = [P0++];
    R2 = [P0++];
    I0 = R1;
    B0 = R1;
    I2 = R2;
    P1 = [P0++];
    R2 = [P0++];
    P2 = [P0];
    R3 = R2 << 1;
    L0 = R3;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    R2 += -1;
    P0 = R2;
    LSETUP(FEDCT_PAIR, FEDCT_PAIR_END) LC0 = P2;
FEDCT_PAIR:
    A0 = 0;
    A1 = 0;
    R0.L = W[I0++] || R1 = [I2++];
    LSETUP(FEDCT_K, FEDCT_K) LC1 = P0;
FEDCT_K:
    A0 += R0.L * R1.L, A1 += R0.L * R1.H (IS) || R0.L = W[I0++] || R1 = [I2++];
    /* Drain without reading beyond the last sample/weight. */
    A0 += R0.L * R1.L, A1 += R0.L * R1.H (IS);
    A0 = A0 >>> 15;
    A1 = A1 >>> 15;
    R2 = A0.W;
    R3 = A1.W;
    [P1++] = R2;
FEDCT_PAIR_END:
    [P1++] = R3;
    L0 = 0;
    RTS;
_fe_dct_matvec.end:
