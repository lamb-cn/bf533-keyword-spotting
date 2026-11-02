#ifndef BOARD_CAPTURE_H
#define BOARD_CAPTURE_H
#define CAPTURE_RATE 48000u
#define CAPTURE_ROWS 8u
#define CAPTURE_FRAMES 120u
#define CAPTURE_SLOTS 4u
#define CAPTURE_WORDS (CAPTURE_FRAMES*CAPTURE_SLOTS)
enum {CAPTURE_OK=0,CAPTURE_SPI_TIMEOUT=1,CAPTURE_DMA_ERROR=2,CAPTURE_OVERRUN=3,CAPTURE_SEQUENCE=4,CAPTURE_ARGUMENT=5,CAPTURE_CLOCK=6};
typedef struct {volatile unsigned produced,consumed,irqs,overruns,fault,pcm16_samples,last_period_cycles;} CaptureStats;
extern CaptureStats capture_stats;
/* Standard BF533 EZ-KIT AD1836-family,12.288MHz MCLK,default48kHz I2S.
 * raw:8*480 DWORDs,4-byte aligned DMA-accessible uncached SRAM/initialized
 * SDRAM,non-overlapping with output. Never allocate this15KB on the stack.
 * cclk_hz must match actual board clock; initialization does NOT change PLL.
 * slot follows reference ordering:0=L0,1=L1,2=R0,3=R1.
 */
int capture_init(unsigned cclk_hz,int *raw,unsigned slot);
void capture_start(void);
void capture_stop(void);
/* Main-loop only. 0:no complete row;40:40 new16kHz PCM16 samples;
 * -1:fault,discard output and reinitialize. Never call DSP processing in ISR.
 * Once faulted,capture stops rather than silently overwriting live buffers.
 */
int capture_read40(short out[40]);
/* Pure geometry helper shared by IRQ and simulator tests. active is the DMA
 * row about to be written,next after the just-completed row. */
void capture_publish_row(unsigned active,unsigned cycles);
int capture_test_reset(int *raw,unsigned slot);
short capture_pcm24(int word);
/*8-row window=20ms at48kHz. Pure helper used by ISR and tests. */
int capture_clock_ok(unsigned cclk_hz,unsigned elapsed_cycles);
unsigned capture_cclk_from_regs(unsigned clkin_hz,unsigned pll_ctl,unsigned pll_div,unsigned pll_stat);
unsigned capture_board_cclk(unsigned clkin_hz);
#endif
