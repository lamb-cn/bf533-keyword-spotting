#include "mds_def.h"
#include "filter.h"
#include "fir_coeff.h"
#include "fir_input.h"
#include <sysreg.h>

fract16 delay[BASE_TAPLENGTH];
fract16 OUT[BUFFER_SIZE];
volatile unsigned int metrics[5];
void main(void) {
 fir_state_fr16 s;
 unsigned int t0,t1,t2,t3;
 fir_init(s,h,delay,BASE_TAPLENGTH);
 asm volatile("R0 = SYSCFG; BITSET(R0, 1); SYSCFG = R0;" : : : "R0");
 asm volatile("%0 = CYCLES;" : "=d"(t0));
 asm volatile("%0 = CYCLES;" : "=d"(t1));
 asm volatile("%0 = CYCLES;" : "=d"(t2));
 _fir(IN,OUT,BUFFER_SIZE,&s);
 asm volatile("%0 = CYCLES;" : "=d"(t3));
 metrics[0]=t0;metrics[1]=t1;metrics[2]=t2;metrics[3]=t3;
 metrics[4]=0x12345678;
}


