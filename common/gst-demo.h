#ifndef _GST_DEMO_H_
#define _GST_DEMO_H_

#if GST_CHECK_VERSION (1,0,0)
#include <gst/audio/gstaudiodecoder.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/audio/gstaudioencoder.h>
#endif

typedef struct _GstFluDemoStatistics {
  gint64 max_duration;
  gint64 decoded_duration;
} GstFluDemoStatistics;

static inline void
gstflu_demo_reset_statistics (GstFluDemoStatistics * stats)
{
  /* 3 minutes case we can't figure out the duration of the clip */
  stats->max_duration = GST_SECOND * 60 * 3;
  stats->decoded_duration = GST_CLOCK_TIME_NONE;
}

static inline void
gstflu_demo_validate_duration (GstFluDemoStatistics * stats, gint64 duration)
{
#if ENABLE_DEMO_PLUGIN
  if (!GST_CLOCK_TIME_IS_VALID (stats->decoded_duration)) {
    stats->max_duration = gst_util_uint64_scale_int (duration, DEMO_PERCENT, 100);
    /* max 30 minutes */
    if (stats->max_duration > (GST_SECOND * 60 * 30))
      stats->max_duration = GST_SECOND * 60 * 30;
    /* min 3 minutes */
    else if (stats->max_duration < (GST_SECOND * 60 * 3))
      stats->max_duration = GST_SECOND * 60 * 3;
    stats->decoded_duration = 0;
  }
#endif
}

static inline void
gstflu_demo_setup_statistics (GstFluDemoStatistics * stats, GstPad * sink)
{
  GstQuery *q;
  GstPad *peer;
  GstFormat fmt;
  gint64 duration;

  gstflu_demo_reset_statistics (stats);

  if (!sink)
    goto done;

  peer = gst_pad_get_peer (sink);
  if (!peer)
    goto done;

  q = gst_query_new_duration (GST_FORMAT_TIME);
  if (!q) goto p_out;

  if (!gst_pad_query (peer, q))
    goto q_out;

  gst_query_parse_duration (q, &fmt, &duration);
  if (fmt == GST_FORMAT_TIME && GST_CLOCK_TIME_IS_VALID ((guint64)duration)) {
    gstflu_demo_validate_duration (stats, duration);
  }

q_out:
  gst_query_unref (q);
p_out:
  gst_object_unref (peer);
done:
  stats->decoded_duration = 0;
}

static inline GstFlowReturn
gstflu_demo_check_buffer (GstFluDemoStatistics * stats, GstPad * sink,
    GstPad * src, GstClockTime duration)
{
#if ENABLE_DEMO_PLUGIN
  GstElement *element;

  element = gst_pad_get_parent_element (src);

  if (GST_STATE_PLAYING != GST_STATE (element)) {
    gst_object_unref (element);
    return GST_FLOW_OK;
  }

  if (!GST_CLOCK_TIME_IS_VALID (stats->decoded_duration)) {
    gstflu_demo_setup_statistics (stats, sink);
    stats->decoded_duration = 0;
  }

  stats->decoded_duration += duration;
  if (G_UNLIKELY (stats->decoded_duration >= stats->max_duration)) {
    GST_ELEMENT_ERROR (element, STREAM, FAILED,
        ("Fluendo decoders terminated playback of this media"
         " stream as this is an evaluation version of Fluendo's"
         " technology. To get a licensed copy of this Fluendo"
         " product please contact sales@fluendo.com."),
         (NULL));
    gst_object_unref (element);
    return GST_FLOW_EOS;
  } else {
    gst_object_unref (element);
    return GST_FLOW_OK;
  }
#else
  return GST_FLOW_OK;
#endif
}

static inline GstFlowReturn
gstflu_demo_push_buffer (GstFluDemoStatistics * stats, GstPad * sink,
    GstPad * src, GstBuffer * buf)
{
  GstFlowReturn ret;
  if ((ret = gstflu_demo_check_buffer (stats, sink, src,
    GST_BUFFER_DURATION (buf))) != GST_FLOW_OK)
    gst_buffer_unref (buf);
  else
    ret = gst_pad_push (src, buf);
  return ret;
}

