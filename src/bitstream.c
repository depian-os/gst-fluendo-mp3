/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2011>  <support@fluendo.com>
 */

/*********************************************************************
* Adapted from dist10 reference code and used under the license therein:
* ISO MPEG Audio Subgroup Software Simulation Group (1996)
* ISO 13818-3 MPEG-2 Audio Decoder - Lower Sampling Frequency Extension
**********************************************************************/

/* 
 * 2 Bitstream buffer implementations. 1 reading from a provided
 * data pointer, the other from a fixed size ring buffer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>

#include "common.h"
#include "bitstream.h"

GST_DEBUG_CATEGORY_EXTERN (flump3debug);
#define GST_CAT_DEFAULT flump3debug

/* Create and initialise a new bitstream reader */
Bit_stream_struc *
bs_new ()
{
  Bit_stream_struc *bs;

  bs = g_new0 (Bit_stream_struc, 1);
  g_return_val_if_fail (bs != NULL, NULL);

  bs->master.cur_bit = 8;
  bs->master.size = 0;
  bs->master.cur_used = 0;
  bs->read.cur_bit = 8;
  bs->read.size = 0;
  bs->read.cur_used = 0;
  return bs;
}

/* Release a bitstream reader */
void
bs_free (Bit_stream_struc * bs)
{
  g_return_if_fail (bs != NULL);

  g_free (bs);
}

