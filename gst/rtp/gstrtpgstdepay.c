/* GStreamer
 * Copyright (C) <2010> Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "gstrtpgstdepay.h"

GST_DEBUG_CATEGORY_STATIC (rtpgstdepay_debug);
#define GST_CAT_DEFAULT (rtpgstdepay_debug)

static GstStaticPadTemplate gst_rtp_gst_depay_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_rtp_gst_depay_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"application\", "
        "payload = (int) " GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) 90000, " "encoding-name = (string) \"X-GST\"")
    );

GST_BOILERPLATE (GstRtpGSTDepay, gst_rtp_gst_depay, GstBaseRTPDepayload,
    GST_TYPE_BASE_RTP_DEPAYLOAD);

static void gst_rtp_gst_depay_finalize (GObject * object);

static GstStateChangeReturn gst_rtp_gst_depay_change_state (GstElement *
    element, GstStateChange transition);

static void gst_rtp_gst_depay_reset (GstRtpGSTDepay * rtpgstdepay);
static gboolean gst_rtp_gst_depay_setcaps (GstBaseRTPDepayload * depayload,
    GstCaps * caps);
static GstBuffer *gst_rtp_gst_depay_process (GstBaseRTPDepayload * depayload,
    GstBuffer * buf);

static void
gst_rtp_gst_depay_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_gst_depay_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_rtp_gst_depay_sink_template);

  gst_element_class_set_details_simple (element_class,
      "GStreamer depayloader", "Codec/Depayloader/Network",
      "Extracts GStreamer buffers from RTP packets",
      "Wim Taymans <wim.taymans@gmail.com>");
}

static void
gst_rtp_gst_depay_class_init (GstRtpGSTDepayClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseRTPDepayloadClass *gstbasertpdepayload_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasertpdepayload_class = (GstBaseRTPDepayloadClass *) klass;

  gobject_class->finalize = gst_rtp_gst_depay_finalize;

  gstelement_class->change_state = gst_rtp_gst_depay_change_state;

  gstbasertpdepayload_class->set_caps = gst_rtp_gst_depay_setcaps;
  gstbasertpdepayload_class->process = gst_rtp_gst_depay_process;

  GST_DEBUG_CATEGORY_INIT (rtpgstdepay_debug, "rtpgstdepay", 0,
      "Gstreamer RTP Depayloader");
}

static void
gst_rtp_gst_depay_init (GstRtpGSTDepay * rtpgstdepay,
    GstRtpGSTDepayClass * klass)
{
  rtpgstdepay->adapter = gst_adapter_new ();
}

static void
gst_rtp_gst_depay_finalize (GObject * object)
{
  GstRtpGSTDepay *rtpgstdepay;

  rtpgstdepay = GST_RTP_GST_DEPAY (object);

  gst_rtp_gst_depay_reset (rtpgstdepay);
  g_object_unref (rtpgstdepay->adapter);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
store_cache (GstRtpGSTDepay * rtpgstdepay, guint CV, GstCaps * caps)
{
  if (rtpgstdepay->CV_cache[CV])
    gst_caps_unref (rtpgstdepay->CV_cache[CV]);
  rtpgstdepay->CV_cache[CV] = caps;
}

static void
gst_rtp_gst_depay_reset (GstRtpGSTDepay * rtpgstdepay)
{
  guint i;

  gst_adapter_clear (rtpgstdepay->adapter);
  rtpgstdepay->current_CV = 0;
  for (i = 0; i < 8; i++)
    store_cache (rtpgstdepay, i, NULL);
}

static gboolean
gst_rtp_gst_depay_setcaps (GstBaseRTPDepayload * depayload, GstCaps * caps)
{
  GstRtpGSTDepay *rtpgstdepay;
  GstStructure *structure;
  gint clock_rate;
  gboolean res;
  const gchar *capsenc;

  rtpgstdepay = GST_RTP_GST_DEPAY (depayload);

  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "clock-rate", &clock_rate))
    clock_rate = 90000;
  depayload->clock_rate = clock_rate;

  capsenc = gst_structure_get_string (structure, "caps");
  if (capsenc) {
    GstCaps *outcaps;
    gsize out_len;
    gchar *capsstr;
    const gchar *capsver;
    guint CV;

    /* decode caps */
    capsstr = (gchar *) g_base64_decode (capsenc, &out_len);
    outcaps = gst_caps_from_string (capsstr);
    g_free (capsstr);

    /* parse version */
    capsver = gst_structure_get_string (structure, "capsversion");
    if (capsver) {
      CV = atoi (capsver);
    } else {
      /* no version, assume 0 */
      CV = 0;
    }
    /* store in cache */
    rtpgstdepay->current_CV = CV;
    gst_caps_ref (outcaps);
    store_cache (rtpgstdepay, CV, outcaps);

    res = gst_pad_set_caps (depayload->srcpad, outcaps);
    gst_caps_unref (outcaps);
  } else {
    GST_WARNING_OBJECT (depayload, "no caps given");
    rtpgstdepay->current_CV = -1;
    res = TRUE;
  }

  return res;
}

