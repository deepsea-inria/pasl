/*
 *  cholesky.c
 *
 *
 *  Created by David S. Wise on Thu Feb 13 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 * Ported to pasl by Mike Rainey
 * Implements the algorithm described here:
 
 @incollection{wise2005paradigm,
 title={A Paradigm for Parallel Matrix Algorithms},
 author={Wise, David S and Citro, Craig and Hursey, Joshua and Liu, Fang and Rainey, Michael},
 booktitle={Euro-Par 2005 Parallel Processing},
 pages={687--698},
 year={2005},
 publisher={Springer}
 }
 
 */

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "benchmark.hpp"
#include "granularity.hpp"

namespace par = pasl::sched::granularity;

#define RESTRICT

/*---------------------------------------------------------------------*/
/* dilate.h "

/* This is designed for 16-bit undilated integers and 32-bit dilated
 * integers.  If we want to scale that up, it's an easy change but you will
 * need to make it. - Greg 00/05/12
 */

#define dilateBits(x) (dilate_lut[((unsigned int)x)&0xff]+(dilate_lut[(((unsigned int)x)>>8)&0xff]<<16))
#define undilateBits(x) (undilate_lut[((unsigned int)x)&0xff]			\
+ (undilate_lut[(((unsigned int)x)>>8)&0xff]<<4)	\
+ (undilate_lut[(((unsigned int)x)>>16)&0xff]<<8)	\
+ (undilate_lut[(((unsigned int)x)>>24)&0xff]<<12))

#define  undilateByte(i)  ((int)( undilate_lut[ ((unsigned int)(i))&0xff]))
#define undilateShort(i)                                         \
(  (int)(undilate_lut[ ((unsigned int)(i))    &0xff])      \
+((int)(undilate_lut[(((unsigned int)(i))>>8)&0xff])<<4) )


static const unsigned short int dilate_lut[256] = {
  /* lut   ==  "look-up table"   */
  0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015,
  0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
  0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115,
  0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
  0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415,
  0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
  0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515,
  0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,
  0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015,
  0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
  0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115,
  0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
  0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415,
  0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
  0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515,
  0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
  0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015,
  0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
  0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115,
  0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
  0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415,
  0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
  0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515,
  0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
  0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015,
  0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
  0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115,
  0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
  0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415,
  0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
  0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515,
  0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555,
};

static const unsigned char undilate_lut[256] = {
  /* lut   ==  "look-up table"   */
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
  0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f, 0x0e, 0x0f,
};

#define evenBits (((unsigned int)-1) /3)
#define oddBits (evenBits << 1)
#define  evenPlus1(i) (  ((i) -evenBits)&evenBits)
#define   oddPlus1(j) (  ((j) - oddBits)& oddBits)
#define evenMinus1(i) (  ((i) -       1)&evenBits)
#define  oddMinus1(j) (  ((j) -       2)& oddBits)
#define evenInc(i) (i =  ((i) -evenBits)&evenBits)
#define  oddInc(j) (j =  ((j) - oddBits)& oddBits)
#define evenDec(i) (i =  ((i) -       1)&evenBits)
#define  oddDec(j) (j =  ((j) -       2)& oddBits)

#define evenDilate(i)   (dilateBits(i)    )
#define  oddDilate(i)   (dilateBits(i) <<1)
#define evenUndilate(i) (undilateBits(i))
#define  oddUndilate(i) (undilateBits((i)>>1))

#define evenAdd(i,j)       (   ((i) + (j) + oddBits) & evenBits)
#define oddAdd(i,j)        (   ((i) + (j) + evenBits)&  oddBits)
#define evenSub(i,j)       (   ((i) - (j))           & evenBits)
#define oddSub(i,j)        (   ((i) - (j))           &  oddBits)
#define evenAddAssign(i,j) (i= ((i) + (j) + oddBits) & evenBits)
#define oddAddAssign(i,j)  (i= ((i) + (j) + evenBits)&  oddBits)
#define evenSubAssign(i,j) (i= ((i) - (j))           & evenBits)
#define oddSubAssign(i,j)  (i= ((i) - (j))           &  oddBits)

#define evenToOdd(i) (i<<1)
#define oddToEven(j) (j>>1)

/*---------------------------------------------------------------------*/
/* ahnentafel.h */


#define nw(i) (4*i)
#define sw(i) (4*i+1)
#define ne(i) (4*i+2)
#define se(i) (4*i+3)
#define north(a)  (2*(a)  )
#define south(a)  (2*(a)+1)

static int ceilingLg(int i)
{
  int x;
  int lg;
  for (x=i-1, lg=0;
       (x>0);
       x/=2) lg++;
  return lg;
  /*  1|->0;   2|->1;   3|->2;  4|->2;  5|->3    */
}

/*---------------------------------------------------------------------*/
/* baseCases.c */

#ifndef baseOrderLg
#define baseOrderLg 4
/* On Ultras with 16KB D-cache, 4 is too small, and
 5 is just slightly too large.
 3  => 3 matrices at 512B each.
 4  => 3 matrices at 2048B each.
 5  => 3 matrices at 8096B each.
 
 On SGI R1000, 5 is a nicely safe choice and 6 could be too big.
 but only because it is 4-way interleaved.
 
 BUT on Suns with dierct-mapped cache, these numbers should be
 held smaller by magintude of one---at least.
 Otherwise the likelihood of repeated collision is so high
 that the (admittedly rare) case where two operants map
 to the single cache line will run at NOTICABLY L2 cache speed.
 Monica Lam ?? said this ??
 http://doi.acm.org/10.1145/106972.106981
 */
#endif

#ifndef runFLOPSlengthLg
#define runFLOPSlengthLg 8
/*
 8  =>  256 flops or 128 inline madds.
 9  =>  512 flops or 256 inline madds.
 10 => 1024 flops or 512 inline madds.
 
 On Ultras with 16KB I-cache and over 8 bytes per flop
 instruction, there is room for a 1024 flop run, but not 2048.
 
 On R10000 with 32KB I-cache and madd instructions,
 12 is a better setting.
 
 Once again, the length of these runs can exacerbate he bad effects
 of bases of cache-mapping collissions.  So be careful!!!S
 */
#endif


#ifndef soloCommitLg
#define soloCommitLg 2
/*
 Just a dummy in the uniprocessor code.
 But the purpose is to announce how much above the bases
 any parallelism will cease.
 Value of 2 implies 4baseBlocks x 4baseBlocks
 will not ever be run in parallel.
 */
#endif

int tdepth;

#define baseSize (1<<(2* baseOrderLg ))
/* e.g. 256   the area of base block.
 Also a proper bound on local Morton indices.
 */
#define baseOrder (1<< baseOrderLg )
/* e.g. 16   the order of base block. */
/* This is for 16x16 base case in extendBase code.   */

static int aLeaf;
static int leafBound;
static double * aa;
/*
 It is up to us as programmers to be sure that one
 block being read (by a processor or task or superscalar stream)
 does *not* depend on another block being written at the same time.
 */

static int rowMask = (evenBits & (baseSize -1));
static int colMask =  (oddBits & (baseSize -1));

static int level;
static int stripeBound[32];

static int parallelBound;
static int SEblock;


static void doCholeskyBase(int quad);
static void dropSWbase(int sQuad);
/* static void extendSEblkBase(int eQuad, int wQuad); */
static void extendSEtriBase(int wQuad);

/* See the outer-product version of Cholesky
 in Golub and Van Loan,   Algorithm 4.2.2, Page 145.
 */

