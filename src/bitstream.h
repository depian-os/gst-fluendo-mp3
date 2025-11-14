/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2011>  <support@fluendo.com>
 */

/*********************************************************************
* Adapted from dist10 reference code and used under the license therein:
* ISO MPEG Audio Subgroup Software Simulation Group (1996)
* ISO 13818-3 MPEG-2 Audio Decoder - Lower Sampling Frequency Extension
**********************************************************************/

#ifndef __BITSTREAM_H__
#define __BITSTREAM_H__

#include <glib.h>
#include "gst-compat.h"

/* Accumulator optimization on bitstream management */
#define ENABLE_OPT_BS 1

/* Bit stream reader definitions */
#define         MAX_LENGTH      32      /* Maximum length of word written or
                                           read from bit stream */
#define         BS_BYTE_SIZE    8

#if ENABLE_OPT_BS
#define BS_ACUM_SIZE 32
#else
#define BS_ACUM_SIZE 8
#endif

typedef struct BSReader
{
  guint64 bitpos;               /* Number of bits read so far */

  gsize size;                   /* Number of bytes in the buffer list */
  const guint8 *data;           /* Current data buffer */
  guint8 *cur_byte;             /* ptr to the current byte */
  guint8 cur_bit;               /* the next bit to be used in the current byte,
                                 * numbered from 8 down to 1 */
  gsize cur_used;               /* Number of bytes _completely_ consumed out of
                                 * the 'cur buffer' */
} BSReader;

typedef struct Bit_stream_struc
{
  BSReader master;              /* Master tracking position, advanced
                                 * by bs_consume() */
  BSReader read;                /* Current read position, set back to the 
                                 * master by bs_reset() */
} Bit_stream_struc;

/* Create and initialise a new bitstream reader */
Bit_stream_struc *bs_new ();

/* Release a bitstream reader */
void bs_free (Bit_stream_struc * bs);

/* Reset the current read position to the master position */
static inline void
bs_reset (Bit_stream_struc * bs)
{
  memcpy (&bs->read, &bs->master, sizeof (BSReader));
}

/* Reset master and read states */
static inline void
bs_flush (Bit_stream_struc * bs)
{
  g_return_if_fail (bs != NULL);

  bs->master.cur_bit = 8;
  bs->master.size = 0;
  bs->master.cur_used = 0;
  bs->master.cur_byte = NULL;
  bs->master.data = NULL;
  bs->master.bitpos = 0;

  bs_reset (bs);
}

/* Set data as the stream for processing */
gboolean bs_set_data (Bit_stream_struc * bs, const guint8 * data, gsize size);

/* Advance the master position by Nbits */
void bs_consume (Bit_stream_struc * bs, guint32 Nbits);

/* Number of bits available for reading */
static inline guint bs_bits_avail (Bit_stream_struc * bs)
{
  return ((bs->read.size - bs->read.cur_used) * 8 + (bs->read.cur_bit - 8));
}

/* Extract N bytes from the bitstream into the out array. */
void bs_getbytes (Bit_stream_struc * bs, guint8 * out, guint32 N);

/* Advance the read pointer by N bits */
void bs_skipbits (Bit_stream_struc * bs, guint32 N);

/* give number of consumed bytes */
static inline gsize bs_get_consumed (Bit_stream_struc * bs)
{
  return bs->master.cur_used;
}

/* Current bitstream position in bits */
static inline guint64
bs_pos (Bit_stream_struc * bs)
{
  return bs->master.bitpos;
}

/* Current read bitstream position in bits */
static inline guint64
bs_read_pos (Bit_stream_struc * bs)
{
  return bs->read.bitpos;
}

/* Advances the read position to the first bit of next frame or
 * last byte in the buffer when the sync code is not found */
gboolean bs_seek_sync (Bit_stream_struc * bs);

