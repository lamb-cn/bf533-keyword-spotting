#include "cnn_ops.h"
#include "tail_ops.h"
#include <string.h>
#include <stdio.h>
#ifndef BENCH_CASES
#define BENCH_CASES 15
#endif

/* Keep activation and weight streams in distinct L1 SRAM banks. */
#pragma section("L1_data_a")
short activation[27*7*64];
#pragma section("L1_data_b")
short packed[64*64];
#pragma section("L1_data_b")
signed char input[8000];
#pragma section("L1_data_b")
signed char weights[4096];
#pragma section("L1_data_b")
signed char output[8000];
#pragma section("L1_data_a")
#pragma align 4
short patch[64];
int raw[66], ref[64];
signed char bias[64];

/* Record layout consumed by run.ps1; all fields uint32. */
typedef struct {
 unsigned type, mode, bl, rs, macs, c_cycles, weight_cycles;
 unsigned prep_cycles, core_cycles, post_cycles, layer_cycles;
 unsigned raw_errors, final_errors, guard_errors;
} Record;
volatile Record results[15];
volatile unsigned done;
static unsigned rng=0x12345678;
static unsigned ticks(void) { unsigned t; asm volatile("%0 = CYCLES;" : "=d"(t) : : "memory"); return t; }
static int value(int mode) {
 rng=rng*1664525u+1013904223u;
 if(mode==1) return 0;
 if(mode==2) return (rng&0x10000)?-128:127;
 return (int)(rng>>24)-128;
}
#define quantize cnn_quantize
static int current_case;
/* Independent scalar HWC reference. No padded/packed buffers or asm helpers. */
static void oracle(int type,int pixel) {
#ifdef TARGET_ORACLE
 int c,k,y,x,iy,ix,sum,oy=pixel/5,ox=pixel%5;
 for(c=0;c<64;c++) {
  sum=0;
  if(type==2) {
   for(k=0;k<64;k++) sum+=(int)input[pixel*64+k]*weights[c*64+k];
  } else {
   for(y=0;y<(type==0?10:3);y++) for(x=0;x<(type==0?4:3);x++) {
    iy=oy*(type==0?2:1)+y-(type==0?4:1);
    ix=ox*(type==0?2:1)+x-1;
    if(iy>=0 && iy<(type==0?49:25) && ix>=0 && ix<(type==0?10:5)) {
     if(type==0)sum+=(int)input[iy*10+ix]*weights[c*40+y*4+x];
     else sum+=(int)input[(iy*5+ix)*64+c]*weights[(y*3+x)*64+c];
    }
   }
  }
  ref[c]=sum;
 }
#else
 const int *expected=(const int *)0x01000000;
 memcpy(ref,expected+current_case*8000+pixel*64,64*sizeof(int));
#endif
}
static void pack_weights(int type){cnn_pack_weights(type,weights,packed);}
static void prepare(int type){cnn_prepare(type,input,activation);}
static void make_patch(int pixel){cnn_make_patch(input,pixel,patch);}
void cnn_matvec_c(const MatArgs *a);
void cnn_dw3_c(const DwArgs *a);
static void invoke_c(int type,int pixel) {
 MatArgs m;DwArgs d;
 if(type==1) {
  d.x=activation+((pixel/5)*7+pixel%5)*64;
  d.w=packed;d.out=raw;d.channels=64;d.row_bytes=7*64*2;
  cnn_dw3_c(&d);
 } else {
  m.x=type==0?patch:activation+pixel*64;
  m.w=packed;m.out=raw;m.k=type==0?40:64;m.pairs=32;
  cnn_matvec_c(&m);
 }
}
static void invoke(int type,int pixel){cnn_pixel(type,pixel,activation,packed,patch,raw);}
static void layer(int type,int bl,int rs,int relu){
 cnn_layer(type,input,packed,bias,bl,rs,relu,activation,patch,raw,output);
}
static void run(int index,int type,int mode,int bl,int rs) {
 int i,p,c,relu=mode!=2;
 int post_add[64];PostArgs post_args;
 current_case=index;
 unsigned t,s,prep=0,core=0,post=0,base=0,err=0,ferr=0;
 volatile Record *r=&results[index];
 for(i=0;i<8000;i++)input[i]=(signed char)value(mode);
 for(i=0;i<4096;i++)weights[i]=(signed char)value(mode);
 for(i=0;i<64;i++)bias[i]=(signed char)value(0);
 for(i=0;i<64;i++)post_add[i]=bias[i]*(1<<bl)+(rs?(1<<(rs-1)):0);
 post_args.sums=raw;post_args.add=post_add;post_args.n=64;
 post_args.shift=rs;post_args.lower=relu?0:-128;
 raw[64]=0x12345678;raw[65]=0x76543210;
 r->type=type;r->mode=mode;r->bl=bl;r->rs=rs;
 r->macs=125*64*(type==0?40:(type==1?9:64));
 t=ticks();pack_weights(type);r->weight_cycles=ticks()-t;
 t=ticks();prepare(type);prep=ticks()-t;
 for(p=0;p<125;p++) {
  oracle(type,p);
  if(type==0) {t=ticks();make_patch(p);prep+=ticks()-t;}
  /* Poison outputs so an empty/short kernel fails, including zero tensors. */
  for(c=0;c<64;c++)raw[c]=0x5a5a5a5a;
  t=ticks();invoke(type,p);core+=ticks()-t;
  for(c=0;c<64;c++)if(raw[c]!=ref[c])err++;
  post_args.out=output+p*64;
  t=ticks();cnn_post(&post_args);
  post+=ticks()-t;
  for(c=0;c<64;c++) {
#ifdef TARGET_ORACLE
   int n=ref[c]+bias[c]*(1<<bl)+(rs?(1<<(rs-1)):0),q;
   q=n>=0?n/(1<<rs):-((-n+((1<<rs)-1))/(1<<rs));
   if(q>127)q=127;if(q< -128)q=-128;if(relu && q<0)q=0;
#else
   int q=((const signed char *)0x01075300)[index*8000+p*64+c];
#endif
   if(output[p*64+c]!=q)ferr++;
  }
 }
 r->c_cycles=base;r->prep_cycles=prep;r->core_cycles=core;r->post_cycles=post;
 r->raw_errors=err;r->final_errors=ferr;
 r->guard_errors=(raw[64]!=0x12345678 || raw[65]!=0x76543210);
 t=ticks();layer(type,bl,rs,relu);s=ticks();r->layer_cycles=s-t;
 r->guard_errors+=(raw[64]!=0x12345678 || raw[65]!=0x76543210);
 /* Check the actual timed full layer as well. */
 for(p=0;p<125;p++) {
#ifdef TARGET_ORACLE
  oracle(type,p);
#endif
  for(c=0;c<64;c++) {
#ifdef TARGET_ORACLE
  if(output[p*64+c]!=quantize(ref[c],bias[c],bl,rs,relu))r->final_errors++;
#else
  if(output[p*64+c]!=((const signed char *)0x01075300)[index*8000+p*64+c])r->final_errors++;
#endif
 }}
 printf("case %d done: type=%d raw_errors=%u final_errors=%u core=%u layer=%u\n",
        index,type,r->raw_errors,r->final_errors,r->core_cycles,r->layer_cycles);
}
void main(void) {
 int i;
 asm volatile("R0 = SYSCFG; BITSET(R0, 1); SYSCFG = R0;" : : : "R0");
#ifdef RED_SMOKE
 {
 MatArgs m;DwArgs d;
 patch[0]=-128;patch[1]=127;
 packed[0]=-128;packed[1]=127;packed[2]=127;packed[3]=-128;
 raw[0]=raw[1]=0;
 m.x=patch;m.w=packed;m.out=raw;m.k=2;m.pairs=1;
 cnn_matvec(&m);
 results[0].raw_errors=(raw[0]!=32513)+(raw[1]!=-32512);
 memset(activation,0,sizeof(activation));
 activation[0]=-128;activation[1]=127;
 memset(packed,0,sizeof(packed));packed[0]=-128;packed[1]=-128;
 raw[0]=raw[1]=0;
 d.x=activation;d.w=packed;d.out=raw;d.channels=2;d.row_bytes=12;
 cnn_dw3(&d);
 results[1].raw_errors=(raw[0]!=16384)+(raw[1]!=-16256);
 done=0x600dcafe;return;
 }
#endif
 run(0,0,0,2,6);
#if BENCH_CASES > 1
 run(1,1,0,2,5);run(2,2,0,4,8);
#endif
#if BENCH_CASES > 3
 run(3,1,0,2,4);run(4,2,0,5,8);
 run(5,1,0,3,5);run(6,2,0,5,7);
 run(7,1,0,3,5);run(8,2,0,5,8);
 for(i=0;i<3;i++)run(9+i,i,1,2,6);
 for(i=0;i<3;i++)run(12+i,i,2,2,6);
#endif
 done=0x600dcafe;
}
