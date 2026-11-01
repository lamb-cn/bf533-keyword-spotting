#include "frontend.h"
void fe_window_asm(const short*,complex_fract16*);
void fe_window(const short *pcm,complex_fract16 *out){fe_window_asm(pcm,out);}
static unsigned isqrt(unsigned x){
 unsigned r=0,b=1u<<30;while(b>x)b>>=2;
 while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;
}
void fe_magnitude(const complex_fract16 *fft,short *m){
 int i;for(i=0;i<512;i++){
  int re=fft[i].re,im=fft[i].im;
  unsigned a=isqrt((unsigned)(re*re)+(unsigned)(im*im));
  m[i]=(short)(a>32767?32767:a);
 }
}
void fe_mel(const short *m,int *out){
 int i,offset=0;for(i=0;i<40;i++){
  out[i]=fe_dot(m+fe_mel_start[i],fe_mel_weight+offset,fe_mel_len[i]);
  offset+=fe_mel_len[i];
 }
}
extern const short fe_dct_packed[400];
typedef struct {const short*x;const short*w;int*out;int k,pairs;} FeMat;
void fe_dct_matvec(const FeMat*);
void fe_dct40(const short *x,int *out){
 FeMat m;m.x=x;m.w=fe_dct_packed;m.out=out;m.k=40;m.pairs=5;
 fe_dct_matvec(&m);
}
