#include <cdefBF533.h>
#include <ccblkfn.h>
#include <sys/exception.h>
#include <sysreg.h>
int source[64],dest[64];
volatile unsigned result[12];
static unsigned ticks(void){unsigned t;asm volatile("%0=CYCLES;":"=d"(t)::"memory");return t;}
EX_INTERRUPT_HANDLER(dma_isr){
 unsigned s=*pMDMA_D1_IRQ_STATUS;
 *pMDMA_D1_IRQ_STATUS=s&3;
 result[2]++;result[3]=s;
}
void main(void){
 int i;unsigned t;
 asm volatile("R0=SYSCFG;BITSET(R0,1);SYSCFG=R0;":::"R0");
 for(i=0;i<64;i++){source[i]=i*123+7;dest[i]=-1;}
 *pSIC_IAR2=(*pSIC_IAR2&~(15u<<24))|(2u<<24);
 register_handler(ik_ivg9,dma_isr);
 *pSIC_IMASK|=MDMA1_IRQ;
 *pMDMA_S1_CONFIG=0;*pMDMA_D1_CONFIG=0;
 *pMDMA_D1_IRQ_STATUS=3;
 *pMDMA_S1_START_ADDR=(void*)0x01010000;*pMDMA_D1_START_ADDR=dest;
 *pMDMA_S1_X_COUNT=64;*pMDMA_D1_X_COUNT=64;
 *pMDMA_S1_X_MODIFY=4;*pMDMA_D1_X_MODIFY=4;
 ssync();
 *pMDMA_D1_CONFIG=WNR|WDSIZE_32|DI_EN|DMAEN;
 *pMDMA_S1_CONFIG=WDSIZE_32|DMAEN;
 ssync();t=ticks();
 while(!result[2] && ticks()-t<1000000u){}
 result[4]=ticks()-t;
 for(i=0;i<64;i++)if(((const int*)0x01010000)[i]!=dest[i])result[1]++;
 result[5]=*pMDMA_D1_IRQ_STATUS;
 result[0]=0x600dcafe;
}