/* Read N bits from the stream */
/* bs - bit stream structure */
/* N  - number of bits to read from the bit stream */
/* v  - output value */
static inline guint32
bs_getbits (Bit_stream_struc * bs, guint32 N)
{
  guint32 val = 0;
  gint j = N;

  g_assert (N <= MAX_LENGTH);

  while (j > 0) {
    gint tmp;
    gint k;
    gint mask;

    /* Move to the next byte if we consumed the current one */
    if (bs->read.cur_bit == 0) {
      bs->read.cur_bit = 8;
      bs->read.cur_used++;
      bs->read.cur_byte++;
    }

    /* Protect against data limit */
    if ((bs->read.cur_used >= bs->read.size)) {
      GST_WARNING ("Attempted to read beyond data");
      /* Return the bits we got so far */
      return val;
    }
    /* Take as many bits as we can from the current byte */
    k = MIN (j, bs->read.cur_bit);

    /* We want the k bits from the current byte, starting from
     * the cur_bit. Mask out the top 'already used' bits, then shift
     * the bits we want down to the bottom */
    mask = (1 << bs->read.cur_bit) - 1;
    tmp = bs->read.cur_byte[0] & mask;

    /* Trim off the bits we're leaving for next time */
    tmp = tmp >> (bs->read.cur_bit - k);

    /* Adjust our tracking vars */
    bs->read.cur_bit -= k;
    j -= k;
    bs->read.bitpos += k;

    /* Put these bits in the right spot in the output */
    val |= tmp << j;
  }

  return val;
}

/* Read 1 bit from the stream */
static inline guint32
bs_get1bit (Bit_stream_struc * bs)
{
  return bs_getbits (bs, 1);
}

/* read the next byte aligned N bits from the bit stream */
static inline guint32
bs_getbits_aligned (Bit_stream_struc * bs, guint32 N)
{
  guint32 align;

  align = bs->read.cur_bit;
  if (align != 8 && align != 0)
    bs_getbits (bs, align);

  return bs_getbits (bs, N);
}

/* Huffman decoder bit buffer decls */
#define HDBB_BUFSIZE 4096

typedef struct
{
  guint avail;
  guint buf_byte_idx;
  guint buf_bit_idx;
#if ENABLE_OPT_BS
  guint remaining;
  guint32 accumulator;
#endif
  guint8 *buf;
} huffdec_bitbuf;

/* Huffman Decoder bit buffer functions */
void h_setbuf (huffdec_bitbuf * bb, guint8 * buf, guint size);
void h_reset (huffdec_bitbuf * bb);

#if ENABLE_OPT_BS
static inline guint32 h_get1bit (huffdec_bitbuf * bb);
static inline void h_flushbits (huffdec_bitbuf * bb, guint N);
#else
#define h_get1bit(bb) (h_getbits ((bb), 1))
#define h_flushbits(bb,bits) (h_getbits ((bb), (bits)))
#endif

/* read N bits from the bit stream */
static inline guint32 h_getbits (huffdec_bitbuf * bb, guint N);

void h_rewindNbits (huffdec_bitbuf * bb, guint N);
static inline void h_byte_align (huffdec_bitbuf * bb);
static inline guint h_bytes_avail (huffdec_bitbuf * bb);

/* Return the current bit stream position (in bits) */
#if ENABLE_OPT_BS
#define h_sstell(bb) ((bb->buf_byte_idx * 8) - bb->buf_bit_idx)
#else
#define h_sstell(bb) ((bb->buf_byte_idx * 8) + (BS_ACUM_SIZE - bb->buf_bit_idx))
#endif

