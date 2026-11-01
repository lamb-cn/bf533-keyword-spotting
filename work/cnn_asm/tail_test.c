#include "tail_ops.h"
#pragma section("L1_data_a")
short x[8000];
#pragma section("L1_data_b")
short w[768];
signed char original[768],bias[64],out[68];
int sums[64],add[64];
/* 0 done, 1 post errors, 2 pool errors, 3 FC errors, 4 guard errors,
 * 5 post cycles/64, 6 pool cycles/64, 7 FC core cycles/768MAC, 8 FC full cycles.
 */
volatile unsigned result[12];
static unsigned rng=1234567;
static int random8(void){rng=rng*1664525u+1013904223u;return (int)(rng>>24)-128;}
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
static int golden(int n,int a,int shift,int lower){
 int v=n+a,d=1<<shift;
 int q=v>=0?v/d:-((-v+d-1)/d);
 if(q>127)q=127;if(q<lower)q=lower;return q;
}
void main(void){
 int c,p,s,mode,shift,lo,v,i;unsigned t;PostArgs q;MatArgs m;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 out[64]=91;out[65]=37;
 q.sums=sums;q.add=add;q.out=out;q.n=64;
 for(shift=0;shift<16;shift++)for(lo=0;lo<2;lo++){
  q.shift=shift;q.lower=lo?0:-128;
  for(c=0;c<64;c++){
   sums[c]=(c-32)*17*(1<<shift)+(c%3-1);add[c]=(1<<shift)/2;
   out[c]=90;
  }
  cnn_post(&q);
  for(c=0;c<64;c++)if(out[c]!=golden(sums[c],add[c],shift,q.lower))result[1]++;
 }
 t=ticks();cnn_post(&q);result[5]=ticks()-t;
 /* Sample signed pooling sums, boundaries and byte wrap; 64 sums per call. */
 for(s=-16000;s<=15875;s+=4096){
  for(c=0;c<64;c++){
   int target=s+c;if(target>15875)target=15875;
   int base=target/125,rem=target-base*125;
   for(p=0;p<125;p++)x[p*64+c]=(short)(base+(rem>0 && p<rem?1:0)-(rem<0 && p< -rem?1:0));
  }
  cnn_pool(x,out);
  for(c=0;c<64;c++){
   int target=s+c;if(target>15875)target=15875;
   if(out[c]!=(signed char)(target*4/125))result[2]++;
  }
 }
 for(mode=0;mode<4;mode++){
  for(p=0;p<125;p++)for(c=0;c<64;c++)
   x[p*64+c]=(short)(mode==0?127:(mode==1?-128:(mode==2?0:random8())));
  cnn_pool(x,out);
  for(c=0;c<64;c++){
   v=0;for(p=0;p<125;p++)v+=x[p*64+c];
   if(out[c]!=(signed char)(v*4/125))result[2]++;
  }
 }
 t=ticks();cnn_pool(x,out);result[6]=ticks()-t;
 for(mode=0;mode<5;mode++){
  for(c=0;c<64;c++)x[c]=(short)(mode==1?0:(mode==2?-128:(mode==3?127:random8())));
  for(c=0;c<768;c++)original[c]=(signed char)(mode==2?-128:(mode==3?127:random8()));
  for(c=0;c<12;c++)bias[c]=(signed char)random8();
  cnn_fc_pack(original,w);
  m.x=x;m.w=w;m.out=sums;m.k=64;m.pairs=6;
  t=ticks();cnn_matvec(&m);result[7]=ticks()-t;
  for(c=0;c<12;c++){
   v=0;for(i=0;i<64;i++)v+=(int)x[i]*original[c*64+i];
   if(sums[c]!=v)result[3]++;
  }
  out[12]=91;out[13]=37;
  t=ticks();cnn_fc(x,w,bias,out,sums,add);result[8]=ticks()-t;
  if(out[12]!=91 || out[13]!=37)result[4]++;
  for(c=0;c<12;c++){
   v=0;for(i=0;i<64;i++)v+=(int)x[i]*original[c*64+i];
   if(out[c]!=golden(v,bias[c]*4+64,7,-128))result[3]++;
  }
 }
 result[4]+=(out[64]!=91 || out[65]!=37);
 result[0]=0x600dcafe;
}

