/*
 * FLUENDO S.A.
 * Copyright (C) <2005 - 2012>  <support@fluendo.com>
 */

static void gst_flump3dec_init (GstFluMp3Dec * dec);
#define parent_class gst_flump3dec_parent_class
G_DEFINE_TYPE (GstFluMp3Dec, gst_flump3dec, GST_TYPE_ELEMENT);

static inline void
_cleanup (GstFluMp3Dec * dec)
{
  dec->rate = 0;
  dec->channels = 0;
  dec->next_ts = 0;
  dec->avg_bitrate = 0;
  dec->bitrate_sum = 0;
  dec->frame_count = 0;
  dec->last_posted_bitrate = 0;
  dec->in_ts = GST_CLOCK_TIME_NONE;
  dec->need_discont = TRUE;
  dec->bad = FALSE;
  dec->xing_flags = 0;
  dec->last_dec_ts = GST_CLOCK_TIME_NONE;
}

#define XING_FRAMES_FLAG     0x0001
#define XING_BYTES_FLAG      0x0002
#define XING_TOC_FLAG        0x0004
#define XING_VBR_SCALE_FLAG  0x0008

static inline Mp3TlRetcode
_check_for_xing (GstFluMp3Dec * dec, const fr_header * mp3hdr)
{
  const guint32 xing_id = 0x58696e67;   /* 'Xing' in hex */
  const guint32 info_id = 0x496e666f;   /* 'Info' in hex - found in LAME CBR files */
  const guint XING_HDR_MIN = 8;
  gint xing_offset;

  guint32 read_id;

  if (mp3hdr->version == MPEG_VERSION_1) {      /* MPEG-1 file */
    if (mp3hdr->channels == 1)
      xing_offset = 0x11;
    else
      xing_offset = 0x20;
  } else {                      /* MPEG-2 header */
    if (mp3hdr->channels == 1)
      xing_offset = 0x09;
    else
      xing_offset = 0x11;
  }

  bs_reset (dec->bs);

  if (bs_bits_avail (dec->bs) < 8 * (xing_offset + XING_HDR_MIN)) {
    GST_DEBUG ("Not enough data to read Xing header");
    return MP3TL_ERR_NEED_DATA;
  }

  /* Read 4 bytes from the frame at the specified location */
  bs_skipbits (dec->bs, 8 * xing_offset);
  read_id = bs_getbits (dec->bs, 32);
  if (read_id == xing_id || read_id == info_id) {
    guint32 xing_flags;
    guint bytes_needed = 0;

    /* Read 4 base bytes of flags, big-endian */
    xing_flags = bs_getbits (dec->bs, 32);
    if (xing_flags & XING_FRAMES_FLAG)
      bytes_needed += 4;
    if (xing_flags & XING_BYTES_FLAG)
      bytes_needed += 4;
    if (xing_flags & XING_TOC_FLAG)
      bytes_needed += 100;
    if (xing_flags & XING_VBR_SCALE_FLAG)
      bytes_needed += 4;
    if (bs_bits_avail (dec->bs) < 8 * bytes_needed) {
      GST_DEBUG ("Not enough data to read Xing header (need %d)", bytes_needed);
      return MP3TL_ERR_NEED_DATA;
    }

    GST_DEBUG ("Reading Xing header");
    dec->xing_flags = xing_flags;

    if (xing_flags & XING_FRAMES_FLAG) {
      dec->xing_frames = bs_getbits (dec->bs, 32);
      dec->xing_total_time = gst_util_uint64_scale (GST_SECOND,
          (guint64) (dec->xing_frames) * (mp3hdr->frame_samples),
          mp3hdr->sample_rate);
    } else {
      dec->xing_frames = 0;
      dec->xing_total_time = 0;
    }

    if (xing_flags & XING_BYTES_FLAG)
      dec->xing_bytes = bs_getbits (dec->bs, 32);
    else
      dec->xing_bytes = 0;

    if (xing_flags & XING_TOC_FLAG) {
      gint i;
      for (i = 0; i < 100; i++)
        dec->xing_seek_table[i] = bs_getbits (dec->bs, 8);
    } else {
      memset (dec->xing_seek_table, 0, 100);
    }

    if (xing_flags & XING_VBR_SCALE_FLAG)
      dec->xing_vbr_scale = bs_getbits (dec->bs, 32);
    else
      dec->xing_vbr_scale = 0;

    GST_DEBUG ("Xing header reported %u frames, time %" G_GUINT64_FORMAT
        ", vbr scale %u\n", dec->xing_frames,
        dec->xing_total_time, dec->xing_vbr_scale);
  } else {
    GST_DEBUG ("No Xing header found");
    bs_reset (dec->bs);
  }

  return MP3TL_ERR_OK;
}

