.section program;
.global _cnn_matvec;
.global _cnn_dw3;
.align 8;
_cnn_matvec:
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
    LSETUP(MV_PAIR, MV_PAIR_END) LC0 = P2;
MV_PAIR:
    A0 = 0;
    A1 = 0;
    R0.L = W[I0++] || R1 = [I2++];
    LSETUP(MV_K, MV_K) LC1 = P0;
MV_K:
    A0 += R0.L * R1.L, A1 += R0.L * R1.H (IS) || R0.L = W[I0++] || R1 = [I2++];
    /* Drain without reading beyond the last sample/weight. */
    A0 += R0.L * R1.L, A1 += R0.L * R1.H (IS);
    R2 = A0.W;
    R3 = A1.W;
    [P1++] = R2;
MV_PAIR_END:
    [P1++] = R3;
    L0 = 0;
    RTS;
_cnn_matvec.end:
.align 8;
_cnn_dw3:
    [--SP] = (R7:4, P5:3);
    P2 = R0;
    P0 = [P2++];
    P1 = [P2++];
    P3 = [P2++];
    R2 = [P2++];
    R3 = [P2];
    R1 = R2 << 1;
    M0 = R1;
    R0 = R1 << 1;
    R3 = R3 - R0;
    M2 = R3; /* third column load advances to next row */
    P2 = R2;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    LSETUP(DW_PAIR, DW_PAIR_END) LC0 = P2 >> 1;
DW_PAIR:
    I0 = P0;
    I2 = P1;
    A0 = 0;
    A1 = 0;
    R0 = [I0++M0] || R1 = [I2++M0]; /* tap 0 */
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M0] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M2] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M0] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M0] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M2] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M0] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0++M0] || R1 = [I2++M0];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS) || R0 = [I0] || R1 = [I2];
    A0 += R0.L * R1.L, A1 += R0.H * R1.H (IS);
    R2 = A0.W;
    R3 = A1.W;
    [P3++] = R2;
    [P3++] = R3;
    P0 += 4;
DW_PAIR_END:
    P1 += 4;
    (R7:4, P5:3) = [SP++];
    RTS;
_cnn_dw3.end:
