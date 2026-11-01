/*	edited from test_cfft.c
 *
 *	Example of the FFT routine from the Blackfin DSP library.
 *
 *	Compare results in the output buffer "out" to the
 * 	desired output (files "out_re.ans" and out_im.ans")
 *
 *	See the documentation for more info on the DSP routines
 *
 */

#include "ccblkfn.h"
#include <btc.h>
#include <math.h>
#include <fract.h>
#include <filter.h> 
#include <complex.h>

_Pragma("linkage_name __rfft8")
        
extern void rfft8(const fract16  *in,       	// Input array
                  complex_fract16 *t,       	// Temporary buffer
                  complex_fract16 *out,     	// Output array
                  const complex_fract16 *w, 	// Twiddle table 
                  int wst);                   	//   

/*----------------------------------------------------*/
/*   use the integer                                  */
/*----------------------------------------------------*/
/*--  input  16 point --*/
/* input sequence       */
fract16 x[32]=
{0x0800,
0x1000,
0x1800,
0x2000,
0x2800,
0x3000,
0x3800,
0x4000,
0x4800,
0x5000,
0x5800,
0x6000,
0x6800,
0x7000,
0x7800,
0x7FFF,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,}; 
          
fract16 h[32]=
{0x0CCC,
0x0CCC,
0x0CCC,
0x0CCC,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
0x0000,
};

/* output sequece     */
fract16 out[32];
complex_fract16 h_fft[8];
complex_fract16 x_fft[8];
complex_fract16 result_fft[8]; //存储x和h频域相乘后的结果
complex_fract16 result[8];	   //存储result_fft做逆变换后的结果

void main(void) 
{
	//用重叠相加法实现卷积
	
	complex_fract16 t[8]; //临时存储空间
	complex_fract16 w[8]; //旋转因子存储空间	
	fract16 x_temp[8];
		
	int loopCount = 4;
	int lenX = 4;
	
	int i,j;
	
    for(i=0;i<32;i++) out[i] = 0;
    for(i=0;i<8;i++) x_temp[i] = 0;
    
    twidfftrad2_fr16(w,8);//生成旋转因子
	rfft8(h,t,h_fft,w,1); //计算h_fft
    
	for(i=0;i<loopCount;i++)
	{
		for(j=0;j<lenX;j++) x_temp[j] = x[i*lenX+j];
		rfft8(x_temp,t,x_fft,w,1);
				
		for(j=0;j<8;j++) result_fft[j] = cmlt_fr16(x_fft[j],h_fft[j]);
		//ifft_fr16(result_fft,t,result,w,1,8,0,0);
		ifft_fr16(result_fft, result, w, 1, 8, 0, 0);
		for(j=0;j<8;j++) out[i*lenX+j]+=result[j].re;		
	}
		
//	while(1)
//	{
//	}

}
