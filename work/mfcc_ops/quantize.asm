.section program;
.global _fe_quantize10;
.align 8;
_fe_quantize10:
    [--SP] = R4;
    P0 = R0;
    P1 = R1;
    P2 = 10;
    R2 = 64;
    R3 = 127;
    R4 = -128;
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    LSETUP(Q10_START,Q10_END) LC0 = P2;
Q10_START:
    R0 = [P0++];
    R1 = R0 >>> 31;
    /* unsigned abs, including INT_MIN; logical >> handles the high bit */
    R0 = R0 ^ R1;
    R0 = R0 - R1;
    R0 = R0 + R2 (NS);
    R0 = R0 >> 7;
    R0 = R0 ^ R1;
    R0 = R0 - R1;
    R0 = MIN(R0,R3);
    R0 = MAX(R0,R4);
Q10_END:
    B[P1++] = R0;
    R4 = [SP++];
    RTS;
_fe_quantize10.end:
