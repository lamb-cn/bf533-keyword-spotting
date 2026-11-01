#ifndef FRONTEND32_H
#define FRONTEND32_H
#include "../mfcc_ops/frontend.h"
typedef struct {
 complex_fract32 frame[1024],spectrum[1024],twiddle[512];
 float magnitude[512];
 short logmel[40];int cepstra[10];
} FeWorkspace32;
void fe32_init(FeWorkspace32 *w);
void fe32_compute(const short *pcm,signed char *out,FeWorkspace32 *w);
#endif
