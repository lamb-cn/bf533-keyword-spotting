#ifndef BF533_CNN_H
#define BF533_CNN_H
/* Little-endian BF533, sizeof(int)=4, sizeof(short)=2.
 * All pointers four-byte aligned. Values in short storage MUST be [-128,127].
 * No bias/quantization in assembly: exact signed integer sums, not Q15.
 * Caller owns DAG/M/loop registers (standard C ABI). L0..L3 zero on return.
 */
typedef struct {
    const short *x;
    const short *w; /* [output_pair][k][2] */
    int *out;
    int k;         /* positive EVEN k */
    int pairs;     /* positive */
} MatArgs;
typedef struct {
    const short *x; /* top-left of 3x3 window in padded HWC */
    const short *w; /* [9][channels] */
    int *out;
    int channels;  /* positive even */
    int row_bytes; /* padded row stride */
} DwArgs;
void cnn_matvec(const MatArgs *a);
void cnn_dw3(const DwArgs *a);
#endif
