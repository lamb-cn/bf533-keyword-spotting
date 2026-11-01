#include "frontend.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#pragma section("L1_data_a")
FeWorkspace workspace;
short pcm[640],logs[40];
int energy[40],cepstra[10];
signed char output[12];
volatile unsigned result[12];
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
static int round_g(double x){return x>=0?(int)floor(x+0.5):-(int)floor(-x+0.5);}
static int q_g(long long v){
 long long a=v<0?-v:v,q=a/128;if(a%128>=64)q++;if(v<0)q=-q;
 return q>127?127:(q< -128?-128:(int)q);
}
void main(void){
 int i,j,k,mode,exponent,s,off;unsigned t;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 output[10]=91;output[11]=37;
 /* All FFT exponents and powers across positive int energy range. */
 for(exponent=0;exponent<=10;exponent+=
#ifdef FOCUSED
5
#else
1
#endif
)for(k=0;k<31;k+=
#ifdef FOCUSED
5
#else
1
#endif
){
  for(i=0;i<40;i++)energy[i]=i==0?0:(int)((1u<<k)+(i%7));
  fe_log40(energy,exponent,logs);
  for(i=0;i<40;i++){
   double y=energy[i]?log((double)energy[i])+(exponent-15)*log(2.0):log((double)FLT_MIN);
   if(abs(logs[i]-round_g(y*256))>1)result[1]++;
  }
 }
 t=ticks();fe_log40(energy,10,logs);result[5]=ticks()-t;
 /* Quantizer boundaries, ties, saturation and INT32 endpoints. */
 for(s=-20000;s<=20000;s+=10){
  for(i=0;i<10;i++)cepstra[i]=s+i;
  fe_quantize10(cepstra,output);
  for(i=0;i<10;i++)if(output[i]!=q_g(cepstra[i]))result[2]++;
 }
 cepstra[0]=INT_MIN;cepstra[1]=INT_MAX;
 fe_quantize10(cepstra,output);
 if(output[0]!=-128||output[1]!=127)result[2]++;
 t=ticks();fe_quantize10(cepstra,output);result[6]=ticks()-t;
 fe_init(&workspace);
 for(mode=0;mode<3;mode++){
  for(i=0;i<640;i++)pcm[i]=(short)(mode==0?0:(mode==1?12000:((i*1777)%60001-30000)));
  t=ticks();exponent=fe_mfcc640(pcm,output,&workspace);result[7+mode]=ticks()-t;
  if(exponent!=10)result[3]++;
  /* Independent C golden downstream from separately validated FFT output. */
  for(i=0;i<512;i++){
   double re=workspace.spectrum[i].re,im=workspace.spectrum[i].im;
   s=(int)sqrt(re*re+im*im);if(s>32767)s=32767;workspace.magnitude[i]=(short)s;
  }
  off=0;
  for(i=0;i<40;i++){
   long long sum=0;for(j=0;j<fe_mel_len[i];j++)sum+=(long long)workspace.magnitude[fe_mel_start[i]+j]*fe_mel_weight[off+j];
   off+=fe_mel_len[i];energy[i]=(int)(sum/32768);
   if(energy[i]!=workspace.energies[i])result[3]++;
   double y=energy[i]?log((double)energy[i]/32):log((double)FLT_MIN);
   logs[i]=(short)round_g(y*256);
  }
  for(i=0;i<10;i++){
   long long sum=0;for(j=0;j<40;j++)sum+=(long long)logs[j]*fe_dct[i*40+j];
   int q=sum>=0?(int)(sum/32768):-(int)((-sum+32767)/32768);
   if(abs(output[i]-q_g(q))>1)result[3]++;
  }
 }
 result[4]=(output[10]!=91||output[11]!=37);
 result[0]=0x600dcafe;
}