static inline void
_update_ts (GstFluMp3Dec * dec, GstClockTime new_ts, const fr_header * mp3hdr)
{
  GstClockTimeDiff diff;
  GstClockTime out_ts = dec->next_ts;
  GstClockTime frame_dur = gst_util_uint64_scale (GST_SECOND,
      mp3hdr->frame_samples, mp3hdr->sample_rate);

  /* Only take the new timestamp if it is more than half a frame from
   * our current timestamp */
  if (GST_CLOCK_TIME_IS_VALID (out_ts)) {
    diff = ABS ((GstClockTimeDiff) (new_ts - out_ts));
    if ((GstClockTime) diff > (frame_dur / 2)) {
      GST_DEBUG_OBJECT (dec, "Got frame with new TS %"
          GST_TIME_FORMAT " - using.", GST_TIME_ARGS (new_ts));
      out_ts = new_ts;
    }
  }

  dec->next_ts = out_ts;
}

static inline gboolean
_conv_bytes_to_time (GstFluMp3Dec * dec, gint64 byteval, GstClockTime * timeval)
{
  if (dec->avg_bitrate == 0 || timeval == NULL)
    return FALSE;

  if (byteval == -1)
    *timeval = -1;
  else
    *timeval = gst_util_uint64_scale (GST_SECOND, byteval * 8,
        dec->avg_bitrate);

  return TRUE;
}

static inline gboolean
_total_bytes (GstFluMp3Dec * dec, guint64 * total)
{
  GstQuery *query;
  GstPad *peer;

  if ((peer = gst_pad_get_peer (dec->sinkpad)) == NULL)
    return FALSE;

  query = gst_query_new_duration (GST_FORMAT_BYTES);
  gst_query_set_duration (query, GST_FORMAT_BYTES, -1);

  if (!gst_pad_query (peer, query)) {
    gst_object_unref (peer);
    return FALSE;
  }

  gst_object_unref (peer);

  gst_query_parse_duration (query, NULL, (gint64 *) total);

  return TRUE;
}

static inline gboolean
_total_time (GstFluMp3Dec * dec, GstClockTime * total)
{
  /* If we have a Xing header giving us the total number of frames,
   * use that to get total time */
  if (dec->xing_flags & XING_FRAMES_FLAG) {
    *total = dec->xing_total_time;
  } else {
    /* Calculate time from our bitrate */
    if (!_total_bytes (dec, total))
      return FALSE;

    if (*total != (guint64) - 1 && !_conv_bytes_to_time (dec, *total, total))
      return FALSE;
  }

  return TRUE;
}

static inline gboolean
_time_to_bytepos (GstFluMp3Dec * dec, GstClockTime ts, gint64 * bytepos)
{
  /* 0 always maps to 0, and -1 to -1 */
  if (ts == 0 || ts == (GstClockTime) (-1)) {
    *bytepos = (gint64) ts;
    return TRUE;
  }

  /* If we have a Xing seek table, determine a byte offset to seek to */
  if (dec->xing_flags & XING_TOC_FLAG) {
    gdouble new_pos_percent;
    gint int_percent;
    guint64 total;
    gdouble fa, fb, fx;

    if (!_total_time (dec, &total))
      return FALSE;

    /* We need to know what point in the file to go to, from 0-100% */
    new_pos_percent = (100.0 * ts) / total;
    new_pos_percent = CLAMP (new_pos_percent, 0.0, 100.0);

    int_percent = CLAMP ((int) (new_pos_percent), 0, 99);

    fa = dec->xing_seek_table[int_percent];
    if (int_percent < 99)
      fb = dec->xing_seek_table[int_percent + 1];
    else
      fb = 256.0;
    fx = fa + (fb - fa) * (new_pos_percent - int_percent);

    if (!_total_bytes (dec, &total))
      return FALSE;

    *bytepos = (gint64) ((fx / 256.0) * total);
    GST_DEBUG ("Xing seeking for %g percent time mapped to %g in bytes\n",
        new_pos_percent, fx * 100.0 / 256.0);
  } else {
    /* Otherwise, convert to bytes using bitrate and send upstream */
    if (dec->avg_bitrate == 0)
      return FALSE;

    *bytepos = gst_util_uint64_scale (ts, dec->avg_bitrate, (8 * GST_SECOND));
  }

  return TRUE;
}

