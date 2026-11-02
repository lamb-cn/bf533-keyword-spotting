#include "capture.h"
#include "decimator_coeffs.h"
#include <string.h>
int raw[8*CAPTURE_WORDS];short out[40],history[63];
volatile unsigned result[12];
static int golden(short x){
 unsigned i;long long sum=0;
 for(i=62;i>0;i--)history[i]=history[i-1];history[0]=x;
 for(i=0;i<63;i++)sum+=(long long)history[i]*decimator_h[i];
 sum=sum>=0?sum/32768:-((-sum+32767)/32768);
 return sum>32767?32767:(sum< -32768?-32768:(int)sum);
}
void main(void){
 unsigned b,i,slot,c;int expected[40],count=0;
 if(capture_pcm24(0x7fffff)!=32767 || capture_pcm24(0x800000)!=-32768 || capture_pcm24(0xffffff)!=-1)result[1]++;
 capture_test_reset(raw,0);
 for(b=0;b<32;b++){
  slot=b%8;count=0;
  for(i=0;i<120;i++){
   short x=(short)(((b*120+i)*977)%60001-30000);int g=golden(x);
   for(c=0;c<4;c++)raw[slot*CAPTURE_WORDS+i*4+c]=c?0x7fffff:((unsigned short)x<<8);
   if((i+1)%3==0)expected[count++]=g;
  }
  capture_publish_row((slot+1)%8,b*1282500u);
  if(capture_read40(out)!=40)result[2]++;
  for(i=0;i<40;i++)if(out[i]!=expected[i])result[3]++;
 }
 if(capture_stats.produced!=32 || capture_stats.consumed!=32 || capture_stats.pcm16_samples!=1280 || capture_stats.fault)result[4]++;
 for(c=0;c<4;c++){
  capture_test_reset(raw,c);memset(history,0,sizeof(history));count=0;
  for(i=0;i<120;i++){
   int g=golden(3200);unsigned j;
   for(j=0;j<4;j++)raw[i*4+j]=j==c?(3200<<8):0;
   if((i+1)%3==0)expected[count++]=g;
  }
  capture_publish_row(1,100);if(capture_read40(out)!=40)result[2]++;
  for(i=0;i<40;i++)if(out[i]!=expected[i])result[3]++;
 }
 capture_test_reset(raw,0);
 for(i=0;i<8;i++)capture_publish_row((i+1)%8,i*100);
 if(capture_stats.fault!=CAPTURE_OVERRUN || capture_stats.overruns!=1 || capture_read40(out)!=-1)result[5]++;
 capture_test_reset(raw,0);capture_publish_row(3,100);
 if(capture_stats.fault!=CAPTURE_SEQUENCE)result[6]++;
 if(!capture_clock_ok(513000000u,10260000u) || capture_clock_ok(513000000u,5130000u) || capture_clock_ok(513000000u,20520000u))result[6]++;
 if(capture_cclk_from_regs(27000000u,19u<<9,0,2)!=513000000u || capture_cclk_from_regs(27000000u,20u<<9,16,2)!=270000000u || capture_cclk_from_regs(27000000u,(38u<<9)|1,0,2)!=513000000u || capture_cclk_from_regs(27000000u,256,0,1)!=27000000u)result[6]++;
 result[0]=0x600dcafe;
}