#if ENABLE_OPT_BS
/* This optimizazion assumes that N will be lesser than 32 */
static inline guint32
h_getbits (huffdec_bitbuf * bb, guint N)
{
  guint32 val = 0;
  static guint32 h_mask_table[] = {
    0x00000000,
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
  };

  if (N == 0)
    return 0;

  /* Most common case will be when accumulator has enough bits */
  if (G_LIKELY (N <= bb->buf_bit_idx)) {
    /* first reduce buf_bit_idx by the number of bits that are taken */
    bb->buf_bit_idx -= N;
    /* Displace to right and mask to extract the desired number of bits */
    val = (bb->accumulator >> bb->buf_bit_idx) & h_mask_table[N];
    return (val);
  }

  /* Next cases will be when there's not enough data on the accumulator
     and there's atleast 4 bytes in the */
  if (bb->remaining >= 4) {
    /* First take all remaining bits */
    if (bb->buf_bit_idx > 0)
      val = bb->accumulator & h_mask_table[bb->buf_bit_idx];
    /* calculate how many more bits are required */
    N -= bb->buf_bit_idx;
    /* reload the accumulator */
    bb->buf_bit_idx = 32 - N;   /* subtract the remaining required bits */
    bb->remaining -= 4;

    /* we need reverse the byte order */
#if defined(HAVE_CPU_I386) && !defined (_MSC_VER)
    register guint32 tmp = *((guint32 *) (bb->buf + bb->buf_byte_idx));
  __asm__ ("bswap %0 \n\t": "=r" (tmp):"0" (tmp));
    bb->accumulator = tmp;
#else
    bb->accumulator = (guint32) bb->buf[bb->buf_byte_idx + 3];
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 2]) << 8;
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 1]) << 16;
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 0]) << 24;
#endif
    bb->buf_byte_idx += 4;

    val <<= N;
    val |= (bb->accumulator >> bb->buf_bit_idx) & h_mask_table[N];
    return (val);
  }

  /* Next case when remains less that one word on the buffer */
  if (bb->remaining > 0) {
    /* First take all remaining bits */
    if (bb->buf_bit_idx > 0)
      val = bb->accumulator & h_mask_table[bb->buf_bit_idx];
    /* calculate how many more bits are required */
    N -= bb->buf_bit_idx;
    /* reload the accumulator */
    bb->buf_bit_idx = (bb->remaining * 8) - N;  /* subtract the remaining required bits */

    bb->accumulator = 0;
    /* load remaining bytes into the accumulator in the right order */
    for (; bb->remaining > 0; bb->remaining--) {
      bb->accumulator <<= 8;
      bb->accumulator |= (guint32) bb->buf[bb->buf_byte_idx++];
    }
    val <<= N; 
    val |= (bb->accumulator >> bb->buf_bit_idx) & h_mask_table[N];
    return (val);
  }

  return 0;
}

static inline guint32
h_get1bit (huffdec_bitbuf * bb)
{
  guint32 val = 0;

  /* Most common case will be when accumulator has enough bits */
  if (G_LIKELY (bb->buf_bit_idx > 0)) {
    /* first reduce buf_bit_idx by the number of bits that are taken */
    bb->buf_bit_idx--;
    /* Displace to right and mask to extract the desired number of bits */
    val = (bb->accumulator >> bb->buf_bit_idx) & 0x1;
    return (val);
  }

  /* Next cases will be when there's not enough data on the accumulator
     and there's atleast 4 bytes in the */
  if (bb->remaining >= 4) {
    /* reload the accumulator */
    bb->buf_bit_idx = 31;       /* subtract 1 bit */
    bb->remaining -= 4;

    /* we need reverse the byte order */
#if defined(HAVE_CPU_I386) && !defined (_MSC_VER)
    register guint32 tmp = *((guint32 *) (bb->buf + bb->buf_byte_idx));
  __asm__ ("bswap %0 \n\t": "=r" (tmp):"0" (tmp));
    bb->accumulator = tmp;
#else
    bb->accumulator = (guint32) bb->buf[bb->buf_byte_idx + 3];
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 2]) << 8;
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 1]) << 16;
    bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 0]) << 24;
#endif
    bb->buf_byte_idx += 4;

    val = (bb->accumulator >> bb->buf_bit_idx) & 0x1;
    return (val);
  }

  /* Next case when remains less that one word on the buffer */
  if (bb->remaining > 0) {
    /* reload the accumulator */
    bb->buf_bit_idx = (bb->remaining * 8) - 1;  /* subtract 1 bit  */

    bb->accumulator = 0;
    /* load remaining bytes into the accumulator in the right order */
    for (; bb->remaining > 0; bb->remaining--) {
      bb->accumulator <<= 8;
      bb->accumulator |= (guint32) bb->buf[bb->buf_byte_idx++];
    }

    val = (bb->accumulator >> bb->buf_bit_idx) & 0x1;
    return (val);
  }

  return 0;
}