static void doCholeskyBase(int ahnenQuad){
  register int k, kkk, i, ii, j, jj;
  ahnenQuad *= baseSize;
  for     (k=0,   kkk= (ahnenQuad&colMask); k<baseOrder;
           k++, oddInc(kkk) ) {
    aa[kkk/2 + kkk] = sqrt( aa[kkk/2 + kkk]);
    for   (i=k+1, ii=evenPlus1(kkk/2);     i<baseOrder;
           i++,   evenInc(ii) )       /* k<i */
      aa[ii+kkk] /= aa[kkk/2+kkk];
    for   (j=k+1, jj=evenPlus1(kkk/2);     j<baseOrder;
           j++,   evenInc(jj) )       /* k<j */
      for (i=j,   ii=jj             ;     i<baseOrder;
           i++,   evenInc(ii) )       /* j<=i */
        aa[ii+ 2*jj] -= aa[ii+kkk] * aa[jj+kkk];
  }
  return;
}

static void dropSWbase(int sQuad           ){
  int k, kkk, i, ii, j, jj;
  sQuad *= baseSize;
  for     (k=0,   kkk= (sQuad&colMask); k<baseOrder;
           k++,   oddInc(kkk) ){
    for   (i=0,   ii = (sQuad&rowMask); i<baseOrder;
           i++,   evenInc(ii) )       /* k<i */
      aa[ii+kkk] /= aa[kkk/2+kkk];
    for   (j=k+1, jj=evenPlus1(kkk/2); j<baseOrder;
           j++,   evenInc(jj) )       /* k<j */
      for (i=0,   ii = (sQuad&rowMask); i<baseOrder;
           i++,   evenInc(ii))        /* j<i */
        aa[ii+ 2*jj] -= aa[ii+kkk] * aa[jj+kkk];
  }
  return;
}


/* k  is always an odd-dilated index controlling dot product.  */
#define  madd1(row,col,k)        aBlock[row+k]*bBlock[col+k]
/* eee order here-    ---------^---------------^        */
#define  madd2(row,col,k)    madd1(row,col,k)+madd1(row,col,k+2)
#define  madd4(row,col,k)    madd2(row,col,k)+madd2(row,col,k+8)
#define  madd8(row,col,k)    madd4(row,col,k)+madd4(row,col,k+32)
#define madd16(row,col,k)    madd8(row,col,k)+madd8(row,col,k+128)
#define madd32(row,col,k)   madd16(row,col,k)+madd16(row,col,k+512)
#define madd64(row,col,k)   madd32(row,col,k)+madd32(row,col,k+2048)
#define madd128(row,col,k)  madd64(row,col,k)+madd64(row,col,k+8192)



#define bodyBlockLg     ((runFLOPSlengthLg-1)/3)
#define bodyPaired1 (0==((runFLOPSlengthLg-1)%3))   /* This is block size   */
#define bodyPaired2 (1==((runFLOPSlengthLg-1)%3))   /* cubeRt(number madds) */
#define bodyPaired4 (2==((runFLOPSlengthLg-1)%3))   /* == size for max run. */

#if (bodyPaired1)

#if   (bodyBlockLg == 0)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd1(row,col,0))
#elif (bodyBlockLg == 1)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd2(row,col,0))
#elif   (bodyBlockLg == 2)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd4(row,col,0))
#elif (bodyBlockLg == 3)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd8(row,col,0))
#elif (bodyBlockLg == 4)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd16(row,col,0))
#elif (bodyBlockLg == 5)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd32(row,col,0))
#elif (bodyBlockLg == 6)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd64(row,col,0))
#else
#error        dot Order out of bounds.   Compiler should never see this.
#endif

#else

#if   (bodyBlockLg == 0)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd2(row,col,0))
#elif (bodyBlockLg == 1)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd4(row,col,0))
#elif (bodyBlockLg == 2)
#define dot1(row, col)       cBlock[row+2*(col)]-= (madd8(row,col,0))
#elif (bodyBlockLg == 3)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd16(row,col,0))
#elif (bodyBlockLg == 4)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd32(row,col,0))
#elif (bodyBlockLg == 5)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd62(row,col,0))
#elif (bodyBlockLg == 6)
#define dot1(row, col)       cBlock[row+2*(col)]-=(madd128(row,col,0))
#else
#error        dot Order out of bounds.   Compiler should never see this.
#endif

#endif

#define   dot2(row,col)    dot1(row,col);   dot1(row+1,col)
#define   dot4(row,col)    dot2(row,col);   dot2(row,col+1)
#define   dot8(row,col)    dot4(row,col);   dot4(row+4,col)
#define  dot16(row,col)    dot8(row,col);   dot8(row,col+4)
#define  dot32(row,col)   dot16(row,col);   dot16(row+16,col)
#define  dot64(row,col)   dot32(row,col);   dot32(row,col+16)
#define dot128(row,col)   dot64(row,col);   dot64(row+64,col)
#define dot256(row,col)  dot128(row,col);   dot128(row,col+64)
#define dot512(row,col)  dot256(row,col);   dot256(row+256,col)
#define  dot1k(row,col)  dot512(row,col);   dot512(row,col+256)
#define  dot2k(row,col)   dot1k(row,col);   dot1k(row+1024,col)
#define  dot4k(row,col)   dot2k(row,col);   dot2k(row,col+1024)
/* eee order here-----^-------^       */

#if   (bodyBlockLg==0)
#define prod     dot1(0,0)
#elif (bodyBlockLg==1)
#define prod     dot4(0,0)
#elif (bodyBlockLg==2)
#define prod    dot16(0,0)
#elif (bodyBlockLg==3)
#define prod    dot64(0,0)
#elif (bodyBlockLg==4)
#define prod   dot256(0,0)
#elif (bodyBlockLg==5)
#define prod    dot1k(0,0)
#elif (bodyBlockLg==6)
#define prod    dot4k(0,0)
#else
#error   This line cannot be reached.  The bounds preclude this case.
#endif




/*
 CASE: bodyPaired1
 ----         ----   ----
 |  |  -=     |  | * |  |        1* bodyBlock^3  flops.
 ----         ----   ----
 dotProd length is bodyBlock.
 kk+=    bodyBlock^2.
 ij+=    bodyBlock^2.
 
 CASE: bodyPaired2
 ----      -------   -------
 |  |  -=  |  |  | * |  |  |     2* bodyBlock^3  flops.
 ----      -------   -------
 
 dotProd length is 2*bodyBlock.
 kk+=  (2*bodyBlock)^2.
 ij+=    bodyBlock^2.
 
 CASE: bodyPaired4
 ----      -------
 |  |      |  |  |   -------
 ----  -=  ------- * |  |  |     4* bodyBlock^3  flops.
 |  |      |  |  |   -------
 ----      -------
 dotProd length is 2*bodyBlock.
 kk+=  (2*bodyBlock)^2.
 ij+=  2* bodyBlock^2.
 
 And bodyBlock doubles at      8* bodyBlock^3  flops,  so bodyPaired1.
 */

#if (bodyPaired1)
#define kkStep (1<<(2*(bodyBlockLg)+1))
/* oddDilation of bodyBlock to increment strides of dotProducts */
#else
#define kkStep (1<<(2*(bodyBlockLg+1)+1))
/* oddDilation of 2*bodyBlock to increment strides of dotProducts */
#endif


/*
 static
 */
