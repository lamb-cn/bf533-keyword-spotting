#include "frontend.h"
#include <filter.h>
#include <math.h>
#include <stdlib.h>
extern const short fe_test_tone[1024];
#pragma section("L1_data_a")
complex_fract16 input[1024];
#pragma section("L1_data_b")
complex_fract16 output[1026];
#pragma section("L1_data_b")
complex_fract16 twiddle[512];
short pcm[640],mag[512],logmel[40];
int energies[42],cepstra[12];
volatile unsigned result[12];
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
static int golden(const short*x,const short*w,int n){
 long long s=0;int i;for(i=0;i<n;i++)s+=(long long)x[i]*w[i];
 return s>=0?(int)(s/32768):-(int)((-s+32767)/32768);
}
void main(void){
 int i,j,k,exponent,offset,mode;unsigned t;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 twidfftrad2_fr16(twiddle,1024);
 output[1024].re=1234;output[1024].im=-2345;
 energies[40]=123456;cepstra[10]=654321;
 for(mode=0;mode<4;mode++){
  for(i=0;i<640;i++)pcm[i]=(short)(mode==0?0:(mode==1?(i==0?16384:0):(mode==2?1000:((i*1777)%60001-30000))));
  t=ticks();fe_window(pcm,input);result[4]=ticks()-t;
  for(i=0;i<640;i++)if(input[i].re!=golden(pcm+i,fe_hann+i,1)||input[i].im)result[1]++;
  for(i=640;i<1024;i++)if(input[i].re||input[i].im)result[1]++;
  /* FFT analytical DC fixture, normalized 1/N, every output checked. */
  for(i=0;i<1024;i++){input[i].re=mode==0?0:(mode==1?(i==0?16384:0):(mode==2?1024:fe_test_tone[i]));input[i].im=0;}
  t=ticks();cfft_fr16(input,output,twiddle,1,1024,&exponent,1);result[5]=ticks()-t;
  if(exponent!=10)result[2]++;
  for(i=0;i<1024;i++){
   int expected=mode==1?16:(mode==2&&i==0?1024:(mode==3&&(i==17||i==1007)?4096:0));
   if(abs(output[i].re-expected)>2||abs(output[i].im)>2)result[2]++;
  }
  t=ticks();fe_magnitude(output,mag);result[6]=ticks()-t;
  for(i=0;i<512;i++){
   int re=output[i].re,im=output[i].im;
   int expected=(int)sqrt((double)re*re+(double)im*im);
   if(expected>32767)expected=32767;if(mag[i]!=expected)result[1]++;
   mag[i]=(short)((i*123+mode*77)%20000);
  }
  t=ticks();fe_mel(mag,energies);result[7]=ticks()-t;
  offset=0;for(i=0;i<40;i++){
   if(energies[i]!=golden(mag+fe_mel_start[i],fe_mel_weight+offset,fe_mel_len[i]))result[3]++;
   offset+=fe_mel_len[i];
  }
  for(i=0;i<40;i++)logmel[i]=(short)(mode==0?0:(mode==1?-32768:(mode==2?32767:(i*1971-30000))));
  t=ticks();fe_dct40(logmel,cepstra);result[8]=ticks()-t;
  for(i=0;i<10;i++)if(cepstra[i]!=golden(logmel,fe_dct+i*40,40))result[3]++;
 }
 result[9]=(output[1024].re!=1234 || output[1024].im!=-2345 || energies[40]!=123456 || cepstra[10]!=654321);
 result[0]=0x600dcafe;
}
