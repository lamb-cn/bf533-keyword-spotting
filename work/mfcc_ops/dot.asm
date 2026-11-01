.section program;
.global _fe_dot;
.align 8;
_fe_dot:
    I0 = R0;
    I2 = R1;
    R2 += -1;
    P0 = R2;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    A0 = 0;
    R0.L = W[I0++] || R1.L = W[I2++];
    CC = R2 == 0;
    IF CC JUMP DOT_LAST;
    LSETUP(DOT_LOOP, DOT_LOOP) LC0 = P0;
DOT_LOOP:
    A0 += R0.L * R1.L (IS) || R0.L = W[I0++] || R1.L = W[I2++];
DOT_LAST:
    A0 += R0.L * R1.L (IS);
    A0 = A0 >>> 15;
    R0 = A0.W;
    RTS;
_fe_dot.end:
