#include "capture.h"
#include "decimator_coeffs.h"
#include <cdefBF533.h>
#include <ccblkfn.h>
#include <sys/exception.h>
#include <string.h>
/* Same exact Q15 dot ISA kernel already verified for the MFCC filter bank. */
extern int fe_dot(const short*,const short*,int);
CaptureStats capture_stats;
static int *raw_base;
static unsigned selected_slot,last_active,last_irq;
static volatile unsigned owner[8]; /* FREE0,READY1,CPU_HELD2 */
static short delay_line[126];static unsigned delay_pos,phase;
static unsigned clock_hz;
static unsigned clock_epoch;
static volatile unsigned armed,running;
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
static unsigned lock_irq(void){unsigned m;asm volatile("CLI %0;":"=d"(m)::"memory");return m;}
static void unlock_irq(unsigned m){asm volatile("STI %0;"::"d"(m):"memory");}
static unsigned active_row(void){
 unsigned address=(unsigned)*pDMA1_CURR_ADDR;
 return ((address-(unsigned)raw_base)/(CAPTURE_WORDS*4))%CAPTURE_ROWS;
}
void capture_stop(void){
 running=0;
#ifndef BOARD_CAPTURE_TEST
 *pSPORT0_RCR1&=~RSPEN;ssync();*pDMA1_CONFIG=0;ssync();
#endif
}
int capture_clock_ok(unsigned cclk,unsigned elapsed){
 unsigned expected=cclk/50;
 return cclk>=1000000u && elapsed>=expected*9/10 && elapsed<=expected*11/10;
}
unsigned capture_cclk_from_regs(unsigned clkin,unsigned ctl,unsigned div,unsigned stat){
 unsigned m=(ctl&MSEL)>>9;unsigned long long hz;
 if((ctl&BYPASS) || (stat&(ACTIVE_PLLENABLED|ACTIVE_PLLDISABLED)))return clkin;
 if(!(stat&FULL_ON))return 0;
 if(!m)m=64;
 hz=(unsigned long long)clkin*m;
 if(ctl&DF)hz/=2;
 hz>>=(div&CSEL)>>4;
 return hz>750000000u || hz<1000000u?0:(unsigned)hz;
}
unsigned capture_board_cclk(unsigned clkin){return capture_cclk_from_regs(clkin,*pPLL_CTL,*pPLL_DIV,*pPLL_STAT);}
static void fault(unsigned reason){capture_stats.fault=reason;capture_stop();}
int capture_test_reset(int *raw,unsigned slot){
 if(!raw || ((unsigned)raw&3) || slot>=4)return -1;
 raw_base=raw;selected_slot=slot;last_active=0;last_irq=0;
 memset((void*)&capture_stats,0,sizeof(capture_stats));memset((void*)owner,0,sizeof(owner));
 memset(delay_line,0,sizeof(delay_line));delay_pos=phase=0;return 0;
}
void capture_publish_row(unsigned active,unsigned cycles){
 unsigned completed;
 if(capture_stats.fault)return;
 if(running && clock_hz && capture_stats.irqs && cycles-last_irq>(clock_hz/400)*3/2){
  capture_stats.overruns++;fault(CAPTURE_SEQUENCE);return;
 }
 if(active>=8 || active!=(last_active+1)%8){fault(CAPTURE_SEQUENCE);return;}
 completed=(active+7)%8;
 if(completed!=capture_stats.produced%8 || owner[completed]!=0 || owner[active]!=0){
  capture_stats.overruns++;fault(CAPTURE_OVERRUN);return;
 }
 if(capture_stats.irqs)capture_stats.last_period_cycles=cycles-last_irq;
 last_irq=cycles;last_active=active;
 owner[completed]=1;capture_stats.produced++;capture_stats.irqs++;
 if(running && capture_stats.irqs%8==0){
  if(!capture_clock_ok(clock_hz,cycles-clock_epoch)){fault(CAPTURE_CLOCK);return;}
  clock_epoch=cycles;
 }
}
EX_INTERRUPT_HANDLER(capture_rx_isr){
 unsigned status=*pDMA1_IRQ_STATUS;*pDMA1_IRQ_STATUS=status&3;ssync();
 if(status&DMA_ERR){fault(CAPTURE_DMA_ERROR);return;}
 if(status&DMA_DONE)capture_publish_row(active_row(),ticks());
}
short capture_pcm24(int word){
 unsigned u=(unsigned)word&0xffffffu;
 int signed24=(u&0x800000u)?(int)(u|0xff000000u):(int)u;
 return (short)(signed24>>8);
}
int capture_read40(short out[40]){
 unsigned slot,mask,i,n=0;int value;
 if(!out || capture_stats.fault)return -1;
 mask=lock_irq();slot=capture_stats.consumed%8;
 if(owner[slot]!=1){unlock_irq(mask);return 0;}
#ifndef BOARD_CAPTURE_TEST
 if(active_row()==slot){capture_stats.overruns++;fault(CAPTURE_OVERRUN);unlock_irq(mask);return -1;}
#endif
 owner[slot]=2;unlock_irq(mask);
 for(i=0;i<120;i++){
  short x=capture_pcm24(raw_base[slot*CAPTURE_WORDS+i*4+selected_slot]);
  delay_line[delay_pos]=delay_line[delay_pos+63]=x;delay_pos=(delay_pos+1)%63;
  if(++phase==3){
   phase=0;value=fe_dot(delay_line+delay_pos,decimator_h,63);
   if(value>32767)value=32767;if(value< -32768)value=-32768;out[n++]=(short)value;
  }
 }
 mask=lock_irq();
 if(capture_stats.fault){unlock_irq(mask);return -1;}
 owner[slot]=0;capture_stats.consumed++;capture_stats.pcm16_samples+=n;
 unlock_irq(mask);return (int)n;
}
static void delay_cycles(unsigned count){unsigned t=ticks();while(ticks()-t<count){}}
static int codec_init(void){
 /* I2S24,ADC48kHz/0dB,256x MCLK. DACs muted:RX-only bring-up. */
 static const unsigned short commands[11]={0x0000,0x103f,0x23ff,0x33ff,0x43ff,0x53ff,0x63ff,0x73ff,0xc000,0xd000,0xe000};
 volatile unsigned char *direction=(volatile unsigned char*)0x20270006;
 volatile unsigned char *data=(volatile unsigned char*)0x20270004;
 unsigned t;
 *pEBIU_AMBCTL0=0x7bb07bb0;*pEBIU_AMBCTL1=0x7bb07bb0;*pEBIU_AMGCTL=0x000f;ssync();
 *direction|=1;*data&=(unsigned char)~1;ssync();delay_cycles(clock_hz/1000);
 *data|=1;ssync();delay_cycles(clock_hz/333);
 *pSPI_CTL=0;*pDMA5_CONFIG=0;*pDMA5_IRQ_STATUS=3;
 *pSPI_FLG=FLS4;*pSPI_BAUD=64;*pSPI_CTL=3|SIZE|MSTR;
 *pDMA5_PERIPHERAL_MAP=0x5000;*pDMA5_START_ADDR=(void*)commands;
 *pDMA5_X_COUNT=11;*pDMA5_X_MODIFY=2;*pDMA5_CONFIG=WDSIZE_16|DMAEN;
 *pSPI_CTL|=SPE;ssync();t=ticks();
 while(!(*pDMA5_IRQ_STATUS&DMA_DONE)){
  if((*pDMA5_IRQ_STATUS&DMA_ERR) || ticks()-t>clock_hz/10)goto failed;
 }
 while(!(*pSPI_STAT&SPIF) || (*pSPI_STAT&TXS))if(ticks()-t>clock_hz/10)goto failed;
 *pDMA5_CONFIG=0;*pSPI_CTL=0;*pSPI_FLG|=FLG4;ssync();return 0;
failed:
 *pDMA5_CONFIG=0;*pSPI_CTL=0;*pSPI_FLG|=FLG4;ssync();fault(CAPTURE_SPI_TIMEOUT);return -1;
}
int capture_init(unsigned cclk_hz,int *raw,unsigned slot){
 unsigned mask;
 if(cclk_hz<1000000u || cclk_hz>750000000u || !raw || ((unsigned)raw&3) || slot>=4)return -1;
 mask=lock_irq();capture_stop();armed=0;
 *pSIC_IMASK&=~DMA1_IRQ;*pDMA1_IRQ_STATUS=3;ssync();
 capture_test_reset(raw,slot);unlock_irq(mask);
 clock_hz=cclk_hz;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 capture_stop();if(codec_init())return -1;
 /* Reference I2S:external clocks/FS,falling edge,24bits,secondary RX.
  * No SPORT TX or DMA2 required for ADC-generated RX clocks. */
 *pSPORT0_RCR1=RFSR|RCKFE;*pSPORT0_RCR2=23|RXSE|RSFSE;
 *pDMA1_PERIPHERAL_MAP=0x1000;*pDMA1_START_ADDR=raw_base;
 *pDMA1_X_COUNT=CAPTURE_WORDS;*pDMA1_X_MODIFY=4;
 *pDMA1_Y_COUNT=8;*pDMA1_Y_MODIFY=4;
 /* DI_SEL=1 means row/inner-loop interrupt,not whole2D buffer. */
 *pDMA1_CONFIG=WNR|WDSIZE_32|DMA2D|DI_SEL|DI_EN|FLOW_AUTO;
 *pDMA1_IRQ_STATUS=3;
 *pSIC_IAR1=(*pSIC_IAR1&~(15u<<4))|(1u<<4);
 register_handler(ik_ivg8,capture_rx_isr);*pSIC_IMASK|=DMA1_IRQ;armed=1;ssync();return 0;
}
void capture_start(void){
 if(running)return;
 if(capture_stats.fault || !raw_base || !armed)return;
 armed=0;running=1;clock_epoch=ticks();last_irq=clock_epoch;
 *pDMA1_CONFIG|=DMAEN;ssync();*pSPORT0_RCR1|=RSPEN;ssync();
}