static inline GstFlowReturn
gstflu_demo_check_video_buffer (GstFluDemoStatistics * stats, GstPad * sink,
    GstPad * src, gint fps_n, gint fps_d)
{
#if ENABLE_DEMO_PLUGIN
  return gstflu_demo_check_buffer (stats, sink, src,
      gst_util_uint64_scale_int (fps_d, GST_SECOND, fps_n));
#else
  return GST_FLOW_OK;
#endif
}

static inline GstFlowReturn
gstflu_demo_check_audio_buffer (GstFluDemoStatistics * stats, GstPad * sink,
    GstPad * src, gsize size, gint rate, gint channels, gint width)
{
#if ENABLE_DEMO_PLUGIN
  gint bs = channels * rate * (width >> 3);

  return gstflu_demo_check_buffer (stats, sink, src,
      gst_util_uint64_scale_int (size, GST_SECOND, bs));
#else
  return GST_FLOW_OK;
#endif
}

#if GST_CHECK_VERSION (1,0,0)

static inline GstFlowReturn
gstflu_demo_finish_audio_decoder_buffer (GstFluDemoStatistics * stats,
    GstAudioDecoder * dec, GstBuffer * buf, gint frames)
{
  GstFlowReturn ret;
  GstAudioInfo *info;

  info = gst_audio_decoder_get_audio_info (dec);
  if ((ret = gstflu_demo_check_audio_buffer (stats,
      GST_AUDIO_DECODER_SINK_PAD (dec), GST_AUDIO_DECODER_SRC_PAD (dec),
      gst_buffer_get_size (buf), GST_AUDIO_INFO_RATE (info),
      GST_AUDIO_INFO_CHANNELS (info), GST_AUDIO_INFO_DEPTH (info)))
       == GST_FLOW_OK)
    ret = gst_audio_decoder_finish_frame (dec, buf, frames);
  else
    gst_buffer_unref (buf);
  return ret;
}

static inline GstFlowReturn
gstflu_demo_check_video_decoder_frame (GstFluDemoStatistics * stats,
    GstVideoDecoder * dec, GstVideoCodecFrame * frame)
{
#if ENABLE_DEMO_PLUGIN
  return gstflu_demo_check_buffer (stats,
      GST_VIDEO_DECODER_SINK_PAD (dec),
      GST_VIDEO_DECODER_SRC_PAD (dec),
      frame->duration);
#else
  return GST_FLOW_OK;
#endif
}

static inline GstFlowReturn
gstflu_demo_finish_video_decoder_frame (GstFluDemoStatistics * stats,
    GstVideoDecoder * dec, GstVideoCodecFrame * frame)
{
  GstFlowReturn ret;

  if ((ret = gstflu_demo_check_video_decoder_frame (stats, dec, frame))
       == GST_FLOW_OK)
    ret = gst_video_decoder_finish_frame (dec, frame);
  else
    gst_video_decoder_drop_frame (dec, frame);
  return ret;
}

static inline GstFlowReturn
gstflu_demo_finish_audio_encoder_frame (GstFluDemoStatistics * stats,
    GstAudioEncoder * enc, GstBuffer * buf, gint samples)
{
  GstFlowReturn ret;
  GstAudioInfo *info;

  info = gst_audio_encoder_get_audio_info (enc);
  if ((ret = gstflu_demo_check_audio_buffer (stats,
      GST_AUDIO_ENCODER_SINK_PAD (enc), GST_AUDIO_ENCODER_SRC_PAD (enc),
      gst_buffer_get_size (buf), GST_AUDIO_INFO_RATE (info),
      GST_AUDIO_INFO_CHANNELS (info), GST_AUDIO_INFO_DEPTH (info)))
       == GST_FLOW_OK)
    ret = gst_audio_encoder_finish_frame (enc, buf, samples);
  else
    gst_buffer_unref (buf);
  return ret;
}

#endif

#endif
