#include "mds_def.h"
#include "filter.h"
/* Keep the supplied FIR arithmetic and assembly unchanged. Compare original
 * default placement with delay in A, coefficients in B (CNN-style banking). */
#ifdef FIR_SPLIT
#pragma section("L1_data_b")
#endif
#include "fir_coeff.h"
#include "fir_input.h"
#ifdef FIR_SPLIT
#pragma section("L1_data_a")
#endif
fract16 delay[BASE_TAPLENGTH];
typedef struct {fract16 out[260];unsigned m[8];} FirResult;
volatile FirResult fir_result;
static unsigned ticks(void){unsigned t;asm volatile("%0 = CYCLES;":"=d"(t)::"memory");return t;}
void main(void){
 fir_state_fr16 state;
 unsigned t0,t1,t2,t3;
 fir_init(state,h,delay,BASE_TAPLENGTH);
 asm volatile("R0 = SYSCFG; BITSET(R0, 1); SYSCFG = R0;" : : : "R0");
 t0=ticks();t1=ticks();t2=ticks();
 _fir(IN,(fract16 *)fir_result.out,BUFFER_SIZE,&state);
 t3=ticks();
 fir_result.m[0]=t0;fir_result.m[1]=t1;fir_result.m[2]=t2;fir_result.m[3]=t3;
 fir_result.m[4]=BUFFER_SIZE;fir_result.m[5]=BASE_TAPLENGTH;
 fir_result.m[6]=0x600dcafe;
#ifdef FIR_SPLIT
 fir_result.m[7]=1;
#else
 fir_result.m[7]=0;
#endif
}
