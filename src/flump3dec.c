/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2012>  <support@fluendo.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "flump3dec.h"

GST_DEBUG_CATEGORY (flump3debug);
#define GST_CAT_DEFAULT flump3debug

#ifdef USE_IPP
#define LONGNAME "Fluendo MP3 Decoder (IPP build)"
#else
#define LONGNAME "Fluendo MP3 Decoder (C build)"
#endif

/* static vars */

/* TODO: Add support for MPEG2 multichannel extension */
static GstStaticPadTemplate sink_factory =
  GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS ("audio/mpeg, mpegversion=(int) 1, "
          "layer = (int) [ 1, 3 ]"));

/* TODO: higher resolution 24 bit decoding */
static GstStaticPadTemplate src_factory =
  GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (AUDIO_SRC_CAPS));

#if GST_CHECK_VERSION(1,0,0)
#include "flump3dec-1_0.c"
#else
#include "flump3dec-0_10.c"
#endif

/* Function implementations */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (flump3debug, "flump3dec", 0, "Fluendo MP3 Decoder");

  if (!gst_element_register (plugin, "flump3dec", GST_RANK_PRIMARY,
          gst_flump3dec_get_type ()))
    return FALSE;

  return TRUE;
}

/*
 * FIXME: Fill in the license, requires new enums in GStreamer
 */
FLUENDO_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    "flump3dec", flump3dec, "Fluendo MP3 decoder",
    plugin_init, VERSION, GST_LICENSE_UNKNOWN, "Fluendo MP3 Decoder",
    "http://www.fluendo.com")