static void
gst_flump3dec_flush (GstFluMp3Dec * dec)
{
  dec->last_dec_ts = GST_CLOCK_TIME_NONE;
  dec->in_ts = GST_CLOCK_TIME_NONE;

  mp3tl_flush (dec->dec);
  if (dec->pending_frame) {
    gst_buffer_unref (dec->pending_frame);
    dec->pending_frame = NULL;
  }

  dec->need_discont = TRUE;
  gst_adapter_clear (dec->adapter);
}

static GstFlowReturn
gst_flump3dec_decode (GstFluMp3Dec * dec, GstClockTime dec_ts,
    gboolean more_data)
{
  Mp3TlRetcode result;
  const fr_header *mp3hdr = NULL;
  GstBuffer *out_buf = NULL;
  GstFlowReturn res = GST_FLOW_OK;
  GstTagList *taglist = NULL;
  guint8 *out_data;
  gsize out_size;
  gsize frame_size;

  GST_DEBUG_OBJECT (dec, "draining, more: %d", more_data);

  mp3tl_set_eos (dec->dec, !more_data);

  while (bs_bits_avail (dec->bs) > 0) {
    /* Find an mp3 header */
    result = mp3tl_sync (dec->dec);
    if (result != MP3TL_ERR_OK)
      break;                    /* Need more data */

    result = mp3tl_decode_header (dec->dec, &mp3hdr);
    if (result != MP3TL_ERR_OK) {
      if (result == MP3TL_ERR_NEED_DATA)
        break;                  /* Need more data */
      else if (result == MP3TL_ERR_STREAM)
        continue;               /* Resync */
      else {
        /* Fatal decoder error */
        res = GST_FLOW_ERROR;
        goto decode_error;
      }
    }

    if (!mp3hdr) {
      /* Fatal decoder error */
      res = GST_FLOW_ERROR;
      goto decode_error;
    }

    if (dec->frame_count == 0) {
      gchar *codec;
      guint ver;
      /* For the first frame in the file, look for a Xing frame after 
       * the header */

      /* Set codec tag */
      if (mp3hdr->version == MPEG_VERSION_1)
        ver = 1;
      else
        ver = 2;

      if (mp3hdr->layer == 3) {
        codec = g_strdup_printf ("MPEG %d Audio, Layer %d (MP3)",
            ver, mp3hdr->layer);
      } else {
        codec = g_strdup_printf ("MPEG %d Audio, Layer %d", ver, mp3hdr->layer);
      }
      if (codec) {
        taglist = gst_tag_list_new ();
        gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE,
            GST_TAG_AUDIO_CODEC, codec, NULL);
        gst_element_found_tags_for_pad (GST_ELEMENT_CAST (dec),
            dec->srcpad, taglist);
      }
      g_free (codec);
      /* end setting the tag */

      GST_DEBUG ("Checking first frame for Xing VBR header");
      result = _check_for_xing (dec, mp3hdr);

      if (result == MP3TL_ERR_NEED_DATA)
        break;
    }

    dec->bitrate_sum += mp3hdr->bitrate;
    dec->frame_count++;

    /* Round the bitrate to the nearest kbps */
    dec->avg_bitrate = (guint)
        (dec->bitrate_sum / dec->frame_count + 500);
    dec->avg_bitrate -= dec->avg_bitrate % 1000;

    /* Change the output caps based on the header */
    if ((mp3hdr->sample_rate != dec->rate || mp3hdr->channels != dec->channels)) {
      GstCaps *caps;

      caps = gst_caps_new_simple ("audio/x-raw-int",
          "endianness", G_TYPE_INT, G_BYTE_ORDER,
          "signed", G_TYPE_BOOLEAN, TRUE,
          "width", G_TYPE_INT, 16,
          "depth", G_TYPE_INT, 16,
          "rate", G_TYPE_INT, mp3hdr->sample_rate,
          "channels", G_TYPE_INT, mp3hdr->channels, NULL);

      GST_DEBUG_OBJECT (dec, "Caps change, rate: %d->%d channels %d->%d",
          dec->rate, mp3hdr->sample_rate, dec->channels, mp3hdr->channels);
      if (!gst_pad_set_caps (dec->srcpad, caps)) {
        gst_caps_unref (caps);
        goto negotiate_error;
      }
      gst_caps_unref (caps);

      dec->rate = mp3hdr->sample_rate;
      dec->channels = mp3hdr->channels;
      dec->bytes_per_sample = mp3hdr->channels * mp3hdr->sample_size / 8;
    }

    /* Check whether the buffer has enough bits to decode the frame
     * minus the header that was already consumed */
    if (bs_bits_avail (dec->bs) < mp3hdr->frame_bits - 32) {
      GST_INFO ("Need %" G_GINT64_FORMAT " more bits to decode this frame",
          (gint64) ((mp3hdr->frame_bits - 32) - (bs_bits_avail (dec->bs))));
      break;                    /* Go get more data */
    }

    /* We have enough bytes in the store, decode a frame */
    frame_size = mp3hdr->frame_samples * dec->bytes_per_sample;
    res = gst_pad_alloc_buffer (dec->srcpad, GST_BUFFER_OFFSET_NONE,
        frame_size, GST_PAD_CAPS (dec->srcpad), &out_buf);
    if (res != GST_FLOW_OK) {
      goto alloc_error;
    }
    out_data = GST_BUFFER_DATA (out_buf);
    out_size = GST_BUFFER_SIZE (out_buf);

    /* Try to decode a frame */
    result = mp3tl_decode_frame (dec->dec, out_data, out_size);

    if (result != MP3TL_ERR_OK) {
      /* Free up the buffer we allocated above */
      gst_buffer_unref (out_buf);
      out_buf = NULL;

      if (result == MP3TL_ERR_NEED_DATA) {
        /* Should never happen, since we checked we had enough bits */
        GST_WARNING_OBJECT (dec,
            "Decoder requested more data than it said it needed!");
        break;
      } else if (result == MP3TL_ERR_BAD_FRAME) {
        /* Update time, and repeat the previous frame if we have one */
        dec->bad = TRUE;

        if (dec->pending_frame != NULL) {
          GST_DEBUG_OBJECT (dec, "Bad frame - using previous frame");
          out_buf = gst_buffer_create_sub (dec->pending_frame, 0,
              GST_BUFFER_SIZE (dec->pending_frame));
          if (out_buf == NULL) {
            res = GST_FLOW_ERROR;
            goto no_buffer;
          }

          gst_buffer_set_caps (out_buf, GST_BUFFER_CAPS (dec->pending_frame));
        } else {
          GST_DEBUG_OBJECT (dec, "Bad frame - no existing frame. Skipping");
          dec->next_ts +=
              gst_util_uint64_scale (GST_SECOND, mp3hdr->frame_samples,
              mp3hdr->sample_rate);
          continue;
        }
      } else {
        res = GST_FLOW_ERROR;
        goto decode_error;
      }
    }

    /* Got a good frame */
    dec->bad = FALSE;

    /* Set the bitrate tag if changed (only care about changes over 10kbps) */
    if ((dec->last_posted_bitrate / 10240) != (dec->avg_bitrate / 10240)) {
      dec->last_posted_bitrate = dec->avg_bitrate;
      taglist = gst_tag_list_new ();
      gst_tag_list_add (taglist, GST_TAG_MERGE_REPLACE, GST_TAG_BITRATE,
          dec->last_posted_bitrate, NULL);
      gst_element_found_tags_for_pad (GST_ELEMENT (dec), dec->srcpad, taglist);
    }

    if (GST_CLOCK_TIME_IS_VALID (dec_ts) && dec_ts != dec->last_dec_ts) {
      /* Use the new timestamp now, and store it so we don't repeat it. */
      _update_ts (dec, dec_ts, mp3hdr);
      dec->last_dec_ts = dec_ts;
    }

    if (G_UNLIKELY (dec->need_discont)) {
      GST_BUFFER_FLAG_SET (out_buf, GST_BUFFER_FLAG_DISCONT);
      dec->need_discont = FALSE;
    }

    GST_BUFFER_TIMESTAMP (out_buf) = dec->next_ts;
    GST_BUFFER_DURATION (out_buf) = gst_util_uint64_scale (GST_SECOND,
        mp3hdr->frame_samples, mp3hdr->sample_rate);
    /* Next_ts is used to generate a continuous serie of timestamps and 
     * handle resynchronizations on original timestamps */
    dec->next_ts += GST_BUFFER_DURATION (out_buf);
    /* The in_ts is used to detect invalid DISCONT flags */
    dec->in_ts += GST_BUFFER_DURATION (out_buf);

    GST_DEBUG_OBJECT (dec, "Have new buffer, size %" G_GSIZE_FORMAT ", ts %"
        GST_TIME_FORMAT, out_size,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (out_buf)));

    if (dec->pending_frame == NULL) {
      GST_DEBUG_OBJECT (dec, "Storing as pending frame");
      dec->pending_frame = out_buf;
    } else {
      /* push previous frame, queue current frame. */
      GST_DEBUG_OBJECT (dec, "Pushing previous frame, ts %"
          GST_TIME_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (dec->pending_frame)));

      res = gst_pad_push (dec->srcpad, dec->pending_frame);
      dec->pending_frame = out_buf;
      if (res != GST_FLOW_OK)
        goto beach;
    }
  }

  /* Might need to flush out the pending decoded frame */
  if (!more_data && (dec->pending_frame != NULL)) {
    GST_DEBUG_OBJECT (dec, "Pushing pending frame");
    res = gst_pad_push (dec->srcpad, dec->pending_frame);
    dec->pending_frame = NULL;
  }

