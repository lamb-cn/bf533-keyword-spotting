#include "tail_ops.h"
void cnn_fc_pack(const signed char *w,short *packed){
 int p,k;for(p=0;p<6;p++)for(k=0;k<64;k++){
  packed[(p*64+k)*2]=w[p*128+k];
  packed[(p*64+k)*2+1]=w[p*128+64+k];
 }
}
void cnn_fc_classes(const short *input,const short *packed,const signed char *bias,
 signed char *out,int *sums,int *add,int classes){
 MatArgs m;PostArgs q;int c;
 m.x=input;m.w=packed;m.out=sums;m.k=64;m.pairs=(classes+1)/2;
 cnn_matvec(&m);
 for(c=0;c<classes;c++)add[c]=bias[c]*4+64;
 q.sums=sums;q.add=add;q.out=out;q.n=classes;q.shift=7;q.lower=-128;
 cnn_post(&q);
}
void cnn_fc(const short *input,const short *packed,const signed char *bias,
 signed char *out,int *sums,int *add){
 cnn_fc_classes(input,packed,bias,out,sums,add,12);
}
