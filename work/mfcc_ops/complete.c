#include "frontend.h"
#include <filter.h>
#include <math.h>
#include <float.h>
static int nearest(float x){return x>=0?(int)(x+0.5f):-(int)(-x+0.5f);}
void fe_log40(const int *e,int exponent,short *out){
 int i;float correction=(exponent-15)*0.69314718055994530942f;
 for(i=0;i<40;i++){
  float y=e[i]>0?logf((float)e[i])+correction:logf(FLT_MIN);
  out[i]=(short)nearest(y*256.0f);
 }
}
void fe_init(FeWorkspace *w){twidfftrad2_fr16(w->twiddle,1024);}
int fe_mfcc640(const short *pcm,signed char *out,FeWorkspace *w){
 int exponent;
 fe_window(pcm,w->frame);
 cfft_fr16(w->frame,w->spectrum,w->twiddle,1,1024,&exponent,1);
 fe_magnitude(w->spectrum,w->magnitude);
 fe_mel(w->magnitude,w->energies);
 fe_log40(w->energies,exponent,w->logmel);
 fe_dct40(w->logmel,w->cepstra);
 fe_quantize10(w->cepstra,out);
 return exponent;
}
