/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2012>  <support@fluendo.com>
 */

static void gst_flump3dec_init (GstFluMp3Dec * dec);
#define parent_class gst_flump3dec_parent_class
G_DEFINE_TYPE (GstFluMp3Dec, gst_flump3dec, GST_TYPE_AUDIO_DECODER);

static inline void
_cleanup (GstFluMp3Dec * dec)
{
  dec->rate = 0;
  dec->channels = 0;
}

static gboolean
gst_flump3dec_start (GstAudioDecoder * base)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (base);

  GST_DEBUG_OBJECT (dec, "start");
  _cleanup (dec);

  /* call upon legacy upstream byte support (e.g. seeking) */
  gst_audio_decoder_set_estimate_rate (base, TRUE);

  return TRUE;
}

static gboolean
gst_flump3dec_stop (GstAudioDecoder * base)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (base);

  GST_DEBUG_OBJECT (dec, "stop");
  _cleanup (dec);

  return TRUE;
}

static inline void
gst_flump3dec_reset_format (GstFluMp3Dec *dec, const fr_header *mp3hdr)
{
  gint channels, rate;

  rate = mp3hdr->sample_rate;
  channels = mp3hdr->channels;

  /* rate and channels are not supposed to change in a continuous stream,
   * so check this first before doing anything */
  if (!gst_pad_has_current_caps (GST_AUDIO_DECODER_SRC_PAD (dec))
      || dec->channels != channels || dec->rate != rate) {
    GstAudioInfo info;
    static const GstAudioChannelPosition chan_pos[2][2] = {
      {GST_AUDIO_CHANNEL_POSITION_MONO},
      {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
          GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT}
    };

    /* we set the caps even when the pad is not connected so they
     * can be gotten for streaminfo */
    gst_audio_info_init (&info);
    gst_audio_info_set_format (&info,
        GST_AUDIO_FORMAT_S16, rate, channels, chan_pos[channels - 1]);

    gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (dec), &info);

    dec->channels = channels;
    dec->rate = rate;
    dec->bytes_per_sample = (channels * mp3hdr->sample_size) >> 3;
  }
}

static GstFlowReturn
gst_flump3dec_parse (GstAudioDecoder * base, GstAdapter * adapter,
    gint * offset, gint * length)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (base);
  GstFlowReturn res = GST_FLOW_OK;
  gint avail;
  const guint8 *data;  
  gboolean sync, eos;
  *offset = 0;
  *length = 0;

  /* Give data to decoder */
  avail = gst_adapter_available (adapter);
  g_return_val_if_fail (avail > 0, GST_FLOW_ERROR);
  data = gst_adapter_map (adapter, avail);
  bs_set_data (dec->bs, data, avail);

  /* Handle draining on EOS */
  gst_audio_decoder_get_parse_state (base, &sync, &eos);
  GST_DEBUG_OBJECT (dec, "draining, more: %d", !eos);
  mp3tl_set_eos (dec->dec, eos);
  if (!eos) {
    /* Find an mp3 frame */
    if (mp3tl_gather_frame (dec->dec, offset, length) != MP3TL_ERR_OK)
      res = GST_FLOW_EOS;
  } else {
    res = GST_FLOW_EOS;  
  }
  gst_adapter_unmap (adapter);
  return res;
}

static GstFlowReturn
gst_flump3dec_handle_frame (GstAudioDecoder * base, GstBuffer * buffer)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (base);
  GstFlowReturn res = GST_FLOW_OK;
  GstMapInfo map, omap;
  GstBuffer *outbuf;
  Mp3TlRetcode result;
  const fr_header *mp3hdr = NULL;
  gint framesize;

  /* no fancy draining */
  if (G_UNLIKELY (!buffer))
    return GST_FLOW_OK;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  bs_set_data (dec->bs, map.data, map.size);
  result = mp3tl_sync (dec->dec);
  result = mp3tl_decode_header (dec->dec, &mp3hdr);    
  /* Ensure to configure the src caps */
  gst_flump3dec_reset_format (dec, mp3hdr);
  framesize = mp3hdr->frame_samples * dec->bytes_per_sample;

  /* Synthetize the samples */
  outbuf = gst_buffer_new_allocate (NULL, framesize, NULL);    
  if (outbuf) {
    gst_buffer_map (outbuf, &omap, GST_MAP_READWRITE);
    /* Try to decode a frame */
    result = mp3tl_decode_frame (dec->dec, omap.data, omap.size);
    if (result == MP3TL_ERR_BAD_FRAME) {
      /* Fill with silence */
      memset (omap.data, 0, omap.size);
    }
    gst_buffer_unmap (outbuf, &omap);
    gst_buffer_unmap (buffer, &map);

    if (result == MP3TL_ERR_OK) {
      res = gst_audio_decoder_finish_frame (base, outbuf, 1);
    } else if (result != MP3TL_ERR_BAD_FRAME) {
      /* Free up the buffer we allocated above */
      gst_buffer_unref (outbuf);
      res = GST_FLOW_ERROR;
    }
  } else {
    gst_buffer_unmap (buffer, &map);
  }
  
  return res;
}

static void
gst_flump3dec_flush (GstAudioDecoder * base, gboolean hard)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (base);

  mp3tl_flush (dec->dec);
}

static void
gst_flump3dec_dispose (GObject * object)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (object);

  if (dec->dec)
    mp3tl_free (dec->dec);
  dec->dec = NULL;

  if (dec->bs)
    bs_free (dec->bs);
  dec->bs = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_flump3dec_class_init (GstFluMp3DecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstAudioDecoderClass *base_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  base_class = GST_AUDIO_DECODER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_flump3dec_dispose;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_static_metadata (element_class,
      LONGNAME,
      "Codec/Decoder/Audio",
      "Decodes MPEG-1 Layer 1, 2 and 3 streams to raw audio frames",
      "Fluendo Support <support@fluendo.com>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_flump3dec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_flump3dec_stop);
  base_class->parse = GST_DEBUG_FUNCPTR (gst_flump3dec_parse);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_flump3dec_handle_frame);
  base_class->flush = GST_DEBUG_FUNCPTR (gst_flump3dec_flush);
}

static void
gst_flump3dec_init (GstFluMp3Dec * dec)
{
  dec->bs = bs_new ();
  g_return_if_fail (dec->bs != NULL);

  dec->dec = mp3tl_new (dec->bs, MP3TL_MODE_16BIT);
  g_return_if_fail (dec->dec != NULL);
}