static gboolean
read_length (GstRtpGSTDepay * rtpgstdepay, guint8 * data, guint size,
    guint * length, guint * skip)
{
  guint b, len, offset;

  /* start reading the length, we need this to skip to the data later */
  len = offset = 0;
  do {
    if (offset >= size)
      return FALSE;
    b = data[offset++];
    len = (len << 7) | (b & 0x7f);
  } while (b & 0x80);

  /* check remaining buffer size */
  if (size - offset < len)
    return FALSE;

  *length = len;
  *skip = offset;

  return TRUE;
}

static GstCaps *
read_caps (GstRtpGSTDepay * rtpgstdepay, GstBuffer * buf, guint * skip)
{
  guint8 *data;
  guint size, offset, length;
  GstCaps *caps;

  data = GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);

  GST_DEBUG_OBJECT (rtpgstdepay, "buffer size %u", size);

  if (!read_length (rtpgstdepay, data, size, &length, &offset))
    goto too_small;

  GST_DEBUG_OBJECT (rtpgstdepay, "parsing caps %s", &data[offset]);

  /* parse and store in cache */
  caps = gst_caps_from_string ((gchar *) & data[offset]);

  *skip = length + offset;

  return caps;

too_small:
  {
    GST_ELEMENT_WARNING (rtpgstdepay, STREAM, DECODE,
        ("Buffer too small."), (NULL));
    return NULL;
  }
}

static GstEvent *
read_event (GstRtpGSTDepay * rtpgstdepay, guint type,
    GstBuffer * buf, guint * skip)
{
  guint8 *data;
  guint size, offset, length;
  GstStructure *s;
  GstEvent *event;
  GstEventType etype;
  gchar *end;

  data = GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);

  GST_DEBUG_OBJECT (rtpgstdepay, "buffer size %u", size);

  if (!read_length (rtpgstdepay, data, size, &length, &offset))
    goto too_small;

  GST_DEBUG_OBJECT (rtpgstdepay, "parsing event %s", &data[offset]);

  /* parse */
  s = gst_structure_from_string ((gchar *) & data[offset], &end);

  switch (type) {
    case 1:
      etype = GST_EVENT_TAG;
      break;
    case 2:
      etype = GST_EVENT_CUSTOM_DOWNSTREAM;
      break;
    case 3:
      etype = GST_EVENT_CUSTOM_BOTH;
      break;
    default:
      goto unknown_event;
  }
  event = gst_event_new_custom (etype, s);

  *skip = length + offset;

  return event;

too_small:
  {
    GST_ELEMENT_WARNING (rtpgstdepay, STREAM, DECODE,
        ("Buffer too small."), (NULL));
    return NULL;
  }
unknown_event:
  {
    GST_DEBUG_OBJECT (rtpgstdepay, "unknown event type");
    return NULL;
  }
}

