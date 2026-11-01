#define STREAMING 1
#define main pipeline_unused_main
#include "../end_to_end/pipeline.c"
#undef main
#include "stream_core.h"
#include "stream_config.h"
#define STREAM_AUDIO ((const short*)0x01020000)
#define STREAM_OUT ((unsigned char*)0x01400000)
#define TWIDDLE_BACKUP ((unsigned char*)0x01500000)
#define FRAME_DATA 2048
#define FRAME_INFO 16384
#define INFER_INFO 65536
#define SNAPSHOT_DATA 81920
/* Header[0]:magic,[1]:errors,[2]:mode(1 native/0 control),[3]:blocks,
 * [4]:MFCC updates,[5]:inference requests,[6]:native network calls,[7]:events,
 * [8:9]:busy cycles uint64,[10]:max job cycles,[11]:max NN,[12]:deadline misses,
 * [13]:max feature cycles,[14]:audio IRQ,[15]:weight IRQ,[16]:twiddle IRQ,
 * [17]:twiddle init count,[18]:dropped/order errors,[19]:ring corruption,
 * [20]:guards,[21]:FFT frame calls,[22]:silence shortcuts,[23]:MFCC during NN,
 * [24]:twiddle CRC errors,[25]:clock Hz,[26]:frame budget cycles,[27]:duration ms,
 * [28]:event overflow,[29]:feature calls,[30]:calibration calls,[31]:ready.
 */
