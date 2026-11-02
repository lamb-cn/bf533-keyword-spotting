#include "capture.h"
/* Standalone bring-up uses L1. For integration with CNN,put this15KB raw
 * ring in BSP-initialized uncached SDRAM;it does not fit alongside NN L1. */
#pragma section("L1_data_b")
int capture_raw[8*CAPTURE_WORDS];
short pcm_20ms[320],row[40];
volatile unsigned blocks_20ms,init_failed,measured_cclk_hz;
void main(void){
 unsigned fill=0,i;int n;
 /* Standard EZ-KIT CLKIN=27MHz; codec MCLK is a separate12.288MHz clock. */
 measured_cclk_hz=capture_board_cclk(27000000u);
 if(capture_init(measured_cclk_hz,capture_raw,0)){init_failed=1;return;}
 capture_start();
 while(!capture_stats.fault){
  n=capture_read40(row);if(n<0)break;
  for(i=0;i<(unsigned)n;i++)pcm_20ms[fill++]=row[i];
  if(fill==320){
   /* Debugger watch/plot pcm_20ms here. Integration calls the existing
    * incremental MFCC/ring update ONCE for this new20ms block. */
   blocks_20ms++;fill=0;
  }
 }
 capture_stop();
}
