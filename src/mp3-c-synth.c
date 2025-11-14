/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2011>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "mp3tl-priv.h"
#include "table-dewindow.h"
#include "mp3-cos-tables.h"

#define OPT_SYNTH

void
III_subband_synthesis (mp3tl * tl, frame_params * fr_ps,
    gfloat hybridOut[SBLIMIT][SSLIMIT], gint channel,
    short samples[SSLIMIT][SBLIMIT])
{
  gint ss, sb;
  gfloat polyPhaseIn[SBLIMIT];  /* PolyPhase Input. */

  for (ss = 0; ss < 18; ss++) {
    /* Each of the 32 subbands has 18 samples. On each iteration, we take
     * one sample from each subband, (32 samples), and use a 32 point DCT
     * to perform matrixing, and copy the result into the synthesis
     * buffer fifo. */
    for (sb = 0; sb < SBLIMIT; sb++) {
      polyPhaseIn[sb] = hybridOut[sb][ss];
    }

    mp3_SubBandSynthesis (tl, fr_ps, polyPhaseIn, channel,
        &(tl->pcm_sample[channel][ss][0]));
  }
}

#ifndef USE_IPP

#ifdef OPT_SYNTH

#define INV_SQRT_2 (7.071067811865474617150084668537e-01f)

#if defined(USE_ARM_NEON)
static const __CACHE_LINE_DECL_ALIGN(float dct8_k[8]) = {
  INV_SQRT_2, 0.0f, 5.4119610014619701222e-01f, 1.3065629648763763537e+00f,
  5.0979557910415917998e-01f, 6.0134488693504528634e-01f,
  8.9997622313641556513e-01f, 2.5629154477415054814e+00f 
};

