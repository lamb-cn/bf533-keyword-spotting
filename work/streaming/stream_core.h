#ifndef STREAM_CORE_H
#define STREAM_CORE_H
#ifdef STREAM_BOARD
#define STREAM_CLASSES 13
#else
#define STREAM_CLASSES 12
#endif
#define STREAM_ROWS 49
#define STREAM_COEFFS 10
typedef struct {
 signed char ring[49][10],silence[10];
 unsigned head,updates,active,start_ms,last_voice_ms,voiced_frames;
 unsigned votes[STREAM_CLASSES];int peaks[STREAM_CLASSES];
} StreamCore;
typedef struct {unsigned time_ms,label,votes;int peak;} StreamEvent;
void stream_init(StreamCore *s,const signed char silence[10]);
unsigned stream_hash(const void *p,unsigned bytes);
void stream_copy(void *dest,const void *source,unsigned bytes);
unsigned stream_kept_hash(const StreamCore *s);
void stream_push(StreamCore *s,const signed char feature[10]);
void stream_snapshot(const StreamCore *s,signed char feature[490]);
int stream_voice(const short pcm[320]);
void stream_on_audio(StreamCore *s,unsigned time_ms,int voiced);
int stream_on_scores(StreamCore *s,unsigned time_ms,const signed char scores[STREAM_CLASSES],StreamEvent *event);
#endif
