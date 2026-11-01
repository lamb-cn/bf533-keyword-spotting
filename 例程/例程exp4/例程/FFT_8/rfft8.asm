/*****************************************************************************
  File Name      : rfft8.asm

  Include File   : filter.h
  Label name     : __rfft8

  Description    : The C callable prototype of the function is:

                       void rfft_fr16(
                                  const fract16  *in,       // Input array
                                  complex_fract16 *t,       // Temporary buffer
                                  complex_fract16 *out,     // Output array
                                  const complex_fract16 *w, // Twiddle table 
                                  int wst)                  // Twiddle stride
*****************************************************************************/

.section    program;
.global     __rfft8;

.align     8;
__rfft8:


/** Function Prologue **/

        [--SP] = (R7:4, P5:3);         // Save reserved registers
        B0 = R0;                       // Address Input array
        B1 = R1;                       // Address Temporary buffer
        B2 = R2;                       // Address Output array
        R0 = [SP+40];                  // Address Twiddle table
        P1 = [SP+44];                  // Twiddle stride
        R2 = 8;                        // Size FFT

        B3 = R0;                       // Address Twiddle table

        L0 = 0;                        // L registers should be initialized to 0
        L1 = 0;               
        L2 = 0;              
        L3 = 0;            
        R3 = 4;
        CC = R2 <= R3;     
        If CC Jump Terminate;         // Exit if FFT size <= 4 


/** Butterfly - First Stage **/

        I2 = B1;                      // Address Temporary array
        P4 = R2;                      // Size FFT
        M0 = R2;                      // Offset required for bit reversing
        
        R3.H = 0;                     // Clear high bit 
        R5.H = 0;                     // Clear high bit
 
        P5 = P4 >> 2;                 // Set loop counter to Size FFT/4
        R2.H = 0;                     // Clear high bit
        R4.H = 0;                     // Clear high bit
        
        M1 = B0;                      // Address Input array
        I0 = 0;                      
        I1 = M1;

        /* 
         * Read data from input array in bit-reversed order,
         * divide the data by 2 to avoid overflow
         * and perform first butterfly computation. 
         * During each iteration, 4 real values are loaded for processing. 
         * The complex results are written to the temporary buffer.
         *
         * At this stage, the twiddle coefficients can be ignored
         * since their value is always 1 for the real part and 
         * 0 for the imaginary part.
         */
        lsetup(Stage12_strt, Stage12_end) LC0 = P5; 
Stage12_strt:
           I0 += M0 (BREV) || R2.L = W[I1];    
           I1 = I0;
           I1 += M1;
           R2 = R2 >>> 1 (V) || I0 += M0 (BREV) || R3.L = W[I1];
           I1 = I0;
           R3 = R3 >>> 1 (V) || I1 += M1 || NOP;
           R0 = R2 +|+ R3, R1 = R2 -|- R3 (ASR) || I0 += M0 (BREV) || 
                                                         R4.L = W[I1];
           I1 = I0;
           R4 = R4 >>> 1 (V) || I1 += M1 || NOP;
           I0 += M0 (BREV) || R5.L = W[I1];
           R5 = R5 >>> 1 (V);
           R6 = R5 +|+ R4, R7 = R5 -|- R4 (ASR, CO);
           R0 = R0 +|+ R6, R6 = R0 -|- R6 (ASR);
           R1 = R1 +|+ R7, R7 = R1 -|- R7 (ASR) || [I2++] = R0 || NOP;
           I1 = I0;
           I1 += M1 || [I2++] = R1;
           [I2++] = R6;
Stage12_end:
           [I2++] = R7; 

        R1 = P1;                       // Twiddle stride wst
        R1 = R1 << 2;                  // R1 = wst * 4
        P3 = 4;                        // Loop counter number of lines in each 
                                       // butterfly at stage 3.
        R7 = P5;  
        R7.L = R1.L * R7.L (IS);       // R7 = (4 * wst) * (Size FFT / 4) 
                                       //    = wst * Size FFT

        R2 = P4;                       // Size FFT   
        R3 = 8;                
        M1 = 16;                       // Offset for next butterfly

/** Butterfly - Final Stage **/

        //M1 = P2;                       // Offset next butterfly

Esc_mid:
        I0 = B1;                       // Address temporary buffer
        I2 = B1;     

        I1 = B2;                       // Address output array
        I3 = B2;      

        P4 = B3;                       // Address twiddle table
        R7 = R7 >>> 1 || I2 += M1  || R2 = [I0++];
                                       // I2 = &temporary[FFT Size / 2]
        P5 = R7;                       // Twiddle offset

        I3 += M1 || R4 = [I2++];       // I3 = &output[FFT Size / 2]
        R3 = [P4++P5];                 // Skip first set of twiddle coeffs
        R3 = [P4++P5];

        /*
         * Iter for FFT Size / 4
         * Perform last butterfly, reading 4 values from temporary buffer
         * and storing the result in the output array in normal order.
         */
        lsetup(Last_strt, Last_end) LC1 = P3 >> 1;
Last_strt:   
           R2 = R2 +|+ R4, R4 = R2 -|- R4 || R6 = [I2++];
           A1 = R3.L * R6.H, A0 = R3.L * R6.L || [I3++] = R4 || R5 = [I0++];
           R6.H = (A1 += R3.H *R6.L), R6.L = (A0 -= R3.H * R6.H)  || 
                                                              R3 = [P4++P5];
           R5 = R5 +|+ R6, R6 = R5 -|- R6 || R4 = [I2++] || [I1++] = R2;
           A1 = R3.L * R4.H, A0 = R3.L * R4.L || [I1++] = R5;
           R2 = [I0++]; 
Last_end: 
           R4.H = (A1 += R3.H * R4.L), R4.L = ( A0 -= R3.H * R4.H) || 
                                               R3 = [P4++P5] || [I3++] = R6;


/** Function Exit **/

Terminate:
        (R7:4, P5:3) = [SP++];         // Restore preserved registers
        RTS;                           // Return

.__rfft8.end:


