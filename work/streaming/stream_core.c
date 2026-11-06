#include "stream_core.h"
#include <string.h>
void stream_copy(void *dest,const void *source,unsigned bytes){
 volatile unsigned char *d=dest;const volatile unsigned char *s=source;unsigned i;
 /* 10-byte rows alternate 4/2-byte alignment. Volatile byte accesses prevent
  * CCES -O from widening a strided memcpy into an unaligned DWORD store. */
 for(i=0;i<bytes;i++)d[i]=s[i];
}
/* Runtime policy uses only incoming PCM and classifier scores. No manifest,
 * keyword sequence, clip identity or expected event times are linked here. */
void stream_init(StreamCore *s,const signed char silence[10]){
 unsigned i;memset(s,0,sizeof(*s));stream_copy(s->silence,silence,10);
 for(i=0;i<49;i++)stream_copy(s->ring[i],silence,10);
}
unsigned stream_hash(const void *p,unsigned bytes){
 const unsigned char *b=p;unsigned h=2166136261u,i;
 for(i=0;i<bytes;i++){h^=b[i];h*=16777619u;}return h;
}
unsigned stream_kept_hash(const StreamCore *s){
 unsigned row,col,h=2166136261u;
 for(row=0;row<49;row++)if(row!=s->head)for(col=0;col<10;col++){
  h^=(unsigned char)s->ring[row][col];h*=16777619u;
 }
 return h;
}
void stream_push(StreamCore *s,const signed char feature[10]){
 stream_copy(s->ring[s->head],feature,10);s->head=(s->head+1)%49;s->updates++;
}
void stream_snapshot(const StreamCore *s,signed char out[490]){
 unsigned i;for(i=0;i<49;i++)stream_copy(out+i*10,s->ring[(s->head+i)%49],10);
}
int stream_voice(const short pcm[320]){
 unsigned sum=0,i;
 /* RMS>100. Early exit is exact since all terms are nonnegative.
  * Before each add sum<=3,200,000; one squared INT16 fits signed INT32. */
 for(i=0;i<320;i++){
  int v=pcm[i];sum+=(unsigned)(v*v);if(sum>320u*100u*100u)return 1;
 }
 return 0;
}
void stream_on_audio(StreamCore *s,unsigned t,int voice){
 unsigned c;
 if(voice){
  if(!s->active){
   s->active=1;s->start_ms=t;s->voiced_frames=0;
   for(c=0;c<STREAM_CLASSES;c++){s->votes[c]=0;s->peaks[c]=-128;}
  }
  s->last_voice_ms=t;s->voiced_frames++;
 }
}
int stream_on_scores(StreamCore *s,unsigned t,const signed char scores[STREAM_CLASSES],StreamEvent *e){
 unsigned c,best=0,runner,b;int emit=0;
 if(!s->active)return 0;
 for(c=1;c<STREAM_CLASSES;c++)if(scores[c]>scores[best])best=c;
 runner=best==0?1:0;
 for(c=0;c<STREAM_CLASSES;c++)if(c!=best && scores[c]>scores[runner])runner=c;
 if(t-s->start_ms>=300 && best>=2 && scores[best]>=0 && scores[best]-scores[runner]>=10){
  s->votes[best]++;if(scores[best]>s->peaks[best])s->peaks[best]=scores[best];
 }
#ifdef STREAM_BOARD
 if(t-s->last_voice_ms>=300){
#else
 if(t-s->last_voice_ms>=600){
#endif
  b=2;
  for(c=3;c<STREAM_CLASSES;c++)if(s->votes[c]>s->votes[b] || (s->votes[c]==s->votes[b] && s->peaks[c]>s->peaks[b]))b=c;
  if(s->voiced_frames>=3 && s->votes[b]>=2){
   e->time_ms=t;e->label=b;e->votes=s->votes[b];e->peak=s->peaks[b];emit=1;
  }
  s->active=0;
 }
 return emit;
}