unsigned header[64];
StreamCore core;
signed char next_feature[10];
volatile unsigned stream_twiddle_active,twiddle_ready;
volatile unsigned stream_fe32_calls;
static unsigned twiddle_pending,twiddle_hash;
static void flush_trace(void){
#ifdef STREAM_PROGRESS_FLUSH
 unsigned k;
 /* CCES_runner buffers host stdout even after DSP fflush. Force one pipe
  * buffer out so an hours-long instruction-level run has observable progress.
  * This is diagnostic traffic for accelerated simulation, not a UART driver. */
 for(k=0;k<80;k++)fputs("................................................................\n",stdout);
#endif
 fflush(stdout);
}
void stream_twiddle_isr(unsigned s){
 if(s&DMA_ERR)stats[1]++;
 if(s&DMA_DONE){twiddle_ready=1;stream_twiddle_active=0;header[16]++;}
}
static void fail(unsigned field){header[field]++;header[1]++;}
static void start_block(unsigned block){
 unsigned slot=block&1;
 if(as[slot]){fail(18);return;}
 as[slot]=1;aslot=slot;
 *pMDMA_S1_CONFIG=0;*pMDMA_D1_CONFIG=0;*pMDMA_D1_IRQ_STATUS=3;
 *pMDMA_S1_START_ADDR=(void*)(STREAM_AUDIO+block*320);
 *pMDMA_D1_START_ADDR=audio[slot]+320;
 *pMDMA_S1_X_COUNT=160;*pMDMA_D1_X_COUNT=160;
 *pMDMA_S1_X_MODIFY=4;*pMDMA_D1_X_MODIFY=4;ssync();
 *pMDMA_D1_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S1_CONFIG=WDSIZE_32|DMAEN;ssync();
}
static void restore_twiddle(void){
 stream_twiddle_active=1;twiddle_ready=0;twiddle_pending=1;
 *pMDMA_S0_CONFIG=0;*pMDMA_D0_CONFIG=0;*pMDMA_D0_IRQ_STATUS=3;
 *pMDMA_S0_START_ADDR=TWIDDLE_BACKUP;
 *pMDMA_D0_START_ADDR=scratch.mfcc.twiddle;
 *pMDMA_S0_X_COUNT=1024;*pMDMA_D0_X_COUNT=1024;
 *pMDMA_S0_X_MODIFY=4;*pMDMA_D0_X_MODIFY=4;ssync();
 *pMDMA_D0_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S0_CONFIG=WDSIZE_32|DMAEN;ssync();
}
static int check_twiddle(void){
 unsigned t=ticks();
 if(!twiddle_pending)return 1;
 while(!twiddle_ready){if(stats[1] || ticks()-t>10000000u){fail(24);return 0;}}
 ssync();
 if(stream_hash(scratch.mfcc.twiddle,4096)!=twiddle_hash){fail(24);return 0;}
 header[33]++;
 twiddle_pending=0;return 1;
}
static int all_zero(const short *x){
 unsigned i,any=0;for(i=0;i<640;i++)any|=(unsigned short)x[i];return any==0;
}
static int infer_native(void){
 unsigned i,j;const signed char *in=features;signed char *out=ACT0;
 if(twiddle_pending && !check_twiddle())return 0;
 start_weight(0);
 for(i=0;i<9;i++){
  const Layer *l=layers+i;unsigned slot=i&1;
  if(!await(ws+slot,4))return 0;
  start_weight(i+1);
  cnn_layer(l->kind,in,weight[slot],l->bias,l->bl,l->rs,1,scratch.act,patch,sums,out);
  ws[slot]=0;in=out;out=out==ACT0?ACT1:ACT0;
 }
 for(j=0;j<8000;j++)scratch.act[j]=in[j];
 cnn_pool(scratch.act,pool);
 for(j=0;j<64;j++)scratch.act[j]=pool[j];
 if(!await(ws+1,4))return 0;
 cnn_fc(scratch.act,weight[1],layers[9].bias,scores,sums,fc_add);ws[1]=0;
 header[6]++;return 1;
}
static void write_header(int final){
 unsigned i;
 header[14]=stats[3];header[15]=stats[2];
 header[32]=stream_fe32_calls;
 for(i=0;i<2;i++){
  if(weight[i][4096]!=(short)0x55aa || weight[i][4097]!=(short)0x33cc || audio[i][640]!=0x1234 || audio[i][641]!=0x7777){fail(20);break;}
 }
 header[1]+=stats[1];stats[1]=0;
 if(final)header[0]=0x600dcafe;
 stream_copy(STREAM_OUT,header,sizeof(header));
}
#ifndef STREAM_SWITCH_PROBE
void main(void){
 unsigned block,slot,i,start,dt,time_ms,feature_cycles,before,after,old_head,nn_before,nn_start,nn_cycles,infer_idx=0;
 unsigned frame_record[8],infer_record[16];StreamEvent event;
 unsigned long long busy=0;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
#ifdef STREAM_REPLAY
 header[2]=0;
#else
 header[2]=1;
#endif
 header[25]=STREAM_CLOCK_HZ;header[26]=STREAM_CLOCK_HZ/50;header[27]=STREAM_BLOCKS*20;
#ifdef STREAM_EMBEDDED
 {extern const unsigned stream_image[];header[34]=(unsigned)stream_image;}
#endif
 for(i=0;i<2;i++){
  weight[i][4096]=(short)0x55aa;weight[i][4097]=(short)0x33cc;
  audio[i][640]=0x1234;audio[i][641]=0x7777;
 }
 dma_setup();
#ifdef STREAM_REPLAY
 {signed char silence[10]={-128,0,0,0,0,0,0,0,0,0};stream_init(&core,silence);}
#else
 fe32_init(&scratch.mfcc);header[17]=1;
 stream_copy(TWIDDLE_BACKUP,scratch.mfcc.twiddle,4096);
 twiddle_hash=stream_hash(TWIDDLE_BACKUP,4096);
 memset(audio[0],0,1280);fe32_compute(audio[0],next_feature,&scratch.mfcc);header[30]=1;
 stream_init(&core,next_feature);
#endif
 header[31]=1;
 stream_copy(STREAM_OUT+1280,core.silence,10);
 printf("STREAM ready mode=%u blocks=%u ticks=%u\n",header[2],STREAM_BLOCKS,STREAM_INFERENCES);flush_trace();
 for(block=0;block<STREAM_BLOCKS;block++){
  start=ticks();slot=block&1;time_ms=(block+1)*20;
  start_block(block);
  /* Only new320 samples are DMA-read. Retain prior320 before reusing their slot. */
  if(block)stream_copy(audio[slot],audio[slot^1]+320,640);else memset(audio[slot],0,640);
  if(!await(as+slot,5)){header[1]++;break;}
  stream_on_audio(&core,time_ms,stream_voice(audio[slot]+320));
  nn_start=ticks();
#ifdef STREAM_REPLAY
  stream_copy(next_feature,(const signed char*)0x01100000+block*10,10);
#else
  if(!check_twiddle())break;
  if(all_zero(audio[slot])){stream_copy(next_feature,core.silence,10);header[22]++;}
  else {fe32_compute(audio[slot],next_feature,&scratch.mfcc);header[21]++;}
#endif
  feature_cycles=ticks()-nn_start;header[29]++;
  if(feature_cycles>header[13])header[13]=feature_cycles;
  old_head=core.head;before=stream_kept_hash(&core);
  stream_push(&core,next_feature);
  /* Hash retained physical48 rows with the just-replaced row excluded. */
  after=core.head;core.head=old_head;after=stream_kept_hash(&core);core.head=(old_head+1)%49;
  if(before!=after)fail(19);
  stream_copy(STREAM_OUT+FRAME_DATA+block*10,next_feature,10);
  header[4]=core.updates;
  frame_record[0]=block;frame_record[1]=time_ms;frame_record[2]=core.head;frame_record[3]=core.updates;
  frame_record[4]=before;frame_record[5]=after;frame_record[6]=feature_cycles;frame_record[7]=core.active;
  stream_copy(STREAM_OUT+FRAME_INFO+block*32,frame_record,32);
  if(time_ms%100==0){
   stream_snapshot(&core,features);
   stream_copy(STREAM_OUT+SNAPSHOT_DATA+infer_idx*490,features,490);
   before=stream_hash(core.ring,490);nn_before=stream_fe32_calls;nn_start=ticks();
#ifdef STREAM_REPLAY
   stream_copy(scores,(const signed char*)0x01110000+infer_idx*12,12);
#else
   if(!infer_native()){header[1]++;break;}
#endif
   nn_cycles=ticks()-nn_start;
   if(nn_cycles>header[11])header[11]=nn_cycles;
   if(nn_before!=stream_fe32_calls)fail(23);
   if(before!=stream_hash(core.ring,490))fail(19);
   memset(infer_record,0,sizeof(infer_record));
   infer_record[0]=time_ms;infer_record[1]=core.updates;infer_record[2]=core.head;infer_record[3]=nn_cycles;
   stream_copy((unsigned char*)infer_record+16,scores,12);
   if(stream_on_scores(&core,time_ms,scores,&event)){
    if(header[7]>=64)fail(28);
    else {stream_copy(STREAM_OUT+256+header[7]*16,&event,16);header[7]++;}
    printf("EVENT t=%u label=%u votes=%u peak=%d\n",event.time_ms,event.label,event.votes,event.peak);flush_trace();
    infer_record[9]=event.label;
   }
   infer_record[12]=stream_hash(features,490);infer_record[15]=infer_idx;
   stream_copy(STREAM_OUT+INFER_INFO+infer_idx*64,infer_record,64);
   infer_idx++;header[5]=infer_idx;
#ifndef STREAM_REPLAY
   if(block+1<STREAM_BLOCKS)restore_twiddle();
#endif
  }
  as[slot]=0;header[3]=block+1;
  if(time_ms%1000==0){
   printf("PROGRESS t=%u frames=%u infer=%u native=%u events=%u errors=%u maxjob=%u\n",time_ms,core.updates,header[5],header[6],header[7],header[1],header[10]);flush_trace();
   write_header(0);
  }
  dt=ticks()-start;busy+=dt;
  if(dt>header[10])header[10]=dt;
  if(dt>header[26])fail(12);
  header[8]=(unsigned)busy;header[9]=(unsigned)(busy>>32);
  if(header[1])break;
 }
 if(core.updates!=STREAM_BLOCKS || infer_idx!=STREAM_INFERENCES)header[1]++;
 if(twiddle_pending && !check_twiddle())header[1]++;
#ifndef STREAM_REPLAY
 if(stream_fe32_calls!=header[21]+header[30])header[1]++;
#endif
 *pMDMA_S0_CONFIG=0;*pMDMA_D0_CONFIG=0;*pMDMA_S1_CONFIG=0;*pMDMA_D1_CONFIG=0;ssync();
 write_header(1);
 printf("STREAM complete mode=%u frames=%u infer=%u native=%u events=%u errors=%u\n",header[2],core.updates,header[5],header[6],header[7],header[1]);fflush(stdout);
}
#else
void main(void){
 signed char before[10];unsigned i;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 header[2]=2;
 for(i=0;i<2;i++){
  weight[i][4096]=(short)0x55aa;weight[i][4097]=(short)0x33cc;
  audio[i][640]=0x1234;audio[i][641]=0x7777;
 }
 dma_setup();fe32_init(&scratch.mfcc);
 stream_copy(TWIDDLE_BACKUP,scratch.mfcc.twiddle,4096);twiddle_hash=stream_hash(TWIDDLE_BACKUP,4096);
 start_block(62);stream_copy(audio[0],STREAM_AUDIO+61*320,640);
 if(!await(as,5)){header[1]++;write_header(1);return;}
 printf("SWITCH before MFCC\n");flush_trace();
 fe32_compute(audio[0],before,&scratch.mfcc);
 stream_init(&core,before);stream_snapshot(&core,features);
 printf("SWITCH before NN\n");flush_trace();
 if(!infer_native()){header[1]++;write_header(1);return;}
 printf("SWITCH after NN\n");flush_trace();
 restore_twiddle();
 if(!check_twiddle()){write_header(1);return;}
 printf("SWITCH table CRC restored\n");flush_trace();
 fe32_compute(audio[0],next_feature,&scratch.mfcc);
 for(i=0;i<10;i++)if(before[i]!=next_feature[i])header[1]++;
 write_header(1);
 printf("SWITCH complete errors=%u\n",header[1]);flush_trace();
}
#endif
