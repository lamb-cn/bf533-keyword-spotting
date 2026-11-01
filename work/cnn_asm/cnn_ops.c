#include "cnn_ops.h"
#include "tail_ops.h"
#include <string.h>
void cnn_pack_weights(int kind,const signed char *w,short *packed_w) {
 int p,k,n=kind==CNN_REGULAR?40:64;
 if(kind==CNN_DEPTHWISE){for(k=0;k<576;k++)packed_w[k]=w[k];return;}
 for(p=0;p<32;p++)for(k=0;k<n;k++){
  packed_w[(p*n+k)*2]=w[(p*2)*n+k];
  packed_w[(p*n+k)*2+1]=w[(p*2+1)*n+k];
 }
}
void cnn_prepare(int kind,const signed char *in,short *activation) {
 int y,x,c;
 if(kind==CNN_DEPTHWISE){
  memset(activation,0,27*7*64*sizeof(short));
  for(y=0;y<25;y++)for(x=0;x<5;x++)for(c=0;c<64;c++)
   activation[((y+1)*7+x+1)*64+c]=in[(y*5+x)*64+c];
 }else if(kind==CNN_POINTWISE){
  for(c=0;c<8000;c++)activation[c]=in[c];
 }
}
void cnn_make_patch(const signed char *in,int pixel,short *patch){
 int y,x,iy,ix,oy=pixel/5,ox=pixel%5;
 for(y=0;y<10;y++)for(x=0;x<4;x++){
  iy=oy*2+y-4;ix=ox*2+x-1;
  patch[y*4+x]=(iy>=0 && iy<49 && ix>=0 && ix<10)?in[iy*10+ix]:0;
 }
}
void cnn_pixel(int kind,int pixel,const short *activation,const short *packed_w,
 const short *patch,int *sums){
 MatArgs m;DwArgs d;
 if(kind==CNN_DEPTHWISE){
  d.x=activation+((pixel/5)*7+pixel%5)*64;
  d.w=packed_w;d.out=sums;d.channels=64;d.row_bytes=7*64*2;
  cnn_dw3(&d);
 }else{
  m.x=kind==CNN_REGULAR?patch:activation+pixel*64;
  m.w=packed_w;m.out=sums;m.k=kind==CNN_REGULAR?40:64;m.pairs=32;
  cnn_matvec(&m);
 }
}
signed char cnn_quantize(int sum,int bias,int bl,int rs,int relu){
 int q=(sum+bias*(1<<bl)+(rs?(1<<(rs-1)):0))>>rs;
 if(q>127)q=127;if(q< -128)q=-128;if(relu && q<0)q=0;
 return (signed char)q;
}
void cnn_layer(int kind,const signed char *in,const short *packed_w,
 const signed char *bias,int bl,int rs,int relu,
 short *activation,short *patch,int *sums,signed char *out){
 int p,c,add[64];PostArgs q;
 for(c=0;c<64;c++)add[c]=bias[c]*(1<<bl)+(rs?(1<<(rs-1)):0);
 q.sums=sums;q.add=add;q.n=64;q.shift=rs;q.lower=relu?0:-128;
 cnn_prepare(kind,in,activation);
 for(p=0;p<125;p++){
  if(kind==CNN_REGULAR)cnn_make_patch(in,p,patch);
  cnn_pixel(kind,p,activation,packed_w,patch,sums);
  q.out=out+p*64;
  cnn_post(&q);
 }
}