beach:
  return res;

alloc_error:
  GST_DEBUG_OBJECT (dec, "Couldn't allocate a buffer, skipping decode");
  /* Peer didn't want the buffer */
  mp3tl_skip_frame (dec->dec);
  if (GST_CLOCK_TIME_IS_VALID (dec_ts) && dec_ts != dec->last_dec_ts) {
    /* Use the new timestamp now, and store it so we don't repeat it. */
    _update_ts (dec, dec_ts, mp3hdr);
    dec->last_dec_ts = dec_ts;
  }
  return res;

no_buffer:
  if (res <= GST_FLOW_NOT_NEGOTIATED) {
    GST_ELEMENT_ERROR (dec, RESOURCE, FAILED, (NULL),
        ("Failed to allocate output buffer: reason %s",
            gst_flow_get_name (res)));
  }
  return res;

decode_error:
  {
    const char *reason = mp3tl_get_err_reason (dec->dec);

    /* Set element error */
    if (reason)
      GST_ELEMENT_ERROR (dec, RESOURCE, FAILED, (NULL),
          ("Failed in mp3 stream decoding: %s", reason));
    else
      GST_ELEMENT_ERROR (dec, RESOURCE, FAILED, (NULL),
          ("Failed in mp3 stream decoding: Unknown reason"));
    return res;
  }