STATIC_INLINE void
MPG_DCT_8 (gfloat in[8], gfloat out[8])
{
  __asm__ volatile (
      "vld1.64       {q0-q1}, [%[dct8_k],:128]       \n\t" /* read dct8_k */
      "vld1.64       {q2-q3}, [%[in],:128]           \n\t" /* read in */
      "vrev64.f32    q3, q3                          \n\t"
      "vswp          d6, d7                          \n\t"
      "vadd.f32      q4, q2, q3                      \n\t" /* ei0, ei2, ei3, ei1 */
      "vadd.f32      s28, s16, s19                   \n\t" /* t0 = ei0 + ei1 */
      "vadd.f32      s29, s17, s18                   \n\t" /* t1 = ei2 + ei3 */             
      "vsub.f32      s30, s16, s19                   \n\t" /* t2 = ei0 - ei1 */
      "vsub.f32      s31, s17, s18                   \n\t" /* t3 = ei2 - ei3 */
      "vmul.f32      d15, d15, d1                    \n\t"
      "vsub.f32      q5, q2, q3                      \n\t" /* oi0', oi1', oi2', oi3' */
      "vsub.f32      s27, s30, s31                   \n\t" /* t4' = t2 - t3 */
      "vadd.f32      s24, s28, s29                   \n\t" /* out0 = t0 + t1 */
      "vmul.f32      q5, q5, q1                      \n\t" /* oi0, oi1, oi2, oi3 */
      "vmul.f32      s27, s27, s0                    \n\t" /* out6 = t4 */
      "vadd.f32      s25, s30, s31                   \n\t" /* out2 = t2 + t3 */
      "vadd.f32      s25, s25, s27                   \n\t" /* out2 = t2 + t3 + t4 */
      "vsub.f32      s26, s28, s29                   \n\t" /* out4' = t0 - t1 */
      "vrev64.f32    d11, d11                        \n\t"
      "vmul.f32      s26, s26, s0                    \n\t" /* out4 = (t0 -t1) * INV_SQRT_2 */
      "vadd.f32      d4, d10, d11                    \n\t" /* t0,t1 = oi0 + oi3, oi1 + oi2 */
      "vsub.f32      d5, d10, d11                    \n\t" /* t2',t3' = oi0 - oi3, oi1 - oi3 */
      "vmul.f32      d5, d5, d1                      \n\t" /* t2, t3 */
      "vadd.f32      s12, s10, s11                   \n\t" /* t4 = t2 + t3 */
      "vsub.f32      s13, s10, s11                   \n\t" /* t5' = t2 - t3 */
      "vmul.f32      s31, s13, s0                    \n\t" /* out7 = oo3 = t5 */
      "vadd.f32      s14, s8, s9                     \n\t" /* oo0 = t0 + t1 */
      "vadd.f32      s15, s12, s31                   \n\t" /* oo1 = t4 + t5 */
      "vsub.f32      s16, s8, s9                     \n\t" /* oo2' = t0 - t1 */
      "vmul.f32      s16, s16, s0                    \n\t" /* oo2 */
      "vadd.f32      s28, s14, s15                   \n\t" /* out1 = oo0 + oo1 */
      "vadd.f32      s29, s15, s16                   \n\t" /* out3 = oo1 + oo2 */
      "vadd.f32      s30, s16, s31                   \n\t" /* out5 = oo2 + oo3 */
      "vst2.32       {q6, q7}, [%[out],:128]         \n\t"
    : [in] "+&r" (in),
      [out] "+&r" (out)
    : [dct8_k] "r" (dct8_k)
    : "memory", "cc",
      "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7" 
  );
}
#else
STATIC_INLINE void
MPG_DCT_8 (gfloat in[8], gfloat out[8])
{
  gfloat even_in[4];
  gfloat odd_in[4], odd_out[4];
  gfloat tmp[6];

  /* Even indices */
  even_in[0] = in[0] + in[7];
  even_in[1] = in[3] + in[4];
  even_in[2] = in[1] + in[6];
  even_in[3] = in[2] + in[5];

  tmp[0] = even_in[0] + even_in[1];
  tmp[1] = even_in[2] + even_in[3];
  tmp[2] = (even_in[0] - even_in[1]) * synth_cos64_table[7];
  tmp[3] = (even_in[2] - even_in[3]) * synth_cos64_table[23];
  tmp[4] = (gfloat) ((tmp[2] - tmp[3]) * INV_SQRT_2);

  out[0] = tmp[0] + tmp[1];
  out[2] = tmp[2] + tmp[3] + tmp[4];
  out[4] = (gfloat) ((tmp[0] - tmp[1]) * INV_SQRT_2);
  out[6] = tmp[4];

  /* Odd indices */
  odd_in[0] = (in[0] - in[7]) * synth_cos64_table[3];
  odd_in[1] = (in[1] - in[6]) * synth_cos64_table[11];
  odd_in[2] = (in[2] - in[5]) * synth_cos64_table[19];
  odd_in[3] = (in[3] - in[4]) * synth_cos64_table[27];

  tmp[0] = odd_in[0] + odd_in[3];
  tmp[1] = odd_in[1] + odd_in[2];
  tmp[2] = (odd_in[0] - odd_in[3]) * synth_cos64_table[7];
  tmp[3] = (odd_in[1] - odd_in[2]) * synth_cos64_table[23];
  tmp[4] = tmp[2] + tmp[3];
  tmp[5] = (gfloat) ((tmp[2] - tmp[3]) * INV_SQRT_2);

  odd_out[0] = tmp[0] + tmp[1];
  odd_out[1] = tmp[4] + tmp[5];
  odd_out[2] = (gfloat) ((tmp[0] - tmp[1]) * INV_SQRT_2);
  odd_out[3] = tmp[5];

  out[1] = odd_out[0] + odd_out[1];
  out[3] = odd_out[1] + odd_out[2];
  out[5] = odd_out[2] + odd_out[3];
  out[7] = odd_out[3];
}
#endif

#if defined(USE_ARM_NEON)

