#include <cdefBF533.h>
#include <ccblkfn.h>
#include <sys/exception.h>
#include <string.h>
#include <stdio.h>
#include "../mfcc_ops/frontend.h"
#include "../cnn_asm/cnn_ops.h"
#include "../cnn_asm/tail_ops.h"
#include "model.h"
#include "frontend32.h"
/* Simulator-only reserved external memory map, loaded by runner.
 * On hardware these must be placed by the SDRAM linker/startup project. */
#define AUDIO ((const short*)0x01000000)
#define ACT0 ((signed char*)0x01200000)
#define ACT1 ((signed char*)0x01202000)
#define EXPORT ((unsigned char*)0x01400000)
#pragma section("L1_data_a")
union {FeWorkspace32 mfcc;short act[12096];} scratch;
typedef char workspace_must_fit[(sizeof(FeWorkspace32)<=24192)?1:-1];
#pragma section("L1_data_a")
short patch[64];
#pragma section("L1_data_b")
short weight[2][4100];
#pragma section("L1_data_b")
short audio[2][644];
signed char features[490],pool[64],scores[12];
int sums[64],fc_add[12];
volatile unsigned stats[64];
volatile unsigned ws[2],as[2],wslot,aslot,windex;
#ifdef ACTIVATION_DMA
#ifndef TILE_PIXELS
#define TILE_PIXELS 4
#endif
volatile unsigned output_phase;
static int output_inflight=-1;
#endif
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
#ifdef STREAMING
extern volatile unsigned stream_twiddle_active;
void stream_twiddle_isr(unsigned status);
#endif
EX_INTERRUPT_HANDLER(weight_isr){
 unsigned s=*pMDMA_D0_IRQ_STATUS;*pMDMA_D0_IRQ_STATUS=s&3;
#ifdef STREAMING
 if(stream_twiddle_active){stream_twiddle_isr(s);return;}
#endif
 if(s&DMA_ERR)stats[1]++;
 if(s&DMA_DONE){if(ws[wslot]!=1)stats[1]++;ws[wslot]=2;stats[2]++;stats[30+windex]=ticks();}
}
EX_INTERRUPT_HANDLER(audio_isr){
 unsigned s=*pMDMA_D1_IRQ_STATUS;*pMDMA_D1_IRQ_STATUS=s&3;
 if(s&DMA_ERR)stats[1]++;
 if(s&DMA_DONE){
  if(as[aslot]!=1)stats[1]++;as[aslot]=2;
#ifdef ACTIVATION_DMA
  if(output_phase)stats[61]++;else
#endif
  stats[3]++;
 }
}
static void dma_setup(void){
 /* Lower IVG number = higher priority. SPORT RX reserved IVG8, offline
  * audio-frame prefetch MDMA1 IVG9, weights MDMA0 IVG10. */
 *pSIC_IAR1=(*pSIC_IAR1&~(15u<<4))|(1u<<4);
 *pSIC_IAR2=(*pSIC_IAR2&~((15u<<20)|(15u<<24)))|(3u<<20)|(2u<<24);
 register_handler(ik_ivg9,audio_isr);register_handler(ik_ivg10,weight_isr);
 *pSIC_IMASK|=MDMA0_IRQ|MDMA1_IRQ;ssync();
}
static void start_weight(unsigned i){
 unsigned slot=i&1;const Layer *l=layers+i;
 if(ws[slot]!=0){stats[1]++;return;}
 ws[slot]=1;wslot=slot;windex=i;
 *pMDMA_S0_CONFIG=0;*pMDMA_D0_CONFIG=0;*pMDMA_D0_IRQ_STATUS=3;
 *pMDMA_S0_START_ADDR=(void*)l->address;*pMDMA_D0_START_ADDR=weight[slot];
 *pMDMA_S0_X_COUNT=l->bytes/4;*pMDMA_D0_X_COUNT=l->bytes/4;
 *pMDMA_S0_X_MODIFY=4;*pMDMA_D0_X_MODIFY=4;ssync();
 stats[20+i]=ticks();
 *pMDMA_D0_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S0_CONFIG=WDSIZE_32|DMAEN;ssync();
}
static void start_audio(unsigned i){
 unsigned slot=i&1;
 if(as[slot]!=0){stats[1]++;return;}
 as[slot]=1;aslot=slot;
 *pMDMA_S1_CONFIG=0;*pMDMA_D1_CONFIG=0;*pMDMA_D1_IRQ_STATUS=3;
 *pMDMA_S1_START_ADDR=(void*)(AUDIO+i*320);*pMDMA_D1_START_ADDR=audio[slot];
 *pMDMA_S1_X_COUNT=320;*pMDMA_D1_X_COUNT=320;
 *pMDMA_S1_X_MODIFY=4;*pMDMA_D1_X_MODIFY=4;ssync();
 *pMDMA_D1_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S1_CONFIG=WDSIZE_32|DMAEN;ssync();
}
static int await(volatile unsigned *state,unsigned count_index){
 unsigned t=ticks();
 if(stats[1])return 0;
 while(*state!=2){if(ticks()-t>10000000u || stats[1]){stats[1]++;return 0;}}
 ssync();stats[count_index]+=ticks()-t;*state=3;return 1;
}
#ifdef ACTIVATION_DMA
static int finish_output_dma(void){
 if(output_inflight>=0){
  if(!await(as+output_inflight,63))return 0;
  as[output_inflight]=0;output_inflight=-1;
 }
 return 1;
}
static int submit_output(unsigned slot,signed char *dest,unsigned bytes){
 if(!finish_output_dma())return 0;
 if(as[slot]!=3){stats[1]++;return 0;}
 as[slot]=1;aslot=slot;output_inflight=slot;
 *pMDMA_S1_CONFIG=0;*pMDMA_D1_CONFIG=0;*pMDMA_D1_IRQ_STATUS=3;
 *pMDMA_S1_START_ADDR=audio[slot];*pMDMA_D1_START_ADDR=dest;
 *pMDMA_S1_X_COUNT=bytes/4;*pMDMA_D1_X_COUNT=bytes/4;
 *pMDMA_S1_X_MODIFY=4;*pMDMA_D1_X_MODIFY=4;ssync();
 *pMDMA_D1_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S1_CONFIG=WDSIZE_32|DMAEN;ssync();stats[62]+=bytes;
 return 1;
}
static void layer_output_dma(unsigned layer,const signed char *in,signed char *out){
 const Layer *l=layers+layer;PostArgs q;int add[64],p,c;
 unsigned w=layer&1;
 for(c=0;c<64;c++)add[c]=l->bias[c]*(1<<l->bl)+(1<<(l->rs-1));
 q.sums=sums;q.add=add;q.n=64;q.shift=l->rs;q.lower=0;
 cnn_prepare(l->kind,in,scratch.act);
 /* Prefetch AFTER preparation, so transfer overlaps actual MAC/post work. */
 start_weight(layer+1);
 for(p=0;p<125;p++){
  unsigned slot=(p/TILE_PIXELS)&1;
  if(!(p%TILE_PIXELS)){
   if(as[slot]!=0){stats[1]++;return;}
   as[slot]=3;
  }
  if(l->kind==CNN_REGULAR)cnn_make_patch(in,p,patch);
  cnn_pixel(l->kind,p,scratch.act,weight[w],patch,sums);
  q.out=(signed char*)audio[slot]+(p%TILE_PIXELS)*64;cnn_post(&q);
  if(p%TILE_PIXELS==TILE_PIXELS-1 || p==124){
   if(!submit_output(slot,out+(p/TILE_PIXELS)*TILE_PIXELS*64,(p%TILE_PIXELS+1)*64))return;
  }
 }
 finish_output_dma();
}
#endif
static void finish(void){
 unsigned i;
 for(i=0;i<2;i++){
  if(weight[i][4096]!=(short)0x55aa || weight[i][4097]!=(short)0x33cc)stats[1]++;
  if(audio[i][640]!=(short)0x1234 || audio[i][641]!=(short)0x7777)stats[1]++;
 }
 stats[0]=0x600dcafe;memcpy(EXPORT,(const void*)stats,256);
 memcpy(EXPORT+256,features,490);memcpy(EXPORT+73024,pool,64);memcpy(EXPORT+73088,scores,12);
}
void main(void){
 unsigned i,j,t,total;
 signed char *in=features,*out=ACT0;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 for(i=0;i<2;i++){
  weight[i][4096]=(short)0x55aa;weight[i][4097]=(short)0x33cc;
  audio[i][640]=0x1234;audio[i][641]=0x7777;
 }
 dma_setup();total=ticks();
#ifdef REPLAY_FEATURES
 memcpy(features,(const unsigned char*)AUDIO+32000,490);
#ifdef SERIAL_WEIGHTS
 stats[9]=3;
#else
 stats[9]=2;
#endif
#ifdef ACTIVATION_DMA
 stats[9]=TILE_PIXELS==16?5:4;
#endif
#else
 stats[9]=1;fe32_init(&scratch.mfcc);
 start_audio(0);t=ticks();
 for(i=0;i<49;i++){
  unsigned slot=i&1;
  if(!await(as+slot,5)){finish();return;}
  if(i+1<49)start_audio(i+1);
  fe32_compute(audio[slot],features+i*10,&scratch.mfcc);
  as[slot]=0;
  if(i%10==0)printf("MFCC %u/49\n",i+1);
 }
 stats[6]=ticks()-t;
#endif
 start_weight(0);t=ticks();
#ifdef ACTIVATION_DMA
 output_phase=1;
#endif
 for(i=0;i<9;i++){
  unsigned slot=i&1;const Layer *l=layers+i;
#ifdef SERIAL_WEIGHTS
  if(i)start_weight(i);
#endif
  if(!await(ws+slot,4)){finish();return;}
#if !defined(SERIAL_WEIGHTS) && !defined(ACTIVATION_DMA)
  start_weight(i+1);
#endif
  stats[40+i]=ticks();
#ifdef ACTIVATION_DMA
  layer_output_dma(i,in,out);
#else
  cnn_layer(l->kind,in,weight[slot],l->bias,l->bl,l->rs,1,scratch.act,patch,sums,out);
#endif
  stats[50+i]=ticks();stats[10+i]=stats[50+i]-stats[40+i];
  ws[slot]=0;
  /* Validation journal outside layer timing. Real deployment omits this. */
  memcpy(EXPORT+1024+i*8000,out,8000);
  in=out;out=(out==ACT0?ACT1:ACT0);
  printf("conv %u complete\n",i+1);
 }
 stats[7]=ticks()-t;
 t=ticks();for(j=0;j<8000;j++)scratch.act[j]=in[j];
 cnn_pool(scratch.act,pool);stats[60]=ticks()-t;
 for(j=0;j<64;j++)scratch.act[j]=pool[j];
#ifdef SERIAL_WEIGHTS
 start_weight(9);
#endif
 if(!await(ws+1,4)){finish();return;}
 stats[49]=ticks();
 cnn_fc(scratch.act,weight[1],layers[9].bias,scores,sums,fc_add);
 stats[59]=ticks();stats[19]=stats[59]-stats[49];ws[1]=0;
 stats[8]=ticks()-total;
 if(stats[2]!=10)stats[1]++;
#ifndef REPLAY_FEATURES
 if(stats[3]!=49)stats[1]++;
#endif
 finish();
}