negotiate_error:
  GST_ELEMENT_ERROR (dec, CORE, NEGOTIATION, (NULL), (NULL));
  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_flump3dec_sink_chain (GstPad * pad, GstBuffer * buffer)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (gst_pad_get_parent (pad));
  GstFlowReturn res = GST_FLOW_OK;
  GstClockTime pts, duration;
  gboolean discont;
  gsize avail;

  discont = GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  pts = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);

  /* check that we really have a discont */
  if (G_UNLIKELY (discont)) {
    if (GST_CLOCK_TIME_IS_VALID (pts) && GST_CLOCK_TIME_IS_VALID (dec->in_ts)
        && pts == dec->in_ts) {
      GST_DEBUG_OBJECT (dec, "Ignoring discontinuity flag, not needed");
    } else {
      /* We flush on disconts */
      GST_DEBUG_OBJECT (dec, "this buffer has a DISCONT flag, flushing");
      gst_flump3dec_decode (dec, GST_CLOCK_TIME_NONE, FALSE);
      gst_flump3dec_flush (dec);
    }
  }

  /* We've got a new buffer. Decode it! */
  GST_DEBUG_OBJECT (dec, "New input buffer with TS %" GST_TIME_FORMAT,
      GST_TIME_ARGS (pts));
  gst_adapter_push (dec->adapter, buffer);

  /* Store the new timestamp if it's valid. The input timestamp is tracked to
   * detect invalid DISCONT flags */ 
  if (GST_CLOCK_TIME_IS_VALID (pts))
    dec->in_ts = pts;

  /* Give data to the decoder */
  if ((avail = gst_adapter_available (dec->adapter))) {
    const guint8 *data = gst_adapter_peek (dec->adapter, avail);
    guint consumed;

    bs_set_data (dec->bs, data, avail);

    res = gst_flump3dec_decode (dec, pts, TRUE);

    if (GST_CLOCK_TIME_IS_VALID (pts) &&
        GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DURATION (buffer)))
      dec->in_ts = pts + GST_BUFFER_DURATION (buffer);

    if ((consumed = bs_get_consumed (dec->bs))) {
      GST_DEBUG_OBJECT (dec, "consumed %u bytes", consumed);
      gst_adapter_flush (dec->adapter, consumed);
    }
  }

  /* Update the in_ts with the duration if it was provided */
  if (GST_CLOCK_TIME_IS_VALID (duration)) {
    dec->in_ts += pts + duration;
  }

  gst_object_unref (dec);
  return res;
}

