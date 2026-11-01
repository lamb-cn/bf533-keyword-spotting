#include "cnn.h"
/* Same prepared buffers / raw INT32 output contract as ASM. Compiled with -O.
 * Deliberately conventional scalar C; compiler optimizations are enabled.
 */
void cnn_matvec_c(const MatArgs *a){
 int p,k,s0,s1;
 for(p=0;p<a->pairs;p++){
  s0=0;s1=0;
  for(k=0;k<a->k;k++){
   int x=a->x[k];
   s0+=x*a->w[(p*a->k+k)*2];
   s1+=x*a->w[(p*a->k+k)*2+1];
  }
  a->out[p*2]=s0;a->out[p*2+1]=s1;
 }
}
void cnn_dw3_c(const DwArgs *a){
 int c,y,x,s;
 for(c=0;c<a->channels;c++){
  s=0;
  for(y=0;y<3;y++)for(x=0;x<3;x++)
   s+=(int)a->x[y*(a->row_bytes/2)+x*a->channels+c]*a->w[(y*3+x)*a->channels+c];
  a->out[c]=s;
 }
}
