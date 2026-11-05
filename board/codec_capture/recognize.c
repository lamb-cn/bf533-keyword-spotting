/* Included by the streaming target to share the native MFCC and CNN path. */
#include "capture.h"

#define BOARD_RAW ((int *)0x01600000)
#define BOARD_BLOCKS 1750u
#define BOARD_EVENT_LIMIT 32u
#define LED_DATA (*(volatile unsigned char *)0x20270005u)
#define LED_DIR (*(volatile unsigned char *)0x20270007u)
#define LED_MASK 0x3fu

/* yes, no, up, down, left, right, on, off, stop, go */
static const unsigned char led_patterns[12]={0,0,1,2,4,8,16,32,63,21,42,33};

static void led_write(unsigned pattern){
 LED_DATA=(LED_DATA & ~LED_MASK) | pattern;
 ssync();
}

struct {
 unsigned complete, blocks, inferences, events, fault, dma_errors;
 unsigned rows, overruns, peak, max_job_cycles, cclk, voice_frames;
 StreamEvent event[BOARD_EVENT_LIMIT];
 unsigned led_readback[BOARD_EVENT_LIMIT], quiet_ms[BOARD_EVENT_LIMIT], led_cycles[BOARD_EVENT_LIMIT];
} board_result;

void main(void){
 unsigned block, fill=0, i, slot, time_ms, start, idle, elapsed;
 unsigned hz=capture_board_cclk(27000000u);
 int n, dc=0;
 short row[40];

 board_result.cclk=hz;
 if(capture_init(hz,BOARD_RAW,2)){board_result.fault=100;return;}
 led_write(0);
 LED_DIR|=LED_MASK;
 ssync();
 dma_setup();
 fe32_init(&scratch.mfcc);
 stream_copy(TWIDDLE_BACKUP,scratch.mfcc.twiddle,4096);
 twiddle_hash=stream_hash(TWIDDLE_BACKUP,4096);
 memset(audio[0],0,1280);
 fe32_compute(audio[0],next_feature,&scratch.mfcc);
 stream_init(&core,next_feature);
 capture_start();
 idle=ticks();
#ifdef BOARD_CONTINUOUS
 for(block=0;;){
#else
 for(block=0;block<BOARD_BLOCKS;){
#endif
  slot=block&1;
  n=capture_read40(row);
  if(n<0)break;
  if(!n){
   if(ticks()-idle>hz/4){board_result.fault=101;break;}
   continue;
  }
  idle=ticks();
  for(i=0;i<(unsigned)n;i++){
   /* Remove the codec DC offset before applying the existing RMS gate. */
   int x=row[i], y;
   unsigned magnitude;
   dc+=(x*256-dc)/128;
   y=x-dc/256;
   if(y>32767)y=32767;
   if(y< -32768)y=-32768;
   magnitude=y<0?(unsigned)-y:(unsigned)y;
   if(magnitude>board_result.peak)board_result.peak=magnitude;
   audio[slot][320+fill++]=(short)y;
  }
  if(fill!=320)continue;
  start=ticks();fill=0;time_ms=(block+1)*20;
  if(block)stream_copy(audio[slot],audio[slot^1]+320,640);
  if(!check_twiddle()){board_result.fault=102;break;}
  i=stream_voice(audio[slot]+320);
  board_result.voice_frames+=i;
  stream_on_audio(&core,time_ms,i);
  fe32_compute(audio[slot],next_feature,&scratch.mfcc);
  stream_push(&core,next_feature);
  if(block%5==4){
   StreamEvent event;
   stream_snapshot(&core,features);
   if(!infer_native()){board_result.fault=103;break;}
   board_result.inferences++;
   if(stream_on_scores(&core,time_ms,scores,&event)){
#ifndef BOARD_CONTINUOUS
    if(board_result.events==BOARD_EVENT_LIMIT){board_result.fault=104;break;}
#endif
    {
     unsigned index=board_result.events%BOARD_EVENT_LIMIT, led_start=ticks();
     led_write(led_patterns[event.label]);
     board_result.led_cycles[index]=ticks()-led_start;
     board_result.led_readback[index]=LED_DATA & LED_MASK;
     board_result.quiet_ms[index]=time_ms-core.last_voice_ms;
     board_result.event[index]=event;
     board_result.events++;
#ifdef BOARD_CONTINUOUS
     {
      static const char *const names[12]={
       "silence","unknown","yes","no","up","down","left","right","on","off","stop","go"
      };
      /* Debug stdout halts the core while the host handles the write. */
      capture_stop();
      printf("%u ms  %s  LED=0x%02X\n",event.time_ms,names[event.label],
             (unsigned)led_patterns[event.label]);
      fflush(stdout);
      if(capture_init(hz,BOARD_RAW,2)){board_result.fault=105;break;}
      capture_start();
      idle=ticks();
     }
#endif
    }
   }
   restore_twiddle();
  }
  board_result.blocks=++block;
  elapsed=ticks()-start;
  if(elapsed>board_result.max_job_cycles)board_result.max_job_cycles=elapsed;
 }
 capture_stop();
 if(!board_result.fault)board_result.fault=capture_stats.fault;
 if(!check_twiddle() && !board_result.fault)board_result.fault=102;
 board_result.dma_errors=stats[1];
 board_result.rows=capture_stats.consumed;
 board_result.overruns=capture_stats.overruns;
 board_result.complete=0x600dcafeu;
}