/* Handle incoming events on the sink pad */
static gboolean
gst_flump3dec_sink_event (GstPad * pad, GstEvent * event)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (gst_pad_get_parent (pad));
  gboolean res = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gdouble rate;
      GstFormat format;
      gint64 start, end, base;
      gboolean update;
      gboolean converted = FALSE;

      gst_event_parse_new_segment (event, &update, &rate, &format,
          &start, &end, &base);

      GST_DEBUG_OBJECT (dec, "new segment, format=%d, base=%"
          G_GINT64_FORMAT ", start=%" G_GINT64_FORMAT ", end=%"
          G_GINT64_FORMAT, format, base, start, end);

      if (!update)
        gst_flump3dec_decode (dec, GST_CLOCK_TIME_NONE, FALSE);

      if (format == GST_FORMAT_BYTES) {
        GstClockTime disc_start, disc_end, disc_base;

        if (_conv_bytes_to_time (dec, start, &disc_start) &&
            _conv_bytes_to_time (dec, end, &disc_end) &&
            _conv_bytes_to_time (dec, base, &disc_base)) {
          gst_event_unref (event);
          event = gst_event_new_new_segment (FALSE, rate, GST_FORMAT_TIME,
              disc_start, disc_end, disc_base);
          dec->next_ts = disc_start;

          GST_DEBUG_OBJECT (dec, "Converted to TIME, base=%"
              G_GINT64_FORMAT ", start=%" G_GINT64_FORMAT ", end=%"
              G_GINT64_FORMAT, disc_base, disc_start, disc_end);
          converted = TRUE;
        }
      } else if (format == GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (dec, "Got segment in time format");
        dec->next_ts = start;
      }

      if (!converted && format != GST_FORMAT_TIME) {
        gst_event_unref (event);
        GST_DEBUG_OBJECT (dec, "creating new time segment");
        event = gst_event_new_new_segment (FALSE, rate, GST_FORMAT_TIME,
            0, GST_CLOCK_TIME_NONE, 0);
        dec->next_ts = 0;
      }
    }
    case GST_EVENT_FLUSH_STOP:
      gst_flump3dec_flush (dec);
      break;
    case GST_EVENT_EOS:
      /* Output any remaining frames */
      gst_flump3dec_decode (dec, GST_CLOCK_TIME_NONE, FALSE);
      break;
    default:
      break;
  }

  res = gst_pad_push_event (dec->srcpad, event);

  gst_object_unref (dec);
  return res;
}