static GstBuffer *
gst_rtp_gst_depay_process (GstBaseRTPDepayload * depayload, GstBuffer * buf)
{
  GstRtpGSTDepay *rtpgstdepay;
  GstBuffer *subbuf, *outbuf = NULL;
  gint payload_len;
  guint8 *payload;
  guint CV, frag_offset, avail, offset;

  rtpgstdepay = GST_RTP_GST_DEPAY (depayload);

  payload_len = gst_rtp_buffer_get_payload_len (buf);

  if (payload_len <= 8)
    goto empty_packet;

  if (GST_BUFFER_IS_DISCONT (buf)) {
    GST_WARNING_OBJECT (rtpgstdepay, "DISCONT, clear adapter");
    gst_adapter_clear (rtpgstdepay->adapter);
  }

  payload = gst_rtp_buffer_get_payload (buf);

  /* strip off header
   *
   *  0                   1                   2                   3
   *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * |C| CV  |D|X|Y|Z|     ETYPE     |  MBZ                          |
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   * |                          Frag_offset                          |
   * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */
  frag_offset =
      (payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7];

  avail = gst_adapter_available (rtpgstdepay->adapter);
  if (avail != frag_offset)
    goto wrong_frag;

  /* subbuffer skipping the 8 header bytes */
  subbuf = gst_rtp_buffer_get_payload_subbuffer (buf, 8, -1);
  gst_adapter_push (rtpgstdepay->adapter, subbuf);

  offset = 0;
  if (gst_rtp_buffer_get_marker (buf)) {
    guint avail;
    GstCaps *outcaps;

    /* take the buffer */
    avail = gst_adapter_available (rtpgstdepay->adapter);
    outbuf = gst_adapter_take_buffer (rtpgstdepay->adapter, avail);

    CV = (payload[0] >> 4) & 0x7;

    if (payload[0] & 0x80) {
      guint size;

      /* C bit, we have inline caps */
      outcaps = read_caps (rtpgstdepay, outbuf, &size);
      if (outcaps == NULL)
        goto no_caps;

      GST_DEBUG_OBJECT (rtpgstdepay,
          "inline caps %u, length %u, %" GST_PTR_FORMAT, CV, size, outcaps);

      store_cache (rtpgstdepay, CV, outcaps);

      /* skip caps */
      offset += size;
      avail -= size;
    }
    if (payload[1]) {
      guint size;
      GstEvent *event;

      /* we have an event */
      event = read_event (rtpgstdepay, payload[1], outbuf, &size);
      if (event == NULL)
        goto no_event;

      GST_DEBUG_OBJECT (rtpgstdepay,
          "inline event, length %u, %" GST_PTR_FORMAT, size, event);

      gst_pad_push_event (depayload->srcpad, event);

      /* no buffer after event */
      avail = 0;
    }

    if (avail) {
      if (offset != 0) {
        GstBuffer *temp;

        GST_DEBUG_OBJECT (rtpgstdepay, "sub buffer: offset %u, size %u", offset,
            avail);

        temp = gst_buffer_create_sub (outbuf, offset, avail);
        gst_buffer_unref (outbuf);
        outbuf = temp;
      }

      /* see what caps we need */
      if (CV != rtpgstdepay->current_CV) {
        /* we need to switch caps, check if we have the caps */
        if ((outcaps = rtpgstdepay->CV_cache[CV]) == NULL)
          goto missing_caps;

        GST_DEBUG_OBJECT (rtpgstdepay,
            "need caps switch from %u to %u, %" GST_PTR_FORMAT,
            rtpgstdepay->current_CV, CV, outcaps);

        /* and set caps */
        if (gst_pad_set_caps (depayload->srcpad, outcaps))
          rtpgstdepay->current_CV = CV;
      }

      if (payload[0] & 0x8)
        GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DELTA_UNIT);
      if (payload[0] & 0x4)
        GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_MEDIA1);
      if (payload[0] & 0x2)
        GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_MEDIA2);
      if (payload[0] & 0x1)
        GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_MEDIA3);
    } else {
      gst_buffer_unref (outbuf);
      outbuf = NULL;
    }
  }
  return outbuf;

  /* ERRORS */
empty_packet:
  {
    GST_ELEMENT_WARNING (rtpgstdepay, STREAM, DECODE,
        ("Empty Payload."), (NULL));
    return NULL;
  }
wrong_frag:
  {
    gst_adapter_clear (rtpgstdepay->adapter);
    GST_LOG_OBJECT (rtpgstdepay, "wrong fragment, skipping");
    return NULL;
  }
no_caps:
  {
    GST_WARNING_OBJECT (rtpgstdepay, "failed to parse caps");
    gst_buffer_unref (outbuf);
    return NULL;
  }
no_event:
  {
    GST_WARNING_OBJECT (rtpgstdepay, "failed to parse event");
    gst_buffer_unref (outbuf);
    return NULL;
  }
missing_caps:
  {
    GST_ELEMENT_WARNING (rtpgstdepay, STREAM, DECODE,
        ("Missing caps %u.", CV), (NULL));
    gst_buffer_unref (outbuf);
    return NULL;
  }
}

static GstStateChangeReturn
gst_rtp_gst_depay_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpGSTDepay *rtpgstdepay;
  GstStateChangeReturn ret;

  rtpgstdepay = GST_RTP_GST_DEPAY (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_rtp_gst_depay_reset (rtpgstdepay);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rtp_gst_depay_reset (rtpgstdepay);
      break;
    default:
      break;
  }
  return ret;
}


gboolean
gst_rtp_gst_depay_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rtpgstdepay",
      GST_RANK_MARGINAL, GST_TYPE_RTP_GST_DEPAY);
}