static const __CACHE_LINE_DECL_ALIGN(float dct16_k[8]) = {
  5.0241928618815567820e-01f, 5.2249861493968885462e-01f,
  5.6694403481635768927e-01f, 6.4682178335999007679e-01f,
  6.4682178335999007679e-01f, 1.0606776859903470633e+00f,
  1.7224470982383341955e+00f, 5.1011486186891552563e+00f
};

STATIC_INLINE void
MPG_DCT_16 (gfloat in[16], gfloat out[16])
{
  __CACHE_LINE_DECL_ALIGN(gfloat even_in[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat even_out[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_in[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_out[8]);

  __asm__ volatile (
      "vld1.64       {q0-q1}, [%[dct16_k],:128]     \n\t" /* read dct16_k */
      "vld1.64       {q2-q3}, [%[in],:128]!         \n\t" /* read in */
      "vld1.64       {q4-q5}, [%[in],:128]          \n\t" /* read in */
      "vrev64.f32    q4, q4                         \n\t"
      "vrev64.f32    q5, q5                         \n\t"
      "vswp          d8, d9                         \n\t"
      "vswp          d10, d11                       \n\t"
      "vadd.f32      q6, q2, q4                     \n\t"
      "vadd.f32      q7, q3, q5                     \n\t"
      "vst1.64       {q6-q7},  [%[even_in],:128]    \n\t"
      "vsub.f32      q6, q2, q4                     \n\t"
      "vsub.f32      q7, q3, q5                     \n\t"
      "vmul.f32      q6, q6, q0                     \n\t"
      "vmul.f32      q7, q7, q1                     \n\t"
      "vst1.64       {q6-q7},  [%[odd_in],:128]     \n\t"
    : [in] "+&r" (in)
    : [even_in] "r" (even_in), [odd_in] "r" (odd_in), [dct16_k] "r" (dct8_k)
    : "memory", "cc",
      "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7"
  );

  MPG_DCT_8 (even_in, even_out);
  MPG_DCT_8 (odd_in, odd_out);

  __asm__ volatile (
      "vld1.64       {q0-q1}, [%[even_out],:128]    \n\t"
      "vld1.64       {q2-q3}, [%[odd_out],:128]     \n\t"
      "vswp          q1, q2                         \n\t"        
      "vadd.f32      s4, s4, s5                     \n\t"
      "vadd.f32      s5, s5, s6                     \n\t"
      "vadd.f32      s6, s6, s7                     \n\t"
      "vadd.f32      s7, s7, s12                    \n\t"
      "vst2.32       {q0-q1}, [%[out],:128]!        \n\t"
      "vadd.f32      s12, s12, s13                  \n\t"
      "vadd.f32      s13, s13, s14                  \n\t"
      "vadd.f32      s14, s14, s15                  \n\t"
      "vst2.32       {q2-q3}, [%[out],:128]!        \n\t"
    : [out] "+&r" (out)
    : [even_out] "r" (even_out), [odd_out] "r" (odd_out)
    : "memory", "cc",
      "q0", "q1", "q2", "q3"
  );
}
#else
STATIC_INLINE void
MPG_DCT_16 (gfloat in[16], gfloat out[16])
{
  __CACHE_LINE_DECL_ALIGN(gfloat even_in[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat even_out[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_in[8]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_out[8]);
  gfloat a, b;

  a = in[0]; b = in[15];
  even_in[0] = a + b;
  odd_in[0] = (a - b) * synth_cos64_table[1];
  a = in[1]; b = in[14];
  even_in[1] = a + b;
  odd_in[1] = (a - b) * synth_cos64_table[5];
  a = in[2]; b = in[13];
  even_in[2] = a + b;
  odd_in[2] = (a - b) * synth_cos64_table[9];
  a = in[3]; b = in[12];
  even_in[3] = a + b;
  odd_in[3] = (a - b) * synth_cos64_table[13];
  a = in[4]; b = in[11];
  even_in[4] = a + b;
  odd_in[4] = (a - b) * synth_cos64_table[17];
  a = in[5]; b = in[10];
  even_in[5] = a + b;
  odd_in[5] = (a - b) * synth_cos64_table[21];
  a = in[6]; b = in[9];
  even_in[6] = a + b;
  odd_in[6] = (a - b) * synth_cos64_table[25];
  a = in[7]; b = in[9];
  even_in[7] = a + b;
  odd_in[7] = (a - b) * synth_cos64_table[29];

  MPG_DCT_8 (even_in, even_out);
  MPG_DCT_8 (odd_in, odd_out);
  
  out[0] = even_out[0];
  out[1] = odd_out[0] + odd_out[1];  
  out[2] = even_out[1];
  out[3] = odd_out[1] + odd_out[2];  
  out[4] = even_out[2];
  out[5] = odd_out[2] + odd_out[3];  
  out[6] = even_out[3];
  out[7] = odd_out[3] + odd_out[4];  
  out[8] = even_out[4];
  out[9] = odd_out[4] + odd_out[5];  
  out[10] = even_out[5];
  out[11] = odd_out[5] + odd_out[6];  
  out[12] = even_out[6];
  out[13] = odd_out[6] + odd_out[7];  
  out[14] = even_out[7];
  out[15] = odd_out[7];
}
#endif
STATIC_INLINE void
MPG_DCT_32 (gfloat in[32], gfloat out[32])
{
  gint i;
  __CACHE_LINE_DECL_ALIGN(gfloat even_in[16]);
  __CACHE_LINE_DECL_ALIGN(gfloat even_out[16]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_in[16]);
  __CACHE_LINE_DECL_ALIGN(gfloat odd_out[16]);

  for (i = 0; i < 16; i++) {
    even_in[i] = in[i] + in[31 - i];
    odd_in[i] = (in[i] - in[31 - i]) * synth_cos64_table[2 * i];
  }

  MPG_DCT_16 (even_in, even_out);
  MPG_DCT_16 (odd_in, odd_out);

  for (i = 0; i < 15; i++) {
    out[2 * i] = even_out[i];
    out[2 * i + 1] = odd_out[i] + odd_out[i + 1];
  }
  out[30] = even_out[15];
  out[31] = odd_out[15];
}

#if defined(USE_ARM_NEON)

#define WIN_MAC \
  "  vld1.32     {q0-q1}, [r3,:128], r5         \n\t" /* read win */           \
  "  vld1.32     {q2-q3}, [r4,:128], r5         \n\t" /* read uvec */          \
  "  pld         [r3]                           \n\t"                          \
  "  pld         [r4]                           \n\t"                          \
  "  vmla.f32    q4, q0, q2                     \n\t" /* acc += uvec * win */  \
  "  vmla.f32    q5, q1, q3                     \n\t"

STATIC_INLINE void
mp3_dewindow_output (gfloat *uvec, short *samples, gfloat* window)
{
  __asm__ volatile (
      "pld           [%[win]]                       \n\t"
      "pld           [%[uvec]]                      \n\t"
      "mov           r5, #32*4                      \n\t" /* step = 32 floats */
      "mov           ip, #4                         \n\t" /* ip = 4 */
      "0:                                           \n\t"
      "  add         r3, %[win], r5                 \n\t" /* pw = win */
      "  add         r4, %[uvec], r5                \n\t" /* puvec = uvec */
      "  vld1.32     {q0-q1}, [%[win],:128]!        \n\t" /* read win */
      "  vld1.32     {q2-q3}, [%[uvec],:128]!       \n\t" /* read uvec */
      "  pld         [r3]                           \n\t"
      "  pld         [r4]                           \n\t"
      "  vmul.f32    q4, q0, q2                     \n\t" /* acc = uvec * win */
      "  vmul.f32    q5, q1, q3                     \n\t"
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      WIN_MAC
      "  vcvt.s32.f32 q4, q4, #31                   \n\t"
      "  vcvt.s32.f32 q5, q5, #31                   \n\t"
      "  vshrn.s32   d0, q4, #16                    \n\t"
      "  vshrn.s32   d1, q5, #16                    \n\t"
      "  vst1.64     {d0-d1}, [%[samp],:128]!       \n\t"
      "  pld         [%[win]]                       \n\t"
      "  pld         [%[uvec]]                      \n\t"
      "  subs        ip, ip, #1                     \n\t"
      "  bne         0b                             \n\t"

    : [win] "+&r" (window),
      [samp] "+&r" (samples),
      [uvec] "+&r" (uvec)
    :
    : "memory", "cc", "ip", "r3", "r4", "r5",
      "q0", "q1", "q2", "q3", "q4", "q5"
  );
}

#elif defined(USE_ARM_VFP)
#define CONVERT_TO_INTEGER(v,d)                                               \
    "  fcmpezs     " v "                 \n\t"                                \
    "  fmstat                            \n\t"                                \
    "  faddsgt     " d ", " v ", s2      \n\t"  /* v > 0 ? d = v + 0.5 */     \
    "  fsubsle     " d ", " v ", s2      \n\t"  /* v <= 0 ? d = v - 0.5 */    \
    "  ftosizs     " d ", " d "          \n\t"

#define WRITE_FOUR_SAMPLES                                                    \
    "  fmrrs       r4, r5, {s4, s5}      \n\t"                                \
    "  fmrrs       r6, r7, {s6, s7}      \n\t"                                \
    "  ssat        r4, #16, r4           \n\t"                                \
    "  ssat        r5, #16, r5           \n\t"                                \
    "  ssat        r6, #16, r6           \n\t"                                \
    "  ssat        r7, #16, r7           \n\t"                                \
    "  strh        r4, [%[samp], #0]     \n\t"                                \
    "  strh        r5, [%[samp], #2]     \n\t"                                \
    "  strh        r6, [%[samp], #4]     \n\t"                                \
    "  strh        r7, [%[samp], #6]     \n\t"                                \
    "  add         %[samp], %[samp], #8  \n\t"

STATIC_INLINE void
mp3_dewindow_output (gfloat *uvec, short *samples, gfloat* window)
{
  gfloat k[] = { SCALE, 0.5 };

  __asm__ volatile (
      "fldmias       %[k], {s1-s2}        \n\t"
      "fmrx          ip, fpscr            \n\t" /* read fpscr register into arm */
      "mov           r8, #7               \n\t"
      "orr           r8, ip, r8, lsl #16  \n\t" /* set vector lenght to 8 */
      "fmxr          fpscr, r8            \n\t"
      "mov           r8, #4               \n\t" /* i = 4 */
      "0:                                 \n\t"
      "  mov         r6, %[win]           \n\t" /* r6 = &win[0] */
      "  mov         r4, %[uvec]          \n\t" /* r4 = &uvec[0] */
      "  fldmias     %[win]!, {s8-s15}    \n\t" /* win[0..7] */
      "  fldmias     %[uvec]!, {s16-s23}  \n\t" /* uvec[0..7] */
      "  fmuls       s24, s8, s16         \n\t" /* s24..s31 = win[0..7] * uvec[0..7] */
      /* Multiply and accumulate loop */
      "  mov         r9, #15              \n\t" /* j = 15 */
      "1:                                 \n\t"
      "  add         r6, r6, #128         \n\t" /* r6 = &win[32] */
      "  add         r4, r4, #128         \n\t" /* r4 = &uvec[32] */
      "  fldmias     r6, {s8-s15}         \n\t" /* win[32..39] */
      "  fldmias     r4, {s16-s23}        \n\t" /* uvec[32..39] */
      "  fmacs       s24, s8, s16         \n\t" /* s24..s31 += win[32..39] * uvec[32..39] */
      "  subs        r9, r9, #1           \n\t" /* j-- */
      "  bne         1b                   \n\t"
      /* Scale results */
      "  fmuls       s8, s24, s1          \n\t" /* uvec[0..7] *= SCALE */
      /* Convert samples into integers and write them */
      CONVERT_TO_INTEGER("s8","s4")
      CONVERT_TO_INTEGER("s9","s5")
      CONVERT_TO_INTEGER("s10","s6")
      CONVERT_TO_INTEGER("s11","s7")
      WRITE_FOUR_SAMPLES
      CONVERT_TO_INTEGER("s12","s4")
      CONVERT_TO_INTEGER("s13","s5")
      CONVERT_TO_INTEGER("s14","s6")
      CONVERT_TO_INTEGER("s15","s7")
      WRITE_FOUR_SAMPLES
      "  subs        r8, r8, #1           \n\t" /* i-- */
      "  bne         0b                   \n\t"
      "fmxr          fpscr, ip            \n\t" /* restore original fpscr */
    : [win] "+&r" (window),
      [samp] "+&r" (samples),
      [uvec] "+&r" (uvec)
    : [k] "r" (k)
    : "memory", "cc", "r4", "r5", "r6", "r7", "r8", "r9", "ip",
      "s1", "s2", "s4", "s5", "s6", "s7",
      "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15",
      "s16", "s17", "s18", "s19", "s20", "s21", "s22", "s23",
      "s24", "s25", "s26", "s27", "s28", "s29", "s30", "s31"
  );
}
#else
STATIC_INLINE void
mp3_dewindow_output (gfloat *u_vec, short *samples, gfloat* window)
{
  gint i;
  gfloat *u_vec0;

  /* dewindowing */
  for (i = 0; i < HAN_SIZE; i++)
    u_vec[i] *= dewindow[i];

  /* Now calculate 32 samples */
  for (i = 0; i < 32; i++) {
    gfloat sum;
    u_vec0 = u_vec + i;
    sum = u_vec0[1 << 5];
    sum += u_vec0[2 << 5];
    sum += u_vec0[3 << 5];
    sum += u_vec0[4 << 5];
    sum += u_vec0[5 << 5];
    sum += u_vec0[6 << 5];
    sum += u_vec0[7 << 5];
    sum += u_vec0[8 << 5];
    sum += u_vec0[9 << 5];
    sum += u_vec0[10 << 5];
    sum += u_vec0[11 << 5];
    sum += u_vec0[12 << 5];
    sum += u_vec0[13 << 5];
    sum += u_vec0[14 << 5];
    sum += u_vec0[15 << 5];
    u_vec0[0] += sum;
  }

  for (i = 0; i < 32; i++) {
    gfloat sample = u_vec[i];
    if (sample > 0) {
      sample = sample * SCALE + 0.5f;
      if (sample < (SCALE - 1)) {
        samples[i] = (short) (sample);
      } else {
        samples[i] = (short) (SCALE - 1);
      }
    } else {
      sample = sample * SCALE - 0.5f;
      if (sample > -SCALE) {
        samples[i] = (short) (sample);
      } else {
        samples[i] = (short) (-SCALE);
      }
    }
  }
}
#endif

#if defined(USE_ARM_NEON)
STATIC_INLINE void
build_uvec (gfloat *u_vec, gfloat *cur_synbuf, gint k)
{
  __asm__ volatile (
      "mov           ip, #8                         \n\t" /* i = 8 */
      "mov           r5, #512                       \n\t"
      "sub           r5, r5, #1                     \n\t" /* r5 = 511 */
      "veor          d0, d0                         \n\t"
      "0:                                           \n\t"
      "  add         r4, %[k], #16                  \n\t"      
      "  add         r4, %[cur_synbuf], r4, lsl #2  \n\t"
      "  pld         [r4]                           \n\t"
      "  mov         r3, %[u_vec]                   \n\t"
      "  vstr        s0, [r3, #16*4]                \n\t"
      "  vld1.64     {q1-q2}, [r4,:128]!            \n\t"
      "  vld1.64     {q3-q4}, [r4,:128]!            \n\t"
      "  vst1.64     {q1-q2}, [r3,:128]!            \n\t"
      "  vst1.64     {q3-q4}, [r3,:128]!            \n\t"
      "  add         r3, r3, #4                     \n\t"
      "  vneg.f32    q1, q1                         \n\t"
      "  vneg.f32    q2, q2                         \n\t"
      "  vneg.f32    q3, q3                         \n\t"
      "  vneg.f32    q4, q4                         \n\t"
      "  vrev64.f32  q1, q1                         \n\t"
      "  vrev64.f32  q2, q2                         \n\t"
      "  vrev64.f32  q3, q3                         \n\t"
      "  vrev64.f32  q4, q4                         \n\t"
      "  vswp        d2, d3                         \n\t"
      "  vswp        d6, d7                         \n\t"
      "  vswp        d8, d9                         \n\t"                  
      "  vswp        d4, d5                         \n\t"
      "  vst1.64     {q4}, [r3]!                    \n\t"
      "  vst1.64     {q3}, [r3]!                    \n\t"
      "  vst1.64     {q2}, [r3]!                    \n\t"
      "  vst1.64     {q1}, [r3]!                    \n\t"
      "  add         %[k], %[k], #32                \n\t" /* k += 32 */
      "  and         %[k], %[k], r5                 \n\t" /* k &= 511 */
      "  add         r4, %[cur_synbuf], %[k], lsl #2\n\t"
      "  pld         [r4]                           \n\t"
      "  add         r3, %[u_vec], #48*4            \n\t"
      "  vld1.64     {q1-q2}, [r4,:128]!            \n\t"
      "  vld1.64     {q3-q4}, [r4,:128]!            \n\t"
      "  vldr.32     s2, [r4]                       \n\t"
      "  vneg.f32    q1, q1                         \n\t"
      "  vneg.f32    q2, q2                         \n\t"
      "  vneg.f32    q3, q3                         \n\t"
      "  vneg.f32    q4, q4                         \n\t"
      "  vst1.64     {q1-q2}, [r3,:128]!            \n\t"
      "  vst1.64     {q3-q4}, [r3,:128]!            \n\t"
      "  vneg.f32    s2, s2                         \n\t"
      "  add         r3, %[u_vec], #32*4            \n\t"
      "  vrev64.f32  q1, q1                         \n\t"
      "  vrev64.f32  q3, q3                         \n\t"
      "  vrev64.f32  q4, q4                         \n\t"
      "  vrev64.f32  q2, q2                         \n\t"
      "  vswp        d2, d3                         \n\t"
      "  vswp        d6, d7                         \n\t"
      "  vswp        d8, d9                         \n\t"
      "  vswp        d4, d5                         \n\t"
      "  vstmia      r3!, {s2}                      \n\t"
      "  vstmia      r3!, {q4}                      \n\t"
      "  vstmia      r3!, {q3}                      \n\t"
      "  vstmia      r3!, {q2}                      \n\t"
      "  vstmia      r3!, {q1}                      \n\t"
      "  subs        ip, ip, #1                     \n\t" /* i-- */
      "  add         %[u_vec], %[u_vec], #64*4      \n\t"
      "  add         %[k], %[k], #32                \n\t" /* k += 32 */
      "  and         %[k], %[k], r5                 \n\t" /* k &= 511 */
      "  bne         0b                             \n\t"
    : [u_vec] "+&r" (u_vec), [k] "+&r" (k)
    : [cur_synbuf] "r" (cur_synbuf)
    : "memory", "cc", "r3", "r4", "r5", "ip",
      "q0", "q1", "q2", "q3", "q4"
  );
}
#else
STATIC_INLINE void
build_uvec (gfloat *u_vec, gfloat *cur_synbuf, gint k)
{
  gint i, j;

  for (j = 0; j < 8; j++) {
    for (i = 0; i < 16; i++) {
      /* Copy first 32 elements */
      u_vec [i] = cur_synbuf [k + i + 16];
      u_vec [i + 17] = -cur_synbuf [k + 31 - i];
    }
    
    /* k wraps at the synthesis buffer boundary  */
    k = (k + 32) & 511;

    for (i = 0; i < 16; i++) {
      /* Copy next 32 elements */
      u_vec [i + 32] = -cur_synbuf [k + 16 - i];
      u_vec [i + 48] = -cur_synbuf [k + i];
    }
    u_vec [16] = 0;

    /* k wraps at the synthesis buffer boundary  */
    k = (k + 32) & 511;
    u_vec += 64;
  }
}
#endif

/* Synthesis matrixing variant which uses a 32 point DCT */
void
mp3_SubBandSynthesis (mp3tl * tl ATTR_UNUSED, frame_params * fr_ps,
    float *polyPhaseIn, gint channel, short *samples)
{
  gint k;
  gfloat *cur_synbuf = fr_ps->synbuf[channel];
  __CACHE_LINE_DECL_ALIGN(gfloat u_vec[HAN_SIZE]);

  /* Shift down 32 samples in the fifo, which should always leave room */
  k = fr_ps->bufOffset[channel];
  k = (k - 32) & 511;
  fr_ps->bufOffset[channel] = k;

  /* DCT part */
  MPG_DCT_32 (polyPhaseIn, cur_synbuf + k);

  /* Build the U vector */
  build_uvec (u_vec, cur_synbuf, k);

  /* Dewindow and output samples */
  mp3_dewindow_output (u_vec, samples, (gfloat*) dewindow);
}

#else
/* Original sub band synthesis function, for reference */
void
mp3_SubBandSynthesis (mp3tl * tl ATTR_UNUSED, frame_params * fr_ps,
    float *bandPtr, gint channel, short *samples)
{
  int i, j, k;
  float *bufOffsetPtr, sum;
  gint *bufOffset;
  float *synbuf = fr_ps->synbuf[channel];

  bufOffset = fr_ps->bufOffset;

  bufOffset[channel] = (bufOffset[channel] - 64) & 0x3ff;
  /* g_print ("bufOffset[%d] = %d\n", channel, bufOffset[channel]); */

  bufOffsetPtr = synbuf + bufOffset[channel];

  for (i = 0; i < 64; i++) {
    float *f_row = fr_ps->filter[i];
    sum = 0;
    for (k = 0; k < 32; k++)
      sum += bandPtr[k] * f_row[k];
    bufOffsetPtr[i] = sum;
  }

  /*  S(i,j) = D(j+32i) * U(j+32i+((i+1)>>1)*64)  */
  /*  samples(i,j) = MWindow(j+32i) * bufPtr(j+32i+((i+1)>>1)*64)  */
  for (j = 0; j < 32; j++) {
    sum = 0;
    sum += dewindow[j] * synbuf[(j + bufOffset[channel]) & 0x3ff];
    for (i = 64; i < 512; i += 64) {
      k = j + i;
      sum += dewindow[k] * synbuf[(k + i + bufOffset[channel]) & 0x3ff];
      sum +=
          dewindow[k - 32] * synbuf[(k - 32 + i + bufOffset[channel]) & 0x3ff];
    }

    /* Casting truncates towards zero for both positive and negative numbers,
       the result is cross-over distortion,  1995-07-12 shn */
    if (sum > 0) {
      sum = sum * SCALE + 0.5;
      if (sum < (SCALE - 1)) {
        samples[j] = (short) (sum);
      } else {
        samples[j] = (short) (SCALE - 1);
      }
    } else {
      sum = sum * SCALE - 0.5;
      if (sum > -SCALE) {
        samples[j] = (short) (sum);
      } else {
        samples[j] = (short) (-SCALE);
      }
    }
  }
}

#endif
#endif