static gboolean
gst_flump3dec_src_convert (GstFluMp3Dec * dec, GstFormat src_format,
    gint64 src_value, GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = FALSE;

  g_return_val_if_fail (dec != NULL, FALSE);

  /* 0 always maps to 0, and -1 to -1 */
  if (src_value == 0 || src_value == -1) {
    *dest_value = src_value;
    return TRUE;
  }

  if (dec->rate == 0 || dec->bytes_per_sample == 0) {
    gst_object_unref (dec);
    return FALSE;
  }

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:  /* Bytes to time */
          *dest_value = gst_util_uint64_scale (src_value, GST_SECOND,
              (guint64) (dec->bytes_per_sample) * dec->rate);
          res = TRUE;
          break;
        case GST_FORMAT_DEFAULT:       /* Bytes to samples */
          *dest_value = src_value / dec->bytes_per_sample;
          res = TRUE;
          break;
        default:
          break;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES: /* Time to bytes */
          *dest_value = gst_util_uint64_scale (src_value,
              (guint64) (dec->bytes_per_sample) * dec->rate, GST_SECOND);
          res = TRUE;
          break;
        case GST_FORMAT_DEFAULT:       /* Time to samples */
          *dest_value = gst_util_uint64_scale (src_value, dec->rate,
              GST_SECOND);
          res = TRUE;
          break;
        default:
          break;
      }
      break;
    case GST_FORMAT_DEFAULT:   /* Samples */
      switch (*dest_format) {
        case GST_FORMAT_BYTES: /* Samples to bytes */
          *dest_value = src_value * dec->bytes_per_sample;
          res = TRUE;
          break;
        case GST_FORMAT_TIME:  /* Samples to time */
          *dest_value = gst_util_uint64_scale (src_value, GST_SECOND,
              dec->rate);
          res = TRUE;
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

  return res;
}

static gboolean
gst_flump3dec_src_query (GstPad * pad, GstQuery * query)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (gst_pad_get_parent (pad));
  GstFormat format;
  GstPad *peer;
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      gint64 cur;

      /* Can't answer any queries if the upstream peer got unlinked */
      if ((peer = gst_pad_get_peer (dec->sinkpad)) == NULL)
        goto beach;

      gst_query_parse_position (query, &format, NULL);

      /* If the format is BYTES or SAMPLES (default), we'll calculate it from 
       * time, else let's see if upstream knows the position in the right 
       * format */
      if (format != GST_FORMAT_BYTES && format != GST_FORMAT_BYTES &&
          gst_pad_query (peer, query)) {
        gst_object_unref (peer);
        res = TRUE;
        goto beach;
      }
      gst_object_unref (peer);

      /* Bring to time format, and from there to output format if needed */
      cur = dec->next_ts;

      if (format != GST_FORMAT_TIME &&
          !gst_flump3dec_src_convert (dec, GST_FORMAT_TIME, cur, &format,
              &cur)) {
        gst_query_set_position (query, format, -1);
        goto beach;
      }
      gst_query_set_position (query, format, cur);
      break;
    }
    case GST_QUERY_DURATION:
    {
      guint64 total;

      /* Can't answer any queries if the upstream peer got unlinked */
      if ((peer = gst_pad_get_peer (dec->sinkpad)) == NULL)
        goto beach;

      gst_query_parse_duration (query, &format, NULL);

      /* If the format is BYTES or SAMPLES (default), we'll calculate it from 
       * time, else let's see if upstream knows the duration in the right 
       * format */
      if (format != GST_FORMAT_BYTES && format != GST_FORMAT_DEFAULT &&
          gst_pad_query (peer, query)) {
        gst_object_unref (peer);
        res = TRUE;
        goto beach;
      }
      gst_object_unref (peer);

      if (!_total_time (dec, &total))
        goto beach;

      if (total != -1) {
        if (format != GST_FORMAT_TIME &&
            !gst_flump3dec_src_convert (dec, GST_FORMAT_TIME, total,
                &format, (gint64 *) & total)) {
          gst_query_set_duration (query, format, -1);
          goto beach;
        }
      }

      gst_query_set_duration (query, format, total);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (dec->srcpad, query);
      break;
  }

beach:
  gst_object_unref (dec);
  return res;
}