void extendSEblkBase(int d, int c)
{ /*   register int i, j, k;
   for(    j=0; j<baseSize; evenInc(j))
   for(  i=0; i<baseSize; evenInc(i))
   for(k=0; k<baseSize;  oddInc(k))
   aa[d*baseSize-aLeaf +i+2*j] -=
   aa[c*baseSize-aLeaf +i+k] * aa[bT*baseSize-aLeaf +j+k];
   */
  register int ij, kk;
  double * RESTRICT D  =  d*baseSize  +  aa-aLeaf;
  double * RESTRICT C  =  c*baseSize  +  aa-aLeaf;
  double * RESTRICT BT = ( (d&0xaaaaaaaa)/2 + (c&0xaaaaaaaa) )*baseSize + aa-aLeaf;
  
  for (kk=0;
       kk< (1<<(2*baseOrderLg));                     /* baseOrder**2  */
       kk = ((kk + kkStep +0x555) & 0xAAA) )         /* oddAddition  */
    
#pragma ivdep
#pragma no fission
#if (bodyPaired4)
    for (ij=0;  ij<   (1<<(2*baseOrderLg));
         ij += (2<<(2*bodyBlockLg)) )
    {
      register double *cBlock =
      &(D      [                     ij            ]);
      register double *aBlock =
      &(C      [              kk +  (ij&0x555)     ]);
      register double *bBlock =
      &(BT     [              kk + ((ij&0xAAA)>>1) ]);
      /* eee order here---------^^^  */
      prod;                                           /* N square in C.*/
      cBlock += (1<<(2*bodyBlockLg));
      aBlock += (1<<(2*bodyBlockLg));
      prod;                                           /* S square in C.*/
    }
  
#else
  for (ij=0;  ij  < (1<<(2*baseOrderLg));
       ij += (1<<(2*bodyBlockLg)) )
  {
    register double *cBlock =
    &(D      [                     ij            ]);
    register double *aBlock =
    &(C      [              kk +  (ij&0x555)     ]);
    register double *bBlock =
    &(BT     [              kk + ((ij&0xAAA)>>1) ]);
    /* eee order here---------^^^  */
    prod;                                           /* whole square */
  }
  
#endif
  
}

#undef runFLOPSlengthLg
#undef  madd1
#undef  madd2
#undef  madd4
#undef  madd8
#undef madd16
#undef madd32
#undef madd64
#undef madd128
#undef bodyBlockLg
#undef bodyPaired1
#undef bodyPaired2
#undef bodyPaired4
#undef   dot1
#undef   dot2
#undef   dot4
#undef   dot8
#undef  dot16
#undef  dot32
#undef  dot64
#undef dot128
#undef dot256
#undef dot512
#undef  dot1k
#undef  dot2k
#undef  dot4k
#undef prod
#undef kkStep


/*
 static void extendSEtriBase(          int wQuad){
 int mortonNW = wQuad*baseSize - aLeaf;
 int iijjj     = 3*(mortonNW & rowMask);
 int bound = iijjj+baseSize;
 int increment = 1;
 while ( iijjj < bound)
 { int ii =  (iijjj&rowMask);
 int jj=  (iijjj&colMask)/2;
 if (ii < jj){
 iijjj += increment; increment= 1;
 }else{
 int k, kkk;
 for (k=0, kkk=0; k<baseOrder; k+=16, kkk= oddAdd(kkk,0x200) )
 {
 double * RESTRICT A= &aa[ii+ kkk];
 double * RESTRICT B= &aa[jj+ kkk];
 aa[iijjj]-= (A[0x00]*B[0x00] + A[0x02]*B[0x02] +
 A[0x08]*B[0x08] + A[0x0A]*B[0x0A] +
 A[0x20]*B[0x20] + A[0x22]*B[0x22] +
 A[0x28]*B[0x28] + A[0x2A]*B[0x2A] +
 A[0x80]*B[0x80] + A[0x82]*B[0x82] +
 A[0x88]*B[0x88] + A[0x8A]*B[0x8A] +
 A[0xA0]*B[0xA0] + A[0xA2]*B[0xA2] +
 A[0xA8]*B[0xA8] + A[0xAA]*B[0xAA] );
 }
 iijjj++;    increment++;
 }
 }
 return;
 }
 */

/*
 It is often useful to have a base case less than 16x16.  Thus the first
 definition of extendSEtriBase is necessary. --marainey
 */
#if(baseOrderLg<4)
static void extendSEtriBase(int wQuad) {
  wQuad *= baseSize;
  register int i,j,k,ii,jj,kkk;
  for(j=0,jj=wQuad&rowMask; j<baseOrder; j++, evenInc(jj)) {
    for(i=j,ii=jj;i<baseOrder;i++,evenInc(ii)) {
      for(k=0,kkk=wQuad&colMask;k<baseOrder;k++,oddInc(kkk)) {
        aa[ii+2*jj] -= aa[ii+kkk] * aa[jj+kkk];
      }
    }
  }
}
#else
/*
 I added a couple fixes to this code to make it work.  However, this code
 might still be unstable, so keep an eye on it.  --marainey
 */
static void extendSEtriBase(int wQuad) {
  register int mortonTri, iijjj, increment, k, kkk;
  increment = -1;
  wQuad *= baseSize;
  iijjj = 3*(wQuad&rowMask);
  mortonTri = iijjj+baseSize;
  while(iijjj<mortonTri) {
    int ii = iijjj&rowMask;
    int jj = (iijjj&colMask)/2;
    if(ii<jj) {
      iijjj += increment; increment = -1;
    } else {
      double * RESTRICT C = &aa[iijjj];
      double * RESTRICT A;
      double * RESTRICT B;
      for(k=0, kkk=wQuad&colMask; k<baseOrder; k+=16, oddAddAssign(kkk,0x200)) {
        A = &aa[ii+kkk]; B = &aa[jj+kkk];
        *C -= (A[0x00]*B[0x00] + A[0x02]*B[0x02] +
               A[0x08]*B[0x08] + A[0x0A]*B[0x0A] +
               A[0x20]*B[0x20] + A[0x22]*B[0x22] +
               A[0x28]*B[0x28] + A[0x2A]*B[0x2A] +
               A[0x80]*B[0x80] + A[0x82]*B[0x82] +
               A[0x88]*B[0x88] + A[0x8A]*B[0x8A] +
               A[0xA0]*B[0xA0] + A[0xA2]*B[0xA2] +
               A[0xA8]*B[0xA8] + A[0xAA]*B[0xAA] );
      }
      iijjj++; increment++;
    }
  }
}
#endif

/*---------------------------------------------------------------------*/
/* central.c */

static void doCholeskyCNTL(int quad);
static void BKdropSWcntl(int sQuad /*,int nQuad*/);
static void WHdropSWcntl(int sQuad /*,int nQuad*/);
static void DNextendSEblkCNTL(int eQuad, int wQuad /*,int nQuadT*/);
static void UPextendSEblkCNTL(int eQuad, int wQuad /*,int nQuadT*/);
static void BKextendSEtriCNTL(int eQuad, int wQuad);
static void WHextendSEtriCNTL(int eQuad, int wQuad);


static void doCholeskyCNTL(int quad){
  if (quad >=leafBound)   doCholeskyBase(quad);
  else{
    /* These calls are totally ordered.  */
    /* For an NxN matrix, at most 2N+lgN invocations. */
    /* Under these constraints, no dovetailing attempted.*/
    /* Precondition:                        nw(quad) in L1 cache  */
    doCholeskyCNTL   (                      nw(quad) );
    BKdropSWcntl     (          sw(quad) /*,nw(quad)*/ );
    /* SE */
    /* NW*SE */
    BKextendSEtriCNTL(se(quad), sw(quad) );
    /* NW */
    doCholeskyCNTL   (se(quad));
    /* Postcondn:     se(quad)    in L1 cache  */
  }
  return;
}

