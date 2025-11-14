/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2012>  <support@fluendo.com>
 */

#ifndef __FLUMP3DEC_H__
#define __FLUMP3DEC_H__

#include "gst-compat.h"
#include "gst-fluendo.h"

#if GST_CHECK_VERSION(1,0,0)
#include <gst/audio/gstaudiodecoder.h>
#endif

#include "mp3tl.h"

G_BEGIN_DECLS

#if GST_CHECK_VERSION(1,0,0)
#define AUDIO_SRC_CAPS \
        "audio/x-raw, " \
        "format = (string) " GST_AUDIO_NE (S16) ", " \
        "layout = (string) interleaved, " \
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, " \
        "channels = (int) { 1,2 } "
#else
#define AUDIO_SRC_CAPS \
        "audio/x-raw-int, " \
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, " \
        "channels = (int) { 1,2 }, " \
        "width = (int) 16, " \
        "depth = (int) 16, " \
        "signed = (boolean) true, " \
        "endianness = (int) BYTE_ORDER"
#endif

#define GST_FLUMP3DEC_TYPE \
  (gst_flump3dec_get_type())
#define GST_FLUMP3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_FLUMP3DEC_TYPE,GstFluMp3Dec))
#define GST_FLUMP3DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_FLUMP3DEC_TYPE,GstPluginTemplate))
#define IS_GST_FLUMP3DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_FLUMP3DEC_TYPE))
#define IS_GST_FLUMP3DEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_FLUMP3DEC_TYPE))

typedef struct GstFluMp3Dec GstFluMp3Dec;
typedef struct GstFluMp3DecClass GstFluMp3DecClass;

struct GstFluMp3Dec
{
#if GST_CHECK_VERSION(1,0,0)
  GstAudioDecoder parent;
#else
  GstElement parent;

  /* Pads */
  GstPad *sinkpad;
  GstPad *srcpad;

  /* Segment */
  GstSegment segment;

  /* Adapter */
  GstAdapter *adapter;  

  GstBuffer *pending_frame;

  gboolean need_discont;

  GstClockTime next_ts;
  GstClockTime last_dec_ts;
  GstClockTime in_ts;

  /* VBR tracking */
  guint avg_bitrate;
  guint64 bitrate_sum;
  guint frame_count;

  guint last_posted_bitrate;

  gboolean bad;

  /* Xing header info */
  guint32 xing_flags;
  guint32 xing_frames;
  GstClockTime xing_total_time;
  guint32 xing_bytes;
  guchar xing_seek_table[100];
  guint32 xing_vbr_scale;
#endif

  guint rate;
  guint channels;  
  guint bytes_per_sample;

  /* Decoder library specific */
  Bit_stream_struc *bs;
  mp3tl *dec;
};

struct GstFluMp3DecClass
{
#if GST_CHECK_VERSION(1,0,0)
  GstAudioDecoderClass parent_class;
#else
  GstElementClass parent_class;
#endif
};

GType gst_flump3dec_get_type (void);

G_END_DECLS

#endif
