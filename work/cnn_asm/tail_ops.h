#ifndef TAIL_OPS_H
#define TAIL_OPS_H
#include "cnn.h"
/* n>0; shift 0..15; lower = -128 or 0. Prepared add[c] includes bias and rounding.
 * sums + add must not overflow INT32. Buffers non-overlapping, 4-byte aligned. */
typedef struct {const int *sums;const int *add;signed char *out;int n,shift,lower;} PostArgs;
void cnn_post(const PostArgs *a);
/* Expanded HWC 125x64 signed INT8 values in INT16 storage.
 * Input/output must not overlap. Exact trunc(sum*4/125) then low byte,
 * matching original pool (NO saturation). Input expansion cost is separate.
 */
void cnn_pool(const short *input,signed char *out);
void cnn_fc_pack(const signed char *w,short *packed);
void cnn_fc(const short *input,const short *packed,const signed char *bias,
            signed char *out,int *sums,int *add);
#endif
