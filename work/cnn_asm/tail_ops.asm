.section program;
.global _cnn_post;
.global _cnn_pool;
.align 8;
_cnn_post:
    [--SP] = (R7:4);
    P0 = R0;
    R1 = [P0++];
    R2 = [P0++];
    I0 = R1;
    I2 = R2;
    P2 = [P0++];
    P1 = [P0++];
    R3 = [P0++];
    R4 = [P0];
    R3 = -R3;
    R5 = 127;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    LSETUP(POST_START, POST_END) LC0 = P1;
POST_START:
    R0 = [I0++] || R1 = [I2++];
    R0 = R0 + R1;
    R0 = ASHIFT R0 BY R3.L;
    R0 = MIN(R0,R5);
    R0 = MAX(R0,R4);
POST_END:
    B[P2++] = R0;
    (R7:4) = [SP++];
    RTS;
_cnn_post.end:
.align 8;
_cnn_pool:
    [--SP] = (R7:4, P5:3);
    P3 = R0;
    P4 = R1;
    P5 = 32;
    P2 = 124;
    M0 = 128;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    LSETUP(POOL_PAIR, POOL_PAIR_END) LC0 = P5;
POOL_PAIR:
    I0 = P3;
    R0 = 0;
    R1 = [I0++M0];
    LSETUP(POOL_SUM, POOL_SUM) LC1 = P2;
POOL_SUM:
    R0 = R0 +|+ R1 || R1 = [I0++M0];
    R0 = R0 +|+ R1;
    R2 = R0.L (X);
    R3 = R0 >>> 16;
    /* floor(abs(sum)*4/125), corrected reciprocal over bounded domain. */
    R7 = R2 >>> 31;
    R2 = ABS R2;
    R2 = R2 << 2;
    R1 = 33554 (Z);
    A0 = R2.L * R1.L (FU);
    R4 = A0.W;
    R4 = R4 >> 22;
    R5 = 125;
    R5 = R4.L * R5.L (IS);
    R5 = R2 - R5;
    R6 = 125;
    CC = R5 < R6;
    R6 = 1;
    R6 = R4 + R6;
    IF !CC R4 = R6;
    R4 = R4 ^ R7;
    R4 = R4 - R7;
    B[P4++] = R4;
    R2 = R3;
    /* floor(abs(sum)*4/125), corrected reciprocal over bounded domain. */
    R7 = R2 >>> 31;
    R2 = ABS R2;
    R2 = R2 << 2;
    R1 = 33554 (Z);
    A0 = R2.L * R1.L (FU);
    R4 = A0.W;
    R4 = R4 >> 22;
    R5 = 125;
    R5 = R4.L * R5.L (IS);
    R5 = R2 - R5;
    R6 = 125;
    CC = R5 < R6;
    R6 = 1;
    R6 = R4 + R6;
    IF !CC R4 = R6;
    R4 = R4 ^ R7;
    R4 = R4 - R7;
    B[P4++] = R4;
POOL_PAIR_END:
    P3 += 4;
    (R7:4, P5:3) = [SP++];
    RTS;
_cnn_pool.end:

