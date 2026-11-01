#ifndef FE_OPS_H
#define FE_OPS_H
#include <complex.h>
extern const short fe_hann[640],fe_dct[400],fe_mel_start[40],fe_mel_len[40],fe_mel_weight[];
/* Integer dot product with Q15 coefficients; output floor(sum/32768).
 * n>0; 40bit accumulator must not overflow. No overlap. */
int fe_dot(const short *x,const short *w,int n);
/* 640 PCM16 samples -> 1024 complex Q15 windowed/zero padded input. */
void fe_window(const short *pcm,complex_fract16 *out);
/* magnitude same integer scale as complex input; uses integer sqrt. */
void fe_magnitude(const complex_fract16 *fft,short *magnitude);
void fe_mel(const short *magnitude,int *energies);
/* Input log-Mel in signed Q8, output 10 signed Q8 coefficients. */
void fe_dct40(const short *logmel,int *out);
/* e>0: ln(e * 2^(fft_exponent-15)); e==0: ln(FLT_MIN).
 * Exponent is the actual FFT block exponent, normally10 for static1024 FFT.
 * Log-Mel output Q8, nearest with ties away from zero. */
void fe_log40(const int *energies,int fft_exponent,short *logmel);
/* DCT Q8 -> model Q1, ties away from zero then signed INT8 saturation. */
void fe_quantize10(const int *cepstra,signed char *out);
typedef struct {
 complex_fract16 frame[1024],spectrum[1024],twiddle[512];
 short magnitude[512];
 int energies[40];
 short logmel[40];
 int cepstra[10];
} FeWorkspace;
/* One-time init then repeated frame calls; no allocation or table generation
 * in the timed frame call. 640 PCM16 ->10 Q1 signed INT8 MFCC. */
void fe_init(FeWorkspace *w);
int fe_mfcc640(const short *pcm,signed char *out,FeWorkspace *w);
#endif