static void BKdropSWcntl(int sQuad /*,int nQuad*/ ){
  if (sQuad >=leafBound)  dropSWbase(sQuad /*,nQuad*/);
  else{
    /*  Partial orders:  A<D<E;     B<C<F    */
    /*    Dovetailed at 3 of the 6 joins. */
    /* Precondition:  either nw(sQuad)   or    nw(nQuad) in L1 cache */
    BKdropSWcntl     (   nw(sQuad)            /*,nw(nQuad)*/); /*A*/
    WHdropSWcntl     (           sw(sQuad)    /*,nw(nQuad)*/); /*B*/
    /* NE */
    DNextendSEblkCNTL(se(sQuad), sw(sQuad)    /*sw,(nQuad)*/); /*C*/
    /* ?NE */
    UPextendSEblkCNTL(   ne(sQuad), nw(sQuad) /*,sw(nQuad)*/); /*D*/
    /* NW */
    BKdropSWcntl     (   ne(sQuad)      /*,se(nQuad)*/      ); /*E*/
    BKdropSWcntl     (     se(sQuad)    /*,se(nQuad)*/      ); /*F*/
    /* Postcondn:  both    se(sQuad)  and  se(nQuad)     in L1 cache  */
  }
  return;
}
static void WHdropSWcntl(int sQuad /*,int nQuad*/ ){
  if (sQuad >=leafBound)  dropSWbase(sQuad /*,nQuad*/);
  else{
    /*  Partial orders:  A<D<E;     B<C<F    */
    /*    Dovetailed at 3 of the 6 joins. */
    /* Precondition: either sw(sQuad)  or     nw(nQuad) in L1 cache */
    WHdropSWcntl     (      sw(sQuad)      /*,nw(nQuad)*/   ); /*B*/
    BKdropSWcntl     (           nw(sQuad) /*,nw(nQuad)*/   ); /*A*/
    /* SE */
    UPextendSEblkCNTL(ne(sQuad), nw(sQuad)    /*,sw(nQuad)*/); /*D*/
    /* ?NE */
    DNextendSEblkCNTL(  se(sQuad), sw(sQuad)  /*,sw(nQuad)*/); /*C*/
    /* SW */
    WHdropSWcntl     (  se(sQuad)         /*,se(nQuad)*/    ); /*F*/
    WHdropSWcntl     (     ne(sQuad)      /*,se(nQuad)*/    ); /*E*/
    /* Postcondition: both ne(sQuad)   and   se(nQuad)  in L1 cache */
  }
  return;
}

