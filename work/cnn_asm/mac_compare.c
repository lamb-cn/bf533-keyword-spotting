#include "cnn.h"
#pragma section("L1_data_a")
short mx[64];
#pragma section("L1_data_b")
short mw[2080*2];
int my[260];
typedef struct{unsigned k,outputs,macs,call_cycles,empty_cycles,net_cycles,errors;} MacRecord;
struct {MacRecord rows[3];unsigned done;} mac_results;
static unsigned ticks(void){unsigned t;asm volatile("%0 = CYCLES;":"=d"(t)::"memory");return t;}
void main(void){
 int test,i,p,k,expected;unsigned t0,t1,t2,t3;
 MatArgs a;MacRecord *r;
 asm volatile("R0 = SYSCFG; BITSET(R0, 1); SYSCFG = R0;" : : : "R0");
 for(test=0;test<3;test++){
  k=test==0?8:(test==1?40:64);
  a.x=mx;a.w=mw;a.out=my;a.k=k;a.pairs=test==0?130:(test==1?26:16);
  for(i=0;i<64;i++)mx[i]=(short)((i*37)%256-128);
  for(i=0;i<2*k*a.pairs;i++)mw[i]=(short)((i*73)%256-128);
  t0=ticks();t1=ticks();t2=ticks();cnn_matvec(&a);t3=ticks();
  r=&mac_results.rows[test];r->k=k;r->outputs=a.pairs*2;r->macs=k*a.pairs*2;
  r->call_cycles=t3-t2;r->empty_cycles=t1-t0;r->net_cycles=r->call_cycles-r->empty_cycles;
  r->errors=0;
  for(p=0;p<a.pairs*2;p++){
   expected=0;for(i=0;i<k;i++)expected+=(int)mx[i]*mw[((p/2)*k+i)*2+(p%2)];
   if(my[p]!=expected)r->errors++;
  }
 }
 mac_results.done=0x600dcafe;
}