/* Set data as the stream for processing */
gboolean
bs_set_data (Bit_stream_struc * bs, const guint8 * data, gsize size)
{
  g_return_val_if_fail (bs != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (size != 0, FALSE);
  
  bs->master.data = data;
  bs->master.cur_byte = (guint8 *) data;
  bs->master.size = size;
  bs->master.bitpos = 0;
  bs->master.cur_used = 0;
  bs_reset (bs);
  return TRUE;
}

/* Advance N bits on the indicated BSreader */
static inline void
bs_eat (Bit_stream_struc * bs, BSReader * read, guint32 Nbits)
{
  while (Nbits > 0) {
    gint k;

    /* Check for the data limit */
    if (read->cur_used >= read->size) {
      return;
    }

    if (Nbits < 8 || read->cur_bit != 8) {
      /* Take as many bits as we can from the current byte */
      k = MIN (Nbits, read->cur_bit);

      /* Adjust our tracking vars */
      read->cur_bit -= k;
      Nbits -= k;
      read->bitpos += k;

      /* Move to the next byte if we consumed the current one */
      if (read->cur_bit == 0) {
        read->cur_bit = 8;
        read->cur_used++;
        read->cur_byte++;
      }
    } else {
      /* Take as many bytes as we can from current buffer */
      k = MIN (Nbits / 8, read->size - read->cur_used);

      read->cur_used += k;
      read->cur_byte += k;

      /* convert to bits */
      k *= 8;
      read->bitpos += k;
      Nbits -= k;
    }
  }
}

/* Advance the master position by Nbits */
void
bs_consume (Bit_stream_struc * bs, guint32 Nbits)
{
#if 0
  static gint n = 0;
  GST_DEBUG ("%d Consumed %d bits to end at %" G_GUINT64_FORMAT,
      n++, Nbits, bs_pos (bs) + Nbits);
#endif
  bs_eat (bs, &bs->master, Nbits);
}

/* Advance the read position by Nbits */
void
bs_skipbits (Bit_stream_struc * bs, guint32 Nbits)
{
  bs_eat (bs, &bs->read, Nbits);
}

/* Advances the read position to the first bit of next frame or
 * last byte in the buffer when the sync code is not found */
gboolean
bs_seek_sync (Bit_stream_struc * bs)
{
  gboolean res = FALSE;
  guint8 last_byte;
  guint8 *start_pos;

  /* Align to the start of the next byte */
  if (bs->read.cur_bit != BS_BYTE_SIZE) {
    bs->read.bitpos += (BS_BYTE_SIZE - bs->read.cur_bit);
    bs->read.cur_bit = BS_BYTE_SIZE;
    bs->read.cur_used++;
    bs->read.cur_byte++;
  }

  /* Ensure there's still some data to read */
  if (G_UNLIKELY (bs->read.cur_used >= bs->read.size)) {
    return FALSE;
  }

  start_pos = bs->read.cur_byte;
  while (bs->read.cur_used < bs->read.size - 1) {
    last_byte = bs->read.cur_byte[0];
    bs->read.cur_byte++;
    bs->read.cur_used++;

    if (last_byte == 0xff && bs->read.cur_byte[0] >= 0xe0) {
      /* Found a sync word */
      res = TRUE;
      break;
    }
  }
  /* Update the tracked position in the reader */
  bs->read.bitpos += BS_BYTE_SIZE * (bs->read.cur_byte - start_pos);
  
  if (res) {
    /* Move past the first 3 bits of 2nd sync byte */
    bs->read.cur_bit = 5;
    bs->read.bitpos += 3;
  }

  return res;
}

/* Extract N bytes from the bitstream into the out array. */
void
bs_getbytes (Bit_stream_struc * bs, guint8 * out, guint32 N)
{
  gint j = N;
  gint to_take;

  while (j > 0) {
    /* Move to the next byte if we consumed any bits of the current one */
    if (bs->read.cur_bit != 8) {
      bs->read.cur_bit = 8;
      bs->read.cur_used++;
      bs->read.cur_byte++;
    }

    /* Check for the data limit */
    if (bs->read.cur_used >= bs->read.size) {
      GST_WARNING ("Attempted to read beyond buffer");
      return;
    }

    /* Take as many bytes as we can from the current buffer */
    to_take = MIN (j, (gint) (bs->read.size - bs->read.cur_used));
    memcpy (out, bs->read.cur_byte, to_take);

    out += to_take;
    bs->read.cur_byte += to_take;
    bs->read.cur_used += to_take;
    j -= to_take;
    bs->read.bitpos += (to_take * 8);
  }
}

void
h_setbuf (huffdec_bitbuf * bb, guint8 * buf, guint size)
{
  bb->avail = size;
  bb->buf_byte_idx = 0;
  bb->buf_bit_idx = 8;
  bb->buf = buf;
#if ENABLE_OPT_BS
  if (buf) {
    /* First load of the accumulator, assumes that size >= 4 */
    bb->buf_bit_idx = 32;
    bb->remaining = bb->avail - 4;

    /* we need reverse the byte order */
    bb->accumulator = (guint) buf[3];
    bb->accumulator |= (guint) (buf[2]) << 8;
    bb->accumulator |= (guint) (buf[1]) << 16;
    bb->accumulator |= (guint) (buf[0]) << 24;

    bb->buf_byte_idx += 4;
  } else {
    bb->remaining = 0;
    bb->accumulator = 0;
  }
#endif
}

void
h_reset (huffdec_bitbuf * bb)
{
  h_setbuf (bb, NULL, 0);
}

#if ENABLE_OPT_BS
void
h_rewindNbits (huffdec_bitbuf * bb, guint N)
{
  guint bits = 0;
  guint bytes = 0;
  if (N <= (BS_ACUM_SIZE - bb->buf_bit_idx))
    bb->buf_bit_idx += N;
  else {
    N -= (BS_ACUM_SIZE - bb->buf_bit_idx);
    bb->buf_bit_idx = 0;
    bits = 8 - (N % 8);
    bytes = (N + 8 + BS_ACUM_SIZE) >> 3;
    if (bb->buf_byte_idx >= bytes)
      bb->buf_byte_idx -= bytes;
    else 
      bb->buf_byte_idx = 0;
    bb->remaining += bytes;
    h_getbits (bb, bits);
  }
}
#else
void
h_rewindNbits (huffdec_bitbuf * bb, guint N)
{
  guint32 byte_off;

  byte_off = (bb->buf_bit_idx + N) / 8;

  g_return_if_fail (bb->buf_byte_idx >= byte_off);

  bb->buf_bit_idx += N;

  if (bb->buf_bit_idx >= 8) {
    bb->buf_bit_idx -= 8 * byte_off;
    bb->buf_byte_idx -= byte_off;
  }
}
#endif