static inline void
h_flushbits (huffdec_bitbuf * bb, guint N)
{
  guint bits;
  guint bytes;

  if (N < 32) {
    bits = N;
  } else {
    N -= bb->buf_bit_idx;
    bytes = N >> 3;
    bits = N & 0x7;
    bb->buf_byte_idx += bytes;
    bb->remaining -= bytes;
    bb->buf_bit_idx = 0;

    if (bb->remaining >= 4) {
      /* reload the accumulator */
      bb->buf_bit_idx = 32;       /* subtract 1 bit */
      bb->remaining -= 4;

      /* we need reverse the byte order */
  #if defined(HAVE_CPU_I386) && !defined (_MSC_VER)
      register guint32 tmp = *((guint32 *) (bb->buf + bb->buf_byte_idx));
    __asm__ ("bswap %0 \n\t": "=r" (tmp):"0" (tmp));
      bb->accumulator = tmp;
  #else
      bb->accumulator = (guint32) bb->buf[bb->buf_byte_idx + 3];
      bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 2]) << 8;
      bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 1]) << 16;
      bb->accumulator |= (guint32) (bb->buf[bb->buf_byte_idx + 0]) << 24;
  #endif
      bb->buf_byte_idx += 4;
    } else if (bb->remaining > 0) {
      /* reload the accumulator */
      bb->buf_bit_idx = bb->remaining * 8;

      bb->accumulator = 0;
      /* load remaining bytes into the accumulator in the right order */
      for (; bb->remaining > 0; bb->remaining--) {
        bb->accumulator <<= 8;
        bb->accumulator |= (guint32) bb->buf[bb->buf_byte_idx++];
      }
    }
  }

  if (bits)
    h_getbits (bb, bits);
}

#else
static inline guint32
h_getbits (huffdec_bitbuf * bb, guint N)
{
  guint32 val = 0;
  guint j = N;
  guint k, tmp;
  guint mask;

  while (j > 0) {
    if (!bb->buf_bit_idx) {
      bb->buf_bit_idx = 8;
      bb->buf_byte_idx++;
      if (bb->buf_byte_idx > bb->avail) {
        return 0;
      }
    }
    k = MIN (j, bb->buf_bit_idx);

    mask = (1 << (bb->buf_bit_idx)) - 1;
    tmp = bb->buf[bb->buf_byte_idx % HDBB_BUFSIZE] & mask;
    tmp = tmp >> (bb->buf_bit_idx - k);
    val |= tmp << (j - k);
    bb->buf_bit_idx -= k;
    j -= k;
  }
  return (val);
}
#endif

/* If not on a byte boundary, skip remaining bits in this byte */
static inline void
h_byte_align (huffdec_bitbuf * bb)
{
#if ENABLE_OPT_BS
  if ((bb->buf_bit_idx % 8) != 0) {
    bb->buf_bit_idx -= (bb->buf_bit_idx % 8);
  }
#else
  /* If buf_bit_idx == 0 or == 8 then we're already byte aligned.
   * Check by looking at the bottom bits of the byte */
  if (bb->buf_byte_idx <= bb->avail && (bb->buf_bit_idx & 0x07) != 0) {
    bb->buf_bit_idx = 8;
    bb->buf_byte_idx++;
  }
#endif
}

static inline guint
h_bytes_avail (huffdec_bitbuf * bb)
{
  if (bb->avail >= bb->buf_byte_idx) {
    if (bb->buf_bit_idx != 8)
      return bb->avail - bb->buf_byte_idx - 1;
    else
      return bb->avail - bb->buf_byte_idx;
  }

  return 0;
}

#endif