static void DNextendSEblkCNTL(int eQuad, int wQuad /*,int nQuadT*/ ){
  if (eQuad >= leafBound) extendSEblkBase(eQuad, wQuad /*,nQuadT*/);
  else{
    /*  Unordered.    */
    /*    Dovetailed at all 8 joins. */
    /* Precondition:  either nw(eQuad) or ne(wQuad) or ne(nQuadT) in L1 cache  */
    DNextendSEblkCNTL( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
    /* ?SW */
    UPextendSEblkCNTL( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* ?NE */
    DNextendSEblkCNTL(   ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
    /* ?SW */
    UPextendSEblkCNTL(   ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
    /* NE */
    UPextendSEblkCNTL( se(eQuad),   se(wQuad)     /*,se(nQuadT)*/ );
    /* ?NW */
    DNextendSEblkCNTL( se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
    /* ?SE */
    UPextendSEblkCNTL(   sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* ?NW */
    DNextendSEblkCNTL(   sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* Postcondition:  each of sw(eQuad), se(wQuad), ne(nQuadT) in L1 cache  */
  }
  return;
}

static void UPextendSEblkCNTL(int eQuad, int wQuad /*,int nQuadT*/){
  if (eQuad >= leafBound) extendSEblkBase(eQuad, wQuad /*,nQuadT*/);
  else{
    /*  Unordered.    */
    /*    Dovetailed at all 8 joins. */
    /* Precondition:  either sw(eQuad) or se(wQuad) or ne(nQuadT) in L1 cache  */
    UPextendSEblkCNTL(   sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* ?NW */
    DNextendSEblkCNTL(   sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* ?SE */
    UPextendSEblkCNTL( se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
    /* ?NW */
    DNextendSEblkCNTL( se(eQuad), se(wQuad)       /*,se(nQuadT)*/ );
    /* NE */
    DNextendSEblkCNTL(   ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
    /* ?SW */
    UPextendSEblkCNTL(   ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
    /* ?NE */
    DNextendSEblkCNTL( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* ?SW */
    UPextendSEblkCNTL( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
    /* Postcondition:  each of nw(eQuad), ne(wQuad), ne(nQuadT) in L1 cache  */
  }
  return;
}

static void BKextendSEtriCNTL(int eQuad, int wQuad){
  if (wQuad >= leafBound) extendSEtriBase( /*eQuad,*/ wQuad);
  else{
    /*  Unordered.    */
    /*    Dovetailed at all 6 joins. */
    /* Pretcondition:  something useful in L1 cache:  nw*se(wQuad), ne*sw(wQuad) */
    WHextendSEtriCNTL(  se(eQuad),          sw(wQuad)         );
    /* ? */
    BKextendSEtriCNTL(  se(eQuad), se(wQuad)                  );
    /* NE */
    DNextendSEblkCNTL(sw(eQuad),   se(wQuad)   /*,ne(wQuad)*/ );
    /* ?SW */
    UPextendSEblkCNTL(sw(eQuad), sw(wQuad) /*,nw(wQuad)*/     );
    /* NE */
    WHextendSEtriCNTL(  nw(eQuad),            nw(wQuad)       );
    /* ? */
    BKextendSEtriCNTL(  nw(eQuad),          ne(wQuad)         );
    /* Postcondition:  both nw(eQuad) and ne(wQuad) in L1 cache  */
  }
  return;
}

static void WHextendSEtriCNTL(int eQuad, int wQuad){
  if (wQuad >= leafBound) extendSEtriBase( /*eQuad,*/ wQuad);
  else{
    /*  Unordered.    */
    /*    Dovetailed at all 6 joins. */
    /* Precondition:  either nw(eQuad) or ne(wQuad) in L1 cache  */
    WHextendSEtriCNTL(  nw(eQuad),        ne(wQuad)           );
    /* ? */
    BKextendSEtriCNTL(  nw(eQuad),            nw(wQuad)       );
    /*NE*/
    DNextendSEblkCNTL(sw(eQuad), sw(wQuad) /*,nw(wQuad)*/     );
    /* ?SW */
    UPextendSEblkCNTL(sw(eQuad),   se(wQuad)   /*,ne(wQuad)*/ );
    /* NE */
    WHextendSEtriCNTL(  se(eQuad), se(wQuad)                  );
    /* ? */
    BKextendSEtriCNTL(  se(eQuad),          sw(wQuad)         );
    /* Postcondition:  something useful in L1 cache:   nw*se(wQuad), ne*sw(wQuad) */
  } }

/*---------------------------------------------------------------------*/
/* centralParallel.c */

struct arguments { int c, a, pr; };

static void doCholeskyCNTLpar(int procs, int quad);
static void BKdropSWcntlPAR(int procs, int sQuad /*,int nQuad*/);
static void WHdropSWcntlPAR(int procs, int sQuad /*,int nQuad*/);
static void DNextendSEblkCNTLpar(int procs, int eQuad, int wQuad /*,int nQuadT*/);
static void UPextendSEblkCNTLpar(int procs, int eQuad, int wQuad /*,int nQuadT*/);
static void BKextendSEtriCNTLpar(int procs, int eQuad, int wQuad);
static void WHextendSEtriCNTLpar(int procs, int eQuad, int wQuad);


static void doCholeskyCNTLpar(int procs, int quad){
  if (quad >=parallelBound)   doCholeskyCNTL(quad);
  else{
    /*SERIAL*/
    /* These calls are totally ordered.  */
    /* For an NxN matrix, at most 2N+lgN invocations. */
    /* Under these constraints, no dovetailing attempted.*/
    /* Precondition:                                  nw(quad) in L1 cache  */
    doCholeskyCNTLpar   (procs,                       nw(quad) );
    BKdropSWcntlPAR     (procs,           sw(quad) /*,nw(quad)*/ );
    /* SE */
    /* NW*SE */
    BKextendSEtriCNTLpar(procs, se(quad), sw(quad) );
    /* NW */
    doCholeskyCNTLpar   (procs, se(quad));
    /* Postcondn:               se(quad)    in L1 cache  */
  }
  return;
}

static void BKdropSWcntlPAR(int procs, int sQuad /*,int nQuad*/ ){
  if (sQuad >=parallelBound)   BKdropSWcntl(sQuad /*,nQuad*/);
  else{
    /*SERIAL*/
    /*  Partial orders:  A<D<E;     B<C<F    */
    /*    Dovetailed at 3 of the 6 joins. */
    /* Precondition:  either       nw(sQuad)     or       nw(nQuad) in L1 cache */
    BKdropSWcntlPAR     (procs,    nw(sQuad)            /*,nw(nQuad)*/); /*A*/
    WHdropSWcntlPAR     (procs,            sw(sQuad)    /*,nw(nQuad)*/); /*B*/
    /* NE */
    DNextendSEblkCNTLpar(procs, se(sQuad), sw(sQuad)    /*sw,(nQuad)*/); /*C*/
    /* ?NE */
    UPextendSEblkCNTLpar(procs,    ne(sQuad), nw(sQuad) /*,sw(nQuad)*/); /*D*/
    /* NW */
    BKdropSWcntlPAR     (procs,    ne(sQuad)      /*,se(nQuad)*/      ); /*E*/
    BKdropSWcntlPAR     (procs,      se(sQuad)    /*,se(nQuad)*/      ); /*F*/
    /* Postcondn:  both              se(sQuad)  and  se(nQuad)     in L1 cache  */
  }
  return;
}

static void WHdropSWcntlPAR(int procs, int sQuad /*,int nQuad*/ ){
  if (sQuad >=parallelBound)   WHdropSWcntl(sQuad /*,nQuad*/);
  else{
    /*SERIAL*/
    /*  Partial orders:  A<D<E;     B<C<F    */
    /*    Dovetailed at 3 of the 6 joins. */
    /* Precondition: either sw(sQuad)  or     nw(nQuad) in L1 cache */
    WHdropSWcntlPAR     (procs,       sw(sQuad)      /*,nw(nQuad)*/   ); /*B*/
    BKdropSWcntlPAR     (procs,            nw(sQuad) /*,nw(nQuad)*/   ); /*A*/
    /* SE */
    UPextendSEblkCNTLpar(procs, ne(sQuad), nw(sQuad)    /*,sw(nQuad)*/); /*D*/
    /* ?NE */
    DNextendSEblkCNTLpar(procs,   se(sQuad), sw(sQuad)  /*,sw(nQuad)*/); /*C*/
    /* SW */
    WHdropSWcntlPAR     (procs,   se(sQuad)         /*,se(nQuad)*/    ); /*F*/
    WHdropSWcntlPAR     (procs,      ne(sQuad)      /*,se(nQuad)*/    ); /*E*/
    /* Postcondition: both ne(sQuad)   and   se(nQuad)  in L1 cache */
  }
  return;
}


/*   The comment   /# NW/SW #/  indicates a quadrant that will be preloaded
 in cache.  The NW declares that if this function were recrrring down
 to a base case, then the northwest quadrant of that base case would be in cache.
 If the recurrence does not go to uniprocessing and to a base case,
 then the SW indicates that the southwest quadrant of the aligned parameter
 is held in cache between the two recursive calls.
 */

using controller_type = par::control_by_prediction;

controller_type DNextendSEblkCNTLpar_contr("DNextendSEblkCNTLpar");

long complexity(int quad) {
  int lg = tdepth - level;
  long order = 1<<lg;
  return order * order * order;
}

static void * dnXPwest(void *args);
static void * dnXPeast(void *args);
static void DNextendSEblkCNTLpar(int procs, int eQuad, int wQuad /*,int nQuadT*/ ){
  par::cstmt(DNextendSEblkCNTLpar_contr, [&] { return complexity(eQuad); }, [&] {
    if (procs==1 || eQuad >= parallelBound) DNextendSEblkCNTL(eQuad, wQuad /*,nQuadT*/);
    else{
      /*  Unordered.    */
      /*    Dovetailed at all 4 joins. */
      pthread_t       tid0, tid1;
      struct arguments in;
      int ec;                 /*error code*/
      
      /* Setup the structures to pass data to the threads. */
      in.c = eQuad;  in.a = wQuad;  in.pr= procs/2;

      par::fork2([&] {
        dnXPwest(&in);
      }, [&] {
        dnXPeast(&in);
      });
    }
  });
  return;
}
static void * dnXPwest(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the WESTERN process.*/
    /* Precondition:  either nw(eQuad) or ne(wQuad) or ne(nQuadT) in L1 cache  */
    DNextendSEblkCNTLpar(procs,  nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
    /* SW/SW */
    UPextendSEblkCNTLpar(procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* NE/NE */
    UPextendSEblkCNTLpar(procs,    sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* NW/NW */
    DNextendSEblkCNTLpar(procs,    sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* Postcondition:  each of sw(eQuad), se(wQuad), ne(nQuadT) in L1 cache  */
  }
  return NULL;
}
static void * dnXPeast(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the EASTERN process.*/
    /* Precondition:  something in L1: nw/se(eQuad), ne/se(wQuad), ne/se(eQuad). */
    DNextendSEblkCNTLpar(procs,  se(eQuad),   se(wQuad)     /*,se(nQuadT)*/ );
    /* SW/NE */
    UPextendSEblkCNTLpar(procs,  se(eQuad),   sw(wQuad)   /*,sw(nQuadT)*/   );
    /* NE/SE */
    UPextendSEblkCNTLpar(procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
    /* NW/SE */
    DNextendSEblkCNTLpar(procs,    ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
    /* Postcondition: something in L1:  sw/ne(eQuad), se/ne(wQuad), ne/se(eQuad). */
  }
  return NULL;
}

controller_type UPextendSEblkCNTLpar_contr("UPextendSEblkCNTLpar_contr");

static void * upXPwest(void *args);
static void * upXPeast(void *args);
static void UPextendSEblkCNTLpar(int procs, int eQuad, int wQuad /*,int nQuadT*/){
  par::cstmt(UPextendSEblkCNTLpar_contr, [&] { return complexity(eQuad); }, [&] {
    if (procs==1 || eQuad >= parallelBound) UPextendSEblkCNTL(eQuad, wQuad /*,nQuadT*/);
    else{
      /*  Unordered.    */
      /*    Dovetailed at all 4 joins. */
      pthread_t       tid0, tid1;
      struct arguments in;
      int ec;                 /*error code*/
      
      /* Setup the structures to pass data to the threads. */
      in.c = eQuad;  in.a = wQuad;  in.pr= procs/2;
      
      par::fork2([&] {
        upXPwest(&in);
      }, [&] {
        upXPeast(&in);
      });
    }
  });
  return;
}
static void * upXPwest(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the WESTERN process.*/
    /* Precondition:  either sw(eQuad) or se(wQuad) or ne(nQuadT) in L1 cache  */
    UPextendSEblkCNTLpar(procs,    sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* NW/NW */
    DNextendSEblkCNTLpar(procs,    sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* NE/NE */
    DNextendSEblkCNTLpar(procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* SW/SW */
    UPextendSEblkCNTLpar(procs,  nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
    /* Postcondition:  each of nw(eQuad), ne(wQuad), ne(nQuadT) in L1 cache  */
  }
  return NULL;
}
static void * upXPeast(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the EASTERN process.*/
    /* Precondition:  something in L1:  sw/ne(eQuad), se/ne(wQuad), ne/se(eQuad). */
    UPextendSEblkCNTLpar(procs,    ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
    /* NW/SW */
    DNextendSEblkCNTLpar(procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
    /* NE/SE */
    DNextendSEblkCNTLpar(procs,  se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
    /* SW/NE */
    UPextendSEblkCNTLpar(procs,  se(eQuad), se(wQuad)       /*,se(nQuadT)*/ );
    /* Postcondition: something in L1: nw/se(eQuad), ne/se(wQuad), ne/se(eQuad). */
  }
  return NULL;
}

static void BKextendSEtriCNTLpar(int procs, int eQuad, int wQuad){
  if (procs==1 || eQuad >=parallelBound) BKextendSEtriCNTL(eQuad, wQuad);
  else{
    /*SERIAL*/
    /*  Unordered.    */
    /*    Dovetailed at all 6 joins. */
    /* Pretcondition:  something useful in L1 cache:  nw*se(wQuad), ne*sw(wQuad) */
    WHextendSEtriCNTLpar(procs,  se(eQuad),          sw(wQuad)         );
    /* ? */
    BKextendSEtriCNTLpar(procs,   se(eQuad), se(wQuad)                  );
    /* NE */
    DNextendSEblkCNTLpar(procs, sw(eQuad),   se(wQuad)   /*,ne(wQuad)*/ );
    /* ?SW */
    UPextendSEblkCNTLpar(procs, sw(eQuad), sw(wQuad) /*,nw(wQuad)*/     );
    /* NE */
    WHextendSEtriCNTLpar(procs,   nw(eQuad),            nw(wQuad)       );
    /* ? */
    BKextendSEtriCNTLpar(procs,   nw(eQuad),          ne(wQuad)         );
    /* Postcondition:  both nw(eQuad) and ne(wQuad) in L1 cache  */
  }
  return;
}


static void WHextendSEtriCNTLpar(int procs, int eQuad, int wQuad){
  if (procs==1 || eQuad >=parallelBound) WHextendSEtriCNTL(eQuad, wQuad);
  else{
    /*SERIAL*/
    /*  Unordered.    */
    /*    Dovetailed at all 6 joins. */
    /* Precondition:  either nw(eQuad) or ne(wQuad) in L1 cache  */
    WHextendSEtriCNTLpar(procs,   nw(eQuad),        ne(wQuad)           );
    /* ? */
    BKextendSEtriCNTLpar(procs,   nw(eQuad),            nw(wQuad)       );
    /*NE*/
    DNextendSEblkCNTLpar(procs, sw(eQuad), sw(wQuad) /*,nw(wQuad)*/     );
    /* ?SW */
    UPextendSEblkCNTLpar(procs, sw(eQuad),   se(wQuad)   /*,ne(wQuad)*/ );
    /* NE */
    WHextendSEtriCNTLpar(procs,   se(eQuad), se(wQuad)                  );
    /* ? */
    BKextendSEtriCNTLpar(procs,   se(eQuad),          sw(wQuad)         );
    /* Postcondition:  something useful in L1 cache:   nw*se(wQuad), ne*sw(wQuad) */
  } }

/*---------------------------------------------------------------------*/
/* recursions.c */

static void doCholesky(int quad);
static void dropSW(int sQuad /*,int nQuad*/ );
static void DNextendSEblk(int eQuad, int wQuad  /*,int nQuadT*/);
static void UPextendSEblk(int eQuad, int wQuad  /*,int nQuadT*/);
static void extendSEtri(int eQuad, int wQuad);

static void doCholesky(int quad){
  if (quad >=leafBound)   doCholeskyBase(quad);
  else{
    level++;
    if ((sw(quad)&evenBits) > stripeBound[level]) doCholesky(nw(quad));
    else{
      doCholeskyCNTL (                      nw(quad)   );
      dropSW         (          sw(quad) /*,nw(quad)*/ );
      extendSEtri    (se(quad), sw(quad) );
      doCholesky     (se(quad));
    }
    level--;
  }
  return;
}

static void dropSW(int sQuad /*,int nQuad*/ ){
  if (sQuad >=leafBound)   dropSWbase(sQuad /*, nQuad*/ );
  else{
    level++;
    if ((sw(sQuad)&evenBits) > stripeBound[level]){
      dropSW         (           nw(sQuad)    /*,nw(nQuad)*/); /*A*/
      UPextendSEblk  (ne(sQuad), nw(sQuad) /*,sw(nQuad)*/   ); /*D*/
      dropSW         (ne(sQuad)      /*,se(nQuad)*/         ); /*E*/
    } else{
      BKdropSWcntl     (           nw(sQuad)    /*,nw(nQuad)*/ ); /*A*/
      /* SE */
      UPextendSEblkCNTL(ne(sQuad), nw(sQuad)  /*,sw(nQuad)*/   ); /*D*/
      /* NW */
      WHdropSWcntl     (ne(sQuad)       /*,se(nQuad)*/         ); /*E*/
      dropSW           (           sw(sQuad)    /*,nw(nQuad)*/ ); /*B*/
      UPextendSEblk    (se(sQuad), sw(sQuad)  /*,sw(nQuad)*/   ); /*C*/
      dropSW           (se(sQuad)       /*,se(nQuad)*/         ); /*F*/
    }
    level--;
  }
  return;
}

static void DNextendSEblk(int eQuad, int wQuad /*,int nQuadT*/){
  if (eQuad >=leafBound)   extendSEblkBase(eQuad, wQuad /*, nQuadT*/);
  else{
    level++;
    if ((sw(eQuad)&evenBits) > stripeBound[level]){
      DNextendSEblk( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
      UPextendSEblk( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
      /* ?NE */
      DNextendSEblk(   ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
      UPextendSEblk(   ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
    }else{
      /*  Unordered.    */
      /* Precondition:  either nw(eQuad) or ne(wQuad) or ne(nQuadT) in L1 cache  */
      DNextendSEblkCNTL( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
      /* ?SW */
      UPextendSEblkCNTL( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
      /* ?NE */
      DNextendSEblkCNTL(   ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
      /* ?SW */
      UPextendSEblkCNTL(   ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
      /* NE */
      UPextendSEblk    ( se(eQuad),   se(wQuad)     /*,se(nQuadT)*/ );
      /* ?NW */
      DNextendSEblk    ( se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
      UPextendSEblk    (   sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
      /* ?NW */
      DNextendSEblk    (   sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    }
    level--;
  }
  return;
}

static void UPextendSEblk(int eQuad, int wQuad  /*,int nQuadT*/){
  if (eQuad >=leafBound)   extendSEblkBase(eQuad, wQuad /*, nQuadT*/);
  else{
    level++;
    if ((sw(eQuad)&evenBits) <= stripeBound[level]){
      /*  Unordered.    */
      UPextendSEblk    (   sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
      /* ?NW */
      DNextendSEblk    (   sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
      /* NE */
      UPextendSEblk    ( se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
      /* ?NW */
      DNextendSEblk    ( se(eQuad), se(wQuad)       /*,se(nQuadT)*/ );
      /* NE */
      DNextendSEblkCNTL(   ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
      /* ?SW */
      UPextendSEblkCNTL(   ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
      /* ?NE */
      DNextendSEblkCNTL( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
      /* ?SW */
      UPextendSEblkCNTL( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
      /* Postcondition:  each of nw(eQuad), ne(wQuad), ne(nQuadT) in L1 cache  */
    }else{
      DNextendSEblk    (   ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
      UPextendSEblk    (   ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
      /* ?NE */
      DNextendSEblk    ( nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
      UPextendSEblk    ( nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
    }
    level--;
  }
  return;
}

static void extendSEtri(int eQuad, int wQuad){
  if (eQuad >=leafBound)   extendSEtriBase(/* eQuad,*/ wQuad);
  else{
    level++;
    if ((sw(eQuad)&evenBits) > stripeBound[level]){
      extendSEtri(  nw(eQuad),           ne(wQuad)   );
      extendSEtri(  nw(eQuad),  nw(wQuad)            );
    }else{
      extendSEtri    (  se(eQuad),             se(wQuad)     );
      extendSEtri    (  se(eQuad),  sw(wQuad)                );
      DNextendSEblk  (sw(eQuad),    sw(wQuad) /*,nw(wQuad)*/ );
      UPextendSEblk  (sw(eQuad), se(wQuad) /* ne(wQuad)*/    );
      WHextendSEtriCNTL(  nw(eQuad),          ne(wQuad)      );
      BKextendSEtriCNTL(  nw(eQuad),  nw(wQuad)              );
    }
    level--;
  }
  return;
}

/*---------------------------------------------------------------------*/
/* recursionsParallel.c */

static void doCholeskyPar(int procs, int quad);
static void dropSWpar(int procs, int sQuad /*,int nQuad*/ );
static void DNextendSEblkPar(int procs, int eQuad, int wQuad  /*,int nQuadT*/);
static void UPextendSEblkPar(int procs, int eQuad, int wQuad  /*,int nQuadT*/);
static void extendSEtriPar(int procs, int eQuad, int wQuad);

static void doCholeskyPar(int procs, int quad){
  if (quad >= parallelBound) doCholesky(quad);
  else{
    level++;
    if ((sw(quad)&evenBits) > stripeBound[level])  doCholeskyPar(procs, nw(quad));
    else{
      /*SERIAL*/
      doCholeskyCNTLpar(procs,                       nw(quad)   );
      dropSWpar        (procs,           sw(quad) /*,nw(quad)*/ );
      extendSEtriPar   (procs, se(quad), sw(quad) );
      doCholeskyPar    (procs, se(quad));
    }
    level--;
  }
  return;
}

static void dropSWpar(int procs, int sQuad /*,int nQuad*/ ){
  if (sQuad >= parallelBound) dropSW(sQuad /*,nQuad*/);
  else{
    /*SERIAL*/
    level++;
    if ((sw(sQuad)&evenBits) > stripeBound[level]){
      dropSWpar           (procs,            nw(sQuad)    /*,nw(nQuad)*/);/*A*/
      UPextendSEblkPar    (procs, ne(sQuad), nw(sQuad) /*,sw(nQuad)*/   );/*D*/
      dropSWpar           (procs, ne(sQuad)      /*,se(nQuad)*/         );/*E*/
    }else{
      BKdropSWcntlPAR     (procs,            nw(sQuad)    /*,nw(nQuad)*/ );/*A*/
      /* SE */
      UPextendSEblkCNTLpar(procs, ne(sQuad), nw(sQuad)  /*,sw(nQuad)*/   );/*D*/
      /* NW */
      WHdropSWcntlPAR     (procs, ne(sQuad)       /*,se(nQuad)*/         );/*E*/
      dropSWpar           (procs,            sw(sQuad)    /*,nw(nQuad)*/ );/*B*/
      UPextendSEblkPar    (procs, se(sQuad), sw(sQuad)  /*,sw(nQuad)*/   );/*C*/
      dropSWpar           (procs, se(sQuad)       /*,se(nQuad)*/         );/*F*/
    }
    level--;
  }
  return;
}

static void extendSEtriPar(int procs, int eQuad, int wQuad){
  if (eQuad >= parallelBound) extendSEtri(eQuad, wQuad);
  else{
    /*SERIAL*/
    level++;
    if ((sw(eQuad)&evenBits) > stripeBound[level]){
      extendSEtriPar(procs,   nw(eQuad),           ne(wQuad)   );
      extendSEtriPar(procs,   nw(eQuad),  nw(wQuad)            );
    }else{
      extendSEtriPar      (procs,   se(eQuad),             se(wQuad)    );
      extendSEtriPar      (procs,   se(eQuad),  sw(wQuad)               );
      DNextendSEblkPar    (procs, sw(eQuad),    sw(wQuad) /*,nw(wQuad)*/);
      UPextendSEblkPar    (procs, sw(eQuad), se(wQuad) /* ne(wQuad)*/   );
      WHextendSEtriCNTLpar(procs,   nw(eQuad),          ne(wQuad)       );
      BKextendSEtriCNTLpar(procs,   nw(eQuad),  nw(wQuad)               );
    }
    level--;
  }
  return;
}

/* The original design was that all parallelism would be constrained to be
 *within* the  extendSEblkPar  calls,  where there was perfect balance among
 the recursive calls (measured in both time and space).
 
 In the following, however, parallelism is allowed.  Why?
 First, because there are at least two calls to densely parallel subproblems,
 already to  extendSEblkPar , in each of the two threads.
 So parallelism will occur, but here it can launch a full level up---
 25% of the tasks, each four times bigger to amortize the dispatch/recovery.
 
 Second, because of the EASTERN/WESTERN split, it is possible to balance
 the loads between collateral parallelsim, even though some of
 the calls are recursive to these non-dense functions.
 
 There is *some* dense parallelism and there is good balance, so parallel
 dispatch is provided already here.
 */

controller_type UPextendSEblkPar_contr("UPextendSEblkPar");

static void * upXwest(void *args);
static void * upXeast(void *args);
static void UPextendSEblkPar(int procs, int eQuad, int wQuad  /*,int nQuadT*/){
  par::cstmt(UPextendSEblkPar_contr, [&] { return complexity(eQuad); }, [&] {
    if (procs==1 || eQuad >= parallelBound) UPextendSEblk(eQuad, wQuad /*,nQuadT*/);
    else{      level++;
      if ((sw(eQuad)&evenBits) > stripeBound[level]){
        UPextendSEblkPar(procs,  nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
        DNextendSEblkPar(procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
        UPextendSEblkPar(procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
        DNextendSEblkPar(procs,    ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
      }else{
        /*  Unordered.    */
        pthread_t       tid0, tid1;
        struct arguments in;
        int ec;                 /*error code*/
        
        /* Setup the structures to pass data to the threads. */
        in.c = eQuad;  in.a = wQuad;  in.pr= procs/2;
        par::fork2([&] {
          upXwest(&in);
        }, [&] {
          upXeast(&in);
        });
      }
      level--;
    }
  });
  return;
}

static void * upXwest(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    
    /* Call this the WESTERN process.*/
    /* Postcondition:  ? either sw(eQuad)  or se(wQuad  or  ne(nQuadT) in cache*/
    UPextendSEblkPar    (procs,    sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* NW?NW */
    DNextendSEblkPar    (procs,    sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* NE?NE */
    DNextendSEblkCNTLpar(procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* SW/SW */
    UPextendSEblkCNTLpar(procs,  nw(eQuad),     ne(wQuad) /*,ne(nQuadT)*/   );
    /* Postcondition:  each of        nw(eQuad), ne(wQuad), ne(nQuadT) in L1 cache  */
  }
  return NULL;
}
static void * upXeast(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    
    /* Call this the EASTERN process.*/
    /* Precondition:  something in L1:  sw/ne(eQuad), se/ne(wQuad), ne/se(eQuad). */
    UPextendSEblkCNTLpar(procs,    ne(eQuad),     ne(wQuad) /*,se(nQuadT)*/ );
    /* NW/SW */
    DNextendSEblkCNTLpar(procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT*/    );
    /* NE/SE */
    DNextendSEblkPar    (procs,  se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
    /* SW?NW */
    UPextendSEblkPar    (procs,  se(eQuad),   se(wQuad)     /*,se(nQuadT)*/ );
    /* Postcondition: something in L1: nw/se(eQuad), ne/se(wQuad), ne/se(eQuad). */
  }
  return NULL;
}

controller_type DNextendSEblkPar_contr("DNextendSEblkPar_contr");

static void * dnXwest(void *args);
static void * dnXeast(void *args);
static void DNextendSEblkPar(int procs, int eQuad, int wQuad  /*,int nQuadT*/){
  par::cstmt(DNextendSEblkPar_contr, [&] { return complexity(eQuad); }, [&] {
    if (procs==1 || eQuad >=parallelBound) DNextendSEblk(eQuad, wQuad /*,nQuadT*/);
    else{
      level++;
      if ((sw(eQuad)&evenBits) > stripeBound[level]){
        UPextendSEblkPar    (procs,    ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
        DNextendSEblkPar    (procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
        UPextendSEblkPar    (procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
        DNextendSEblkPar    (procs,  nw(eQuad),   ne(wQuad) /*,ne(nQuadT)*/     );
      }else{
        /*  Unordered.    */
        pthread_t       tid0, tid1;
        struct arguments in;
        int ec;                 /*error code*/
        
        /* Setup the structures to pass data to the threads. */
        in.c = eQuad;  in.a = wQuad;  in.pr= procs/2;
        par::fork2([&] {
          dnXwest(&in);
        }, [&] {
          dnXeast(&in);
        });
      }
      level--;
    }

  });
  return;
}

static void * dnXwest(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the WESTERN process.*/
    /* Precondition:        either nw(eQuad)  or  ne(wQuad or  ne(nQuadT) in cache*/
    DNextendSEblkCNTLpar(procs,  nw(eQuad),     ne(wQuad) /*,ne(nQuadT)*/   );
    /* SW/SW */
    UPextendSEblkCNTLpar(procs,  nw(eQuad),     nw(wQuad)   /*,nw(nQuadT)*/ );
    /* NE/NE */
    UPextendSEblkPar    (procs,    sw(eQuad),   sw(wQuad)   /*,nw(nQuadT)*/ );
    /* NW?NW */
    DNextendSEblkPar    (procs,    sw(eQuad), se(wQuad)   /*,ne(nQuadT)*/   );
    /* Postcondition: something in L1: ?? sw(eQuad) or se(wQuad or ne(nQuadT) */
  }
  return NULL;
}
static void * dnXeast(void *args)
{ struct arguments *in = (struct arguments *) args;
  int eQuad= in->c; int wQuad= in->a; int procs= in->pr;
  {
    /* Call this the EASTERN process.*/
    /* Precondition: ?   something useful is in L1 cache. */
    DNextendSEblkPar    (procs,  se(eQuad), se(wQuad)       /*,se(nQuadT)*/ );
    /* SW/NE */
    UPextendSEblkPar    (procs,  se(eQuad),     sw(wQuad) /*,sw(nQuadT)*/   );
    /* NE/SE */
    UPextendSEblkCNTLpar(procs,    ne(eQuad),   nw(wQuad) /*,sw(nQuadT)*/   );
    /* NW?SE */
    DNextendSEblkCNTLpar(procs,    ne(eQuad), ne(wQuad)     /*,se(nQuadT)*/ );
    /* Postcondition:  something in L1: sw?ne(eQuad), se?ne(wQuad), ne?se(eQuad). */
  }
  return NULL;
}

/*---------------------------------------------------------------------*/
/* cholesky.c */

void cholesky(double * RESTRICT A, int order, int processors)
{
  /* A is stored in Morton order,
   and allocated space is 3*evenDilated(order) at most.
   So relative addresses range
   from  0  to  3*dilated(order)-1  inclusive
   
   Another representation might be as improper
   lower triangular----also in Morton order,
   but that representation remains be investigated.
   
   order  *may* arrive predilated.  Consider this as alternative.
   */
  
  int rowBound = dilateBits(order-1);
  /* Even-dilated index of bottom row of A.
   2 |-> 1 ;   3 |-> 4 ;  4 |-> 5 ;
   improper upper bound on masking of leaves.
   */
  
  int depth = ceilingLg(order);
  tdepth = depth;
  int i;
  int isParallel;
  
  aLeaf     /* =  3* 4^ceiling(lg (order));     */
  = 3 << (2*depth);
  /* Lowest legal Ahnentafel index on a leaf.
   2 |-> 12 ;   3 |-> 48 ;  4 |-> 48 ;
   improper lower bound on Ahnentafel indices at leaves.
   
   Precompute aLeaf/baseSize for test:
   */
  leafBound=aLeaf/baseSize;
  aa = A;  /* Maybe presubtract off aLeaf from addresss here!*/
  
  
  rowMask = (((1<< (2*depth)) -1) &evenBits);
  /* This will mask off the even-dilated, block-relative,
   row index from an Ahnentafel or Morton index of an element.
   */
  colMask = (((1<< (2*depth)) -1) &oddBits);
  /* This will mask off the odd-dilated, block-relative,
   column index from an Ahnentafel or Morton index of an element.
   */
  
  isParallel = (0 <            (depth - baseOrderLg - soloCommitLg));
//  parallelBound = (3 * (1<<(2* (depth - baseOrderLg - soloCommitLg))));
  parallelBound = (3 * (1<<(2* (depth - baseOrderLg))));
//  std::cout << "leafbound = " << leafBound << " par = " << parallelBound << std::endl;
//  assert(false);
  /*Square matrix=> row and col bounds on Ahnentafel.*/
  
  level=0;
  rowBound = rowBound + (1<< (2*depth));
  /* Now it's an *improper* row bound, after Ahnentafel-mask! */
  while (depth >= 0){
    stripeBound[depth--] = rowBound;
    /*rowBound = evenInc(rowBound) >>2;*/
    rowBound = rowBound >>2;
    /*
     These are improper row bounds for the stripe at each level
     of the recursion.
     That is, if we consider SW or SE of ahnentafelIndex (masked)
     and it is properly greater than  rowBound  at the next level
     then there is no southern half of this matrix.
     */
  }
  
  SEblock = order;
  for (i=0; (SEblock&1)==0; i++) SEblock/=2;
  /* i==lg(order(the SE padblock)) */
  if ( (1<<i)==order) SEblock=3;
  else{
    SEblock = 3* dilateBits(order - (1<<i));
    /* Morton index: NWmost scalar in biggest SE block*/
    SEblock = (SEblock+aLeaf) >> (2*i);
    /* Ahnentafel index of biggest SE block */
  }
  
  if (isParallel){
    if (3== SEblock) doCholeskyCNTLpar(processors,3);
    /* could start with doCholeskyParallel when order is a power of two */
    else             doCholeskyPar    (processors,3);
  }else{
    if (3== SEblock) doCholeskyCNTL(3);
    /* could start with doCholeskyCNTL when order is a power of two */
    else             doCholesky    (3);
  }
  return;
}

/*
 * mkMortonSPDMatrix
 * Returns a randon symmetric positive definite matrix.  The method we used to
 * generate this matrix is critical for performance.  If the matrix we give to
 * cholesky is not SPD, there very well could be divide by zero errors.  I know
 * that the Intels have to trap to handle divide by zeros.  This can dramatically
 * damage run times.  So here is how we generate the SPD matrix:
 *    1. fill a[i,i] with all order+1
 *    2. fill a[i,j] = a[j,i] with x where -1.0 < x < 1.0
 *
 */
double *mkMortonSPDMatrix(int order) {
  double *temp;
  int alloc_size,i,j;
  alloc_size = (3*(dilateBits(order-1))+1);
  temp = (double*)valloc(alloc_size*sizeof(double));
  if(temp==NULL) {
    printf("Error: could not allocate space for order %d matrix",order);
    exit(1);
  }
  srand(time(NULL));
  for(i=0;i<order;i++) {
    for(j=0;j<=i;j++) {
      if(i==j) {
        temp[evenDilate(i)+oddDilate(j)] = (double)order+1;
      } else {
        temp[evenDilate(i)+oddDilate(j)] = temp[evenDilate(j)+oddDilate(i)] = ((double)(rand()%100000))/100000.0;
      }
    }
  }
  return temp;
}

// TODO: fix race condition on updates to `level`

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (pasl::sched::experiment exp) {
    int order = pasl::util::cmdline::parse_or_default_int("order", 1000);
    double* mtx = mkMortonSPDMatrix(order);
    exp([&] {
      cholesky(mtx, order, 2);
    });
    free(mtx);
  });
  std::cout << level << std::endl;
  return 0;
}
