#include "frontend32.h"
#include <filter.h>
#include <math.h>
#include <float.h>
extern const int window32[640];
#ifdef STREAM_INSTRUMENT
extern volatile unsigned stream_fe32_calls;
#endif
void fe32_init(FeWorkspace32 *w){twidfftrad2_fr32(w->twiddle,1024);}
void fe32_compute(const short *pcm,signed char *out,FeWorkspace32 *w){
 int i,j,offset=0,exponent;float scale;
#ifdef STREAM_INSTRUMENT
 stream_fe32_calls++;
#endif
 for(i=0;i<640;i++){
  w->frame[i].re=(int)(((long long)pcm[i]*window32[i])>>15);
  w->frame[i].im=0;
 }
 for(;i<1024;i++)w->frame[i].re=w->frame[i].im=0;
 cfft_fr32(w->frame,w->spectrum,w->twiddle,1,1024,&exponent,1);
 scale=ldexpf(1.0f,exponent-31);
 for(i=0;i<512;i++){
  float re=(float)w->spectrum[i].re,im=(float)w->spectrum[i].im;
  w->magnitude[i]=sqrtf(re*re+im*im)*scale;
 }
 for(i=0;i<40;i++){
  float energy=0,y;
  for(j=0;j<fe_mel_len[i];j++)
   energy+=w->magnitude[fe_mel_start[i]+j]*((float)fe_mel_weight[offset+j]/32768.0f);
  offset+=fe_mel_len[i];
  if(energy<=0)energy=FLT_MIN;
  y=logf(energy)*256;
  w->logmel[i]=(short)(y>=0?(int)(y+.5f):-(int)(-y+.5f));
 }
 fe_dct40(w->logmel,w->cepstra);fe_quantize10(w->cepstra,out);
}
