#ifndef BF533_CNN_OPS_H
#define BF533_CNN_OPS_H
#include "cnn.h"
/* Specialized DS-CNN Small HWC operators, one image, 64 output channels.
 * REGULAR: 49x10x1 -> 25x5x64, kernel10x4, stride2, top/left pad4/1.
 * DEPTHWISE: 25x5x64 -> same, kernel3x3, stride1, pad1.
 * POINTWISE: 25x5x64 -> same, kernel1x1.
 * Caller allocates aligned buffers, no aliasing:
 * activation: DW 12096 shorts / PW 8000 shorts / regular unused.
 * packed_w: DW 576 / PW 4096 / regular 2560 shorts (prepared once).
 * patch: 64 shorts; sums: 64 ints; output: 8000 signed chars.
 * Put activation+patch in L1 A, packed_w in L1 B. No malloc/static scratch.
 * bl in [0,7], rs in [0,15], relu 0/1; bias contains 64 signed bytes.
 */
enum { CNN_REGULAR=0, CNN_DEPTHWISE=1, CNN_POINTWISE=2 };
void cnn_pack_weights(int kind,const signed char *w,short *packed_w);
void cnn_prepare(int kind,const signed char *in,short *activation);
void cnn_make_patch(const signed char *in,int pixel,short *patch);
void cnn_pixel(int kind,int pixel,const short *activation,const short *packed_w,
               const short *patch,int *sums);
signed char cnn_quantize(int sum,int bias,int bl,int rs,int relu);
void cnn_layer(int kind,const signed char *in,const short *packed_w,
               const signed char *bias,int bl,int rs,int relu,
               short *activation,short *patch,int *sums,signed char *out);
#endif