static gboolean
gst_flump3dec_src_event (GstPad * pad, GstEvent * event)
{
  GstFluMp3Dec *dec = GST_FLUMP3DEC (gst_pad_get_parent (pad));
  gboolean res = FALSE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstFormat format;
      GstEvent *seek_event;
      gint64 start, stop;
      GstSeekType start_type, stop_type;
      GstSeekFlags in_flags;
      GstFormat in_format;
      gdouble in_rate;

      gst_event_parse_seek (event, &in_rate, &in_format, &in_flags,
          &start_type, &start, &stop_type, &stop);
      gst_event_unref (event);

      GST_DEBUG_OBJECT (dec,
          "Seek, format %d, flags %d, start type %d start %" G_GINT64_FORMAT
          " stop type %d stop %" G_GINT64_FORMAT,
          in_format, in_flags, start_type, start, stop_type, stop);

      /* Convert request to time format if we can */
      if (in_format == GST_FORMAT_DEFAULT || in_format == GST_FORMAT_BYTES) {
        format = GST_FORMAT_TIME;
        if (!gst_flump3dec_src_convert (dec, in_format, start, &format, &start))
          goto beach;
        if (!gst_flump3dec_src_convert (dec, in_format, stop, &format, &stop))
          goto beach;
      } else {
        format = in_format;
      }

      /* See if upstream can seek by our converted time, or by whatever the 
       * input format is */
      seek_event = gst_event_new_seek (in_rate, format, in_flags, start_type,
          start, stop_type, stop);
      if (!seek_event)
        goto beach;

      if (gst_pad_push_event (dec->sinkpad, seek_event)) {
        res = TRUE;
        goto beach;
      }

      /* From here on, we can only support seeks based on TIME format */
      if (format != GST_FORMAT_TIME)
        goto beach;

      /* Convert TIME to BYTES and send upstream */
      if (!_time_to_bytepos (dec, (GstClockTime) start, &start))
        goto beach;
      if (!_time_to_bytepos (dec, (GstClockTime) stop, &stop))
        goto beach;

      seek_event = gst_event_new_seek (in_rate, GST_FORMAT_BYTES, in_flags,
          start_type, start, stop_type, stop);
      if (!seek_event)
        goto beach;

      res = gst_pad_push_event (dec->sinkpad, seek_event);
      break;
    }
    default:
    {
      res = gst_pad_event_default (dec->srcpad, event);
      break;
    }
  }

beach:
  gst_object_unref (dec);
  return res;
}

static GstStateChangeReturn
gst_flump3dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn res;
  GstFluMp3Dec *dec = GST_FLUMP3DEC (element);

  g_return_val_if_fail (dec != NULL, GST_STATE_CHANGE_FAILURE);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* Prepare the decoder */
      _cleanup (dec);
      gst_segment_init (&dec->segment, GST_FORMAT_TIME);
      break;
    default:
      break;
  }

  res = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* Clean up a bit */
      gst_flump3dec_flush (dec);
      _cleanup (dec);      
      break;
    default:
      break;
  }
  return res;
}

static const GstQueryType *
gst_flump3dec_get_query_types (GstPad * pad ATTR_UNUSED)
{
  static const GstQueryType query_types[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    0
  };

  return query_types;
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

  if (dec->adapter) {
    gst_adapter_clear (dec->adapter);
    g_object_unref (dec->adapter);
    dec->adapter = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_flump3dec_class_init (GstFluMp3DecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_flump3dec_dispose;

  element_class->change_state = gst_flump3dec_change_state;


  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_set_details_simple (element_class,
      LONGNAME,
      "Codec/Decoder/Audio",
      "Decodes MPEG-1 Layer 1, 2 and 3 streams to raw audio frames",
      "Fluendo Support <support@fluendo.com>");
}

static void
gst_flump3dec_init (GstFluMp3Dec * dec)
{
  /* Create and add pads */
  dec->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (dec->sinkpad, gst_flump3dec_sink_event);
  gst_pad_set_chain_function (dec->sinkpad, gst_flump3dec_sink_chain);
  gst_element_add_pad (GST_ELEMENT (dec), dec->sinkpad);

  dec->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_query_type_function (dec->srcpad, gst_flump3dec_get_query_types);
  gst_pad_set_query_function (dec->srcpad, gst_flump3dec_src_query);
  gst_pad_set_event_function (dec->srcpad, gst_flump3dec_src_event);
  gst_pad_use_fixed_caps (dec->srcpad);
  gst_element_add_pad (GST_ELEMENT (dec), dec->srcpad);

  dec->adapter = gst_adapter_new ();

  dec->bs = bs_new ();
  g_return_if_fail (dec->bs != NULL);

  dec->dec = mp3tl_new (dec->bs, MP3TL_MODE_16BIT);
  g_return_if_fail (dec->dec != NULL);

  gst_segment_init (&dec->segment, GST_FORMAT_TIME);
}

