/* GStreamer
* Copyright (C) 2005 Sebastien Moutte <sebastien@moutte.net>
* Copyright (C) 2007 Pioneers of the Inevitable <songbird@songbirdnest.com>
* Copyright (C) 2010 Fluendo S.A. <support@fluendo.com>
*
* gstdirectsoundsink.c:
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
*
*
* The development of this code was made possible due to the involvement
* of Pioneers of the Inevitable, the creators of the Songbird Music player
*
*/

/**
 * SECTION:element-directsoundsink
 *
 * This element lets you output sound using the DirectSound API.
 *
 * Note that you should almost always use generic audio conversion elements
 * like audioconvert and audioresample in front of an audiosink to make sure
 * your pipeline works under all circumstances (those conversion elements will
 * act in passthrough-mode if no conversion is necessary).
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v audiotestsrc ! audioconvert ! volume volume=0.1 ! directsoundsink
 * ]| will output a sine wave (continuous beep sound) to your sound card (with
 * a very low volume as precaution).
 * |[
 * gst-launch -v filesrc location=music.ogg ! decodebin ! audioconvert ! audioresample ! directsoundsink
 * ]| will play an Ogg/Vorbis audio file and output it.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdirectsoundsink.h"
#include <gst/audio/gstaudioiec61937.h>

#include <math.h>

#ifdef __CYGWIN__
#include <unistd.h>
#ifndef _swab
#define _swab swab
#endif
#endif

GST_DEBUG_CATEGORY_STATIC (directsoundsink_debug);
#define GST_CAT_DEFAULT directsoundsink_debug

static void gst_directsound_sink_finalise (GObject * object);

static void gst_directsound_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_directsound_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_directsound_sink_getcaps (GstBaseSink * bsink);
static GstBuffer *gst_directsound_sink_payload (GstBaseAudioSink * sink,
    GstBuffer * buf);
static gboolean gst_directsound_sink_prepare (GstAudioSink * asink,
    GstRingBufferSpec * spec);
static gboolean gst_directsound_sink_unprepare (GstAudioSink * asink);

static gboolean gst_directsound_sink_open (GstAudioSink * asink);
static gboolean gst_directsound_sink_close (GstAudioSink * asink);
static guint gst_directsound_sink_write (GstAudioSink * asink, gpointer data,
    guint length);
static guint gst_directsound_sink_delay (GstAudioSink * asink);
static void gst_directsound_sink_reset (GstAudioSink * asink);
static GstCaps *gst_directsound_probe_supported_formats (GstDirectSoundSink *
    dsoundsink, const GstCaps * template_caps);
static gboolean gst_directsound_sink_acceptcaps (GstPad * pad, GstCaps * caps);
static boolean gst_directsound_sink_is_spdif_format (GstDirectSoundSink *
    dsoundsink);

/* interfaces */
static void gst_directsound_sink_interfaces_init (GType type);
static void
gst_directsound_sink_implements_interface_init (GstImplementsInterfaceClass *
    iface);
static void gst_directsound_sink_mixer_interface_init (GstMixerClass * iface);

static GstStaticPadTemplate directsoundsink_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "signed = (boolean) TRUE, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
        "audio/x-raw-int, "
        "signed = (boolean) FALSE, "
        "width = (int) 8, "
        "depth = (int) 8, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ];"
        "audio/x-ac3, framed = (boolean) true;"
        "audio/x-dts, framed = (boolean) true;"));

enum
{
  PROP_0,
  PROP_VOLUME
};

GST_BOILERPLATE_FULL (GstDirectSoundSink, gst_directsound_sink, GstAudioSink,
    GST_TYPE_AUDIO_SINK, gst_directsound_sink_interfaces_init);

/* interfaces stuff */
static void
gst_directsound_sink_interfaces_init (GType type)
{
  static const GInterfaceInfo implements_interface_info = {
    (GInterfaceInitFunc) gst_directsound_sink_implements_interface_init,
    NULL,
    NULL,
  };

  static const GInterfaceInfo mixer_interface_info = {
    (GInterfaceInitFunc) gst_directsound_sink_mixer_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
      GST_TYPE_IMPLEMENTS_INTERFACE, &implements_interface_info);
  g_type_add_interface_static (type, GST_TYPE_MIXER, &mixer_interface_info);
}

static gboolean
gst_directsound_sink_interface_supported (GstImplementsInterface * iface,
    GType iface_type)
{
  g_return_val_if_fail (iface_type == GST_TYPE_MIXER, FALSE);

  /* for the sake of this example, we'll always support it. However, normally,
   * you would check whether the device you've opened supports mixers. */
  return TRUE;
}

static void
gst_directsound_sink_implements_interface_init (GstImplementsInterfaceClass *
    iface)
{
  iface->supported = gst_directsound_sink_interface_supported;
}

/*
 * This function returns the list of support tracks (inputs, outputs)
 * on this element instance. Elements usually build this list during
 * _init () or when going from NULL to READY.
 */

static const GList *
gst_directsound_sink_mixer_list_tracks (GstMixer * mixer)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (mixer);

  return dsoundsink->tracks;
}

static void
gst_directsound_sink_set_volume (GstDirectSoundSink * dsoundsink)
{
  if (dsoundsink->pDSBSecondary) {
    /* DirectSound controls volume using units of 100th of a decibel,
     * ranging from -10000 to 0. We use a linear scale of 0 - 100
     * here, so remap.
     */
    long dsVolume;
    if (dsoundsink->volume == 0)
      dsVolume = -10000;
    else
      dsVolume = 100 * (long) (20 * log10 ((double) dsoundsink->volume / 100.));
    dsVolume = CLAMP (dsVolume, -10000, 0);

    GST_DEBUG_OBJECT (dsoundsink,
        "Setting volume on secondary buffer to %d from %d", (int) dsVolume,
        (int) dsoundsink->volume);
    IDirectSoundBuffer_SetVolume (dsoundsink->pDSBSecondary, dsVolume);
  }
}

/*
 * Set volume. volumes is an array of size track->num_channels, and
 * each value in the array gives the wanted volume for one channel
 * on the track.
 */

static void
gst_directsound_sink_mixer_set_volume (GstMixer * mixer,
    GstMixerTrack * track, gint * volumes)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (mixer);

  if (volumes[0] != dsoundsink->volume) {
    dsoundsink->volume = volumes[0];

    gst_directsound_sink_set_volume (dsoundsink);
  }
}

static void
gst_directsound_sink_mixer_get_volume (GstMixer * mixer,
    GstMixerTrack * track, gint * volumes)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (mixer);

  volumes[0] = dsoundsink->volume;
}

static void
gst_directsound_sink_mixer_interface_init (GstMixerClass * iface)
{
  /* the mixer interface requires a definition of the mixer type:
   * hardware or software? */
  GST_MIXER_TYPE (iface) = GST_MIXER_SOFTWARE;

  /* virtual function pointers */
  iface->list_tracks = gst_directsound_sink_mixer_list_tracks;
  iface->set_volume = gst_directsound_sink_mixer_set_volume;
  iface->get_volume = gst_directsound_sink_mixer_get_volume;
}

static void
gst_directsound_sink_finalise (GObject * object)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (object);

  g_mutex_free (dsoundsink->dsound_lock);

  if (dsoundsink->tracks) {
    g_list_foreach (dsoundsink->tracks, (GFunc) g_object_unref, NULL);
    g_list_free (dsoundsink->tracks);
    dsoundsink->tracks = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_directsound_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "Direct Sound Audio Sink", "Sink/Audio",
      "Output to a sound card via Direct Sound",
      "Sebastien Moutte <sebastien@moutte.net>");
  gst_element_class_add_static_pad_template (element_class,
      &directsoundsink_sink_factory);
}

static void
gst_directsound_sink_class_init (GstDirectSoundSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstBaseAudioSinkClass *gstbaseaudiosink_class;
  GstAudioSinkClass *gstaudiosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstbaseaudiosink_class = (GstBaseAudioSinkClass *) klass;
  gstaudiosink_class = (GstAudioSinkClass *) klass;

  GST_DEBUG_CATEGORY_INIT (directsoundsink_debug, "directsoundsink", 0,
      "DirectSound sink");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_directsound_sink_finalise;
  gobject_class->set_property = gst_directsound_sink_set_property;
  gobject_class->get_property = gst_directsound_sink_get_property;

  gstbasesink_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_directsound_sink_getcaps);

  gstbaseaudiosink_class->payload =
      GST_DEBUG_FUNCPTR (gst_directsound_sink_payload);

  gstaudiosink_class->prepare =
      GST_DEBUG_FUNCPTR (gst_directsound_sink_prepare);
  gstaudiosink_class->unprepare =
      GST_DEBUG_FUNCPTR (gst_directsound_sink_unprepare);
  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_directsound_sink_open);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_directsound_sink_close);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_directsound_sink_write);
  gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_directsound_sink_delay);
  gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_directsound_sink_reset);

  g_object_class_install_property (gobject_class,
      PROP_VOLUME,
      g_param_spec_double ("volume", "Volume",
          "Volume of this stream", 0.0, 1.0, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_directsound_sink_init (GstDirectSoundSink * dsoundsink,
    GstDirectSoundSinkClass * g_class)
{
  GstMixerTrack *track = NULL;

  dsoundsink->tracks = NULL;
  track = g_object_new (GST_TYPE_MIXER_TRACK, NULL);
  track->label = g_strdup ("DSoundTrack");
  track->num_channels = 2;
  track->min_volume = 0;
  track->max_volume = 100;
  track->flags = GST_MIXER_TRACK_OUTPUT;
  dsoundsink->tracks = g_list_append (dsoundsink->tracks, track);

  dsoundsink->pDS = NULL;
  dsoundsink->cached_caps = NULL;
  dsoundsink->pDSBSecondary = NULL;
  dsoundsink->current_circular_offset = 0;
  dsoundsink->buffer_size = DSBSIZE_MIN;
  dsoundsink->volume = 100;
  dsoundsink->dsound_lock = g_mutex_new ();
  dsoundsink->first_buffer_after_reset = FALSE;

  gst_pad_set_acceptcaps_function (GST_BASE_SINK (dsoundsink)->sinkpad,
      GST_DEBUG_FUNCPTR (gst_directsound_sink_acceptcaps));
}

static void
gst_directsound_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSink *sink = GST_DIRECTSOUND_SINK (object);

  switch (prop_id) {
    case PROP_VOLUME:
      sink->volume = (int) (g_value_get_double (value) * 100);
      gst_directsound_sink_set_volume (sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_directsound_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDirectSoundSink *sink = GST_DIRECTSOUND_SINK (object);

  switch (prop_id) {
    case PROP_VOLUME:
      g_value_set_double (value, (double) sink->volume / 100.);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_directsound_sink_getcaps (GstBaseSink * bsink)
{
  GstElementClass *element_class;
  GstPadTemplate *pad_template;
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (bsink);
  GstCaps *caps;
  gchar *caps_string = NULL;

  if (dsoundsink->pDS == NULL) {
    GST_DEBUG_OBJECT (dsoundsink, "device not open, using template caps");
    return NULL;                /* base class will get template caps for us */
  }

  if (dsoundsink->cached_caps) {
    caps_string = gst_caps_to_string (dsoundsink->cached_caps);
    GST_DEBUG_OBJECT (dsoundsink, "Returning cached caps: %s", caps_string);
    g_free (caps_string);
    return gst_caps_ref (dsoundsink->cached_caps);
  }

  element_class = GST_ELEMENT_GET_CLASS (dsoundsink);
  pad_template = gst_element_class_get_pad_template (element_class, "sink");
  g_return_val_if_fail (pad_template != NULL, NULL);

  caps = gst_directsound_probe_supported_formats (dsoundsink,
      gst_pad_template_get_caps (pad_template));
  if (caps) {
    dsoundsink->cached_caps = gst_caps_ref (caps);
  }

  if (caps) {
    gchar *caps_string = gst_caps_to_string (caps);
    GST_DEBUG_OBJECT (dsoundsink, "returning caps %s", caps_string);
    g_free (caps_string);
  }

  return caps;
}

static gboolean
gst_directsound_sink_acceptcaps (GstPad * pad, GstCaps * caps)
{
  GstDirectSoundSink *dsink =
      GST_DIRECTSOUND_SINK (gst_pad_get_parent_element (pad));
  GstCaps *pad_caps;
  GstStructure *st;
  gboolean ret = FALSE;

  GstRingBufferSpec spec = { 0 };

  pad_caps = gst_pad_get_caps_reffed (pad);
  if (pad_caps) {
    gboolean cret = gst_caps_can_intersect (pad_caps, caps);
    gst_caps_unref (pad_caps);
    if (!cret)
      goto done;
  }

  /* If we've not got fixed caps, creating a stream might fail, so let's just
   * return from here with default acceptcaps behaviour */
  if (!gst_caps_is_fixed (caps))
    goto done;

  /* parse helper expects this set, so avoid nasty warning
   * will be set properly later on anyway  */
  spec.latency_time = GST_SECOND;
  if (!gst_ring_buffer_parse_caps (&spec, caps))
    goto done;

  /* Make sure input is framed (one frame per buffer) and can be payloaded */
  switch (spec.type) {
    case GST_BUFTYPE_AC3:
    case GST_BUFTYPE_DTS:
    {
      gboolean framed = FALSE, parsed = FALSE;
      st = gst_caps_get_structure (caps, 0);

      gst_structure_get_boolean (st, "framed", &framed);
      gst_structure_get_boolean (st, "parsed", &parsed);
      if ((!framed && !parsed) || gst_audio_iec61937_frame_size (&spec) <= 0)
        goto done;
    }
    default:
      break;
  }
  ret = TRUE;

done:
  gst_object_unref (dsink);
  return ret;
}

static gboolean
gst_directsound_sink_open (GstAudioSink * asink)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (asink);
  HRESULT hRes;

  /* create and initialize a DirecSound object */
  if (FAILED (hRes = DirectSoundCreate (NULL, &dsoundsink->pDS, NULL))) {
    GST_ELEMENT_ERROR (dsoundsink, RESOURCE, OPEN_READ,
        ("gst_directsound_sink_open: DirectSoundCreate: %s",
            DXGetErrorString9 (hRes)), (NULL));
    return FALSE;
  }

  if (FAILED (hRes = IDirectSound_SetCooperativeLevel (dsoundsink->pDS,
              GetDesktopWindow (), DSSCL_PRIORITY))) {
    GST_ELEMENT_ERROR (dsoundsink, RESOURCE, OPEN_READ,
        ("gst_directsound_sink_open: IDirectSound_SetCooperativeLevel: %s",
            DXGetErrorString9 (hRes)), (NULL));
    return FALSE;
  }

  return TRUE;
}

static boolean
gst_directsound_sink_is_spdif_format (GstDirectSoundSink * dsoundsink)
{
  GstBufferFormatType type;

  type = GST_BASE_AUDIO_SINK (dsoundsink)->ringbuffer->spec.type;
  return type == GST_BUFTYPE_AC3 || type == GST_BUFTYPE_DTS;
}

static gboolean
gst_directsound_sink_prepare (GstAudioSink * asink, GstRingBufferSpec * spec)
{
  GstDirectSoundSink *dsoundsink = GST_DIRECTSOUND_SINK (asink);
  HRESULT hRes;
  DSBUFFERDESC descSecondary;
  WAVEFORMATEX wfx;

  /*save number of bytes per sample and buffer format */
  dsoundsink->bytes_per_sample = spec->bytes_per_sample;
  dsoundsink->buffer_format = spec->format;

  /* fill the WAVEFORMATEX structure with spec params */
  memset (&wfx, 0, sizeof (wfx));
  if (!gst_directsound_sink_is_spdif_format (dsoundsink)) {
    wfx.cbSize = sizeof (wfx);
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = spec->channels;
    wfx.nSamplesPerSec = spec->rate;
    wfx.wBitsPerSample = (spec->bytes_per_sample * 8) / wfx.nChannels;
    wfx.nBlockAlign = spec->bytes_per_sample;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    /* Create directsound buffer with size based on our configured  
     * buffer_size (which is 200 ms by default) */
    dsoundsink->buffer_size =
        gst_util_uint64_scale_int (wfx.nAvgBytesPerSec, spec->buffer_time,
        GST_MSECOND);
    /* Make sure we make those numbers multiple of our sample size in bytes */
    dsoundsink->buffer_size += dsoundsink->buffer_size % spec->bytes_per_sample;

    spec->segsize =
        gst_util_uint64_scale_int (wfx.nAvgBytesPerSec, spec->latency_time,
        GST_MSECOND);
    spec->segsize += spec->segsize % spec->bytes_per_sample;
    spec->segtotal = dsoundsink->buffer_size / spec->segsize;
  } else {
#ifdef WAVE_FORMAT_DOLBY_AC3_SPDIF
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 48000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    spec->segsize = 6144;
    spec->segtotal = 10;
#else
    g_assert_not_reached ();
#endif
  }

  // Make the final buffer size be an integer number of segments
  dsoundsink->buffer_size = spec->segsize * spec->segtotal;

  GST_INFO_OBJECT (dsoundsink,
      "GstRingBufferSpec->channels: %d, GstRingBufferSpec->rate: %d, GstRingBufferSpec->bytes_per_sample: %d\n"
      "WAVEFORMATEX.nSamplesPerSec: %ld, WAVEFORMATEX.wBitsPerSample: %d, WAVEFORMATEX.nBlockAlign: %d, WAVEFORMATEX.nAvgBytesPerSec: %ld\n"
      "Size of dsound circular buffer=>%d\n", spec->channels, spec->rate,
      spec->bytes_per_sample, wfx.nSamplesPerSec, wfx.wBitsPerSample,
      wfx.nBlockAlign, wfx.nAvgBytesPerSec, dsoundsink->buffer_size);

  /* create a secondary directsound buffer */
  memset (&descSecondary, 0, sizeof (DSBUFFERDESC));
  descSecondary.dwSize = sizeof (DSBUFFERDESC);
  descSecondary.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
  if (!gst_directsound_sink_is_spdif_format (dsoundsink))
    descSecondary.dwFlags |= DSBCAPS_CTRLVOLUME;

  descSecondary.dwBufferBytes = dsoundsink->buffer_size;
  descSecondary.lpwfxFormat = (WAVEFORMATEX *) & wfx;

  hRes = IDirectSound_CreateSoundBuffer (dsoundsink->pDS, &descSecondary,
      &dsoundsink->pDSBSecondary, NULL);
  if (FAILED (hRes)) {
    GST_ELEMENT_ERROR (dsoundsink, RESOURCE, OPEN_READ,
        ("gst_directsound_sink_prepare: IDirectSound_CreateSoundBuffer: %s",
            DXGetErrorString9 (hRes)), (NULL));
    return FALSE;
  }

  gst_directsound_sink_set_volume (dsoundsink);

  return TRUE;
}

static gboolean
gst_directsound_sink_unprepare (GstAudioSink * asink)
{
  GstDirectSoundSink *dsoundsink;

  dsoundsink = GST_DIRECTSOUND_SINK (asink);

  /* release secondary DirectSound buffer */
  if (dsoundsink->pDSBSecondary) {
    IDirectSoundBuffer_Release (dsoundsink->pDSBSecondary);
    dsoundsink->pDSBSecondary = NULL;
  }

  return TRUE;
}

static gboolean
gst_directsound_sink_close (GstAudioSink * asink)
{
  GstDirectSoundSink *dsoundsink = NULL;

  dsoundsink = GST_DIRECTSOUND_SINK (asink);

  /* release DirectSound object */
  g_return_val_if_fail (dsoundsink->pDS != NULL, FALSE);
  IDirectSound_Release (dsoundsink->pDS);
  dsoundsink->pDS = NULL;

  gst_caps_replace (&dsoundsink->cached_caps, NULL);

  return TRUE;
}

static guint
gst_directsound_sink_write (GstAudioSink * asink, gpointer data, guint length)
{
  GstDirectSoundSink *dsoundsink;
  DWORD dwStatus;
  HRESULT hRes;
  LPVOID pLockedBuffer1 = NULL, pLockedBuffer2 = NULL;
  DWORD dwSizeBuffer1, dwSizeBuffer2;
  DWORD dwCurrentPlayCursor;

  dsoundsink = GST_DIRECTSOUND_SINK (asink);

  GST_DSOUND_LOCK (dsoundsink);

  /* get current buffer status */
  hRes = IDirectSoundBuffer_GetStatus (dsoundsink->pDSBSecondary, &dwStatus);

  /* get current play cursor position */
  hRes = IDirectSoundBuffer_GetCurrentPosition (dsoundsink->pDSBSecondary,
      &dwCurrentPlayCursor, NULL);

  if (SUCCEEDED (hRes) && (dwStatus & DSBSTATUS_PLAYING)) {
    DWORD dwFreeBufferSize;

  calculate_freesize:
    /* calculate the free size of the circular buffer */
    if (dwCurrentPlayCursor < dsoundsink->current_circular_offset)
      dwFreeBufferSize =
          dsoundsink->buffer_size - (dsoundsink->current_circular_offset -
          dwCurrentPlayCursor);
    else
      dwFreeBufferSize =
          dwCurrentPlayCursor - dsoundsink->current_circular_offset;

    if (length >= dwFreeBufferSize) {
      Sleep (100);
      hRes = IDirectSoundBuffer_GetCurrentPosition (dsoundsink->pDSBSecondary,
          &dwCurrentPlayCursor, NULL);

      hRes =
          IDirectSoundBuffer_GetStatus (dsoundsink->pDSBSecondary, &dwStatus);
      if (SUCCEEDED (hRes) && (dwStatus & DSBSTATUS_PLAYING))
        goto calculate_freesize;
      else {
        dsoundsink->first_buffer_after_reset = FALSE;
        GST_DSOUND_UNLOCK (dsoundsink);
        return 0;
      }
    }
  }

  if (dwStatus & DSBSTATUS_BUFFERLOST) {
    hRes = IDirectSoundBuffer_Restore (dsoundsink->pDSBSecondary);      /*need a loop waiting the buffer is restored?? */

    dsoundsink->current_circular_offset = 0;
  }

  hRes = IDirectSoundBuffer_Lock (dsoundsink->pDSBSecondary,
      dsoundsink->current_circular_offset, length, &pLockedBuffer1,
      &dwSizeBuffer1, &pLockedBuffer2, &dwSizeBuffer2, 0L);

  if (SUCCEEDED (hRes)) {
    // Write to pointers without reordering.
    memcpy (pLockedBuffer1, data, dwSizeBuffer1);
    if (pLockedBuffer2 != NULL)
      memcpy (pLockedBuffer2, (LPBYTE) data + dwSizeBuffer1, dwSizeBuffer2);

    // Update where the buffer will lock (for next time)
    dsoundsink->current_circular_offset += dwSizeBuffer1 + dwSizeBuffer2;
    dsoundsink->current_circular_offset %= dsoundsink->buffer_size;     /* Circular buffer */

    hRes = IDirectSoundBuffer_Unlock (dsoundsink->pDSBSecondary, pLockedBuffer1,
        dwSizeBuffer1, pLockedBuffer2, dwSizeBuffer2);
  }

  /* if the buffer was not in playing state yet, call play on the buffer 
     except if this buffer is the fist after a reset (base class call reset and write a buffer when setting the sink to pause) */
  if (!(dwStatus & DSBSTATUS_PLAYING) &&
      dsoundsink->first_buffer_after_reset == FALSE) {
    hRes = IDirectSoundBuffer_Play (dsoundsink->pDSBSecondary, 0, 0,
        DSBPLAY_LOOPING);
  }

  dsoundsink->first_buffer_after_reset = FALSE;

  GST_DSOUND_UNLOCK (dsoundsink);

  return length;
}

static guint
gst_directsound_sink_delay (GstAudioSink * asink)
{
  GstDirectSoundSink *dsoundsink;
  HRESULT hRes;
  DWORD dwCurrentPlayCursor;
  DWORD dwBytesInQueue = 0;
  gint nNbSamplesInQueue = 0;
  DWORD dwStatus;

  dsoundsink = GST_DIRECTSOUND_SINK (asink);

  /* get current buffer status */
  hRes = IDirectSoundBuffer_GetStatus (dsoundsink->pDSBSecondary, &dwStatus);

  if (dwStatus & DSBSTATUS_PLAYING) {
    /*evaluate the number of samples in queue in the circular buffer */
    hRes = IDirectSoundBuffer_GetCurrentPosition (dsoundsink->pDSBSecondary,
        &dwCurrentPlayCursor, NULL);

    if (hRes == S_OK) {
      if (dwCurrentPlayCursor < dsoundsink->current_circular_offset)
        dwBytesInQueue =
            dsoundsink->current_circular_offset - dwCurrentPlayCursor;
      else
        dwBytesInQueue =
            dsoundsink->current_circular_offset + (dsoundsink->buffer_size -
            dwCurrentPlayCursor);

      nNbSamplesInQueue = dwBytesInQueue / dsoundsink->bytes_per_sample;
    }
  }

  return nNbSamplesInQueue;
}

static void
gst_directsound_sink_reset (GstAudioSink * asink)
{
  GstDirectSoundSink *dsoundsink;
  LPVOID pLockedBuffer = NULL;
  DWORD dwSizeBuffer = 0;

  dsoundsink = GST_DIRECTSOUND_SINK (asink);

  GST_DSOUND_LOCK (dsoundsink);

  if (dsoundsink->pDSBSecondary) {
    /*stop playing */
    HRESULT hRes = IDirectSoundBuffer_Stop (dsoundsink->pDSBSecondary);

    /*reset position */
    hRes = IDirectSoundBuffer_SetCurrentPosition (dsoundsink->pDSBSecondary, 0);
    dsoundsink->current_circular_offset = 0;

    /*reset the buffer */
    hRes = IDirectSoundBuffer_Lock (dsoundsink->pDSBSecondary,
        dsoundsink->current_circular_offset, dsoundsink->buffer_size,
        &pLockedBuffer, &dwSizeBuffer, NULL, NULL, 0L);

    if (SUCCEEDED (hRes)) {
      memset (pLockedBuffer, 0, dwSizeBuffer);

      hRes =
          IDirectSoundBuffer_Unlock (dsoundsink->pDSBSecondary, pLockedBuffer,
          dwSizeBuffer, NULL, 0);
    }
  }

  dsoundsink->first_buffer_after_reset = TRUE;

  GST_DSOUND_UNLOCK (dsoundsink);
}

/* 
 * gst_directsound_probe_supported_formats: 
 * 
 * Takes the template caps and returns the subset which is actually 
 * supported by this device. 
 * 
 */

static GstCaps *
gst_directsound_probe_supported_formats (GstDirectSoundSink * dsoundsink,
    const GstCaps * template_caps)
{
  HRESULT hRes;
  DSBUFFERDESC descSecondary;
  WAVEFORMATEX wfx;
  GstCaps *caps;
  GstCaps *tmp, *tmp2;
  LPDIRECTSOUNDBUFFER tmpBuffer;

  caps = gst_caps_copy (template_caps);

  /* 
   * Check availability of digital output by trying to create an SPDIF buffer 
   */

#ifdef WAVE_FORMAT_DOLBY_AC3_SPDIF
  /* fill the WAVEFORMATEX structure with some standard AC3 over SPDIF params */
  memset (&wfx, 0, sizeof (wfx));
  wfx.cbSize = 0;
  wfx.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
  wfx.nChannels = 2;
  wfx.nSamplesPerSec = 48000;
  wfx.wBitsPerSample = 16;
  wfx.nBlockAlign = 4;
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

  // create a secondary directsound buffer 
  memset (&descSecondary, 0, sizeof (DSBUFFERDESC));
  descSecondary.dwSize = sizeof (DSBUFFERDESC);
  descSecondary.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
  descSecondary.dwBufferBytes = 6144;
  descSecondary.lpwfxFormat = &wfx;

  hRes = IDirectSound_CreateSoundBuffer (dsoundsink->pDS, &descSecondary,
      &tmpBuffer, NULL);
  if (FAILED (hRes)) {
    GST_INFO_OBJECT (dsoundsink, "AC3 passthrough not supported "
        "(IDirectSound_CreateSoundBuffer returned: %s)\n",
        DXGetErrorString9 (hRes));
    tmp = gst_caps_new_simple ("audio/x-ac3", NULL);
    tmp2 = gst_caps_subtract (caps, tmp);
    gst_caps_unref (tmp);
    gst_caps_unref (caps);
    caps = tmp2;
    tmp = gst_caps_new_simple ("audio/x-dts", NULL);
    tmp2 = gst_caps_subtract (caps, tmp);
    gst_caps_unref (tmp);
    gst_caps_unref (caps);
    caps = tmp2;
  } else {
    GST_INFO_OBJECT (dsoundsink, "AC3 passthrough supported");
    hRes = IDirectSoundBuffer_Release (tmpBuffer);
    if (FAILED (hRes)) {
      GST_DEBUG_OBJECT (dsoundsink,
          "(IDirectSoundBuffer_Release returned: %s)\n",
          DXGetErrorString9 (hRes));
    }
  }
#else
  tmp = gst_caps_new_simple ("audio/x-ac3", NULL);
  tmp2 = gst_caps_subtract (caps, tmp);
  gst_caps_unref (tmp);
  gst_caps_unref (caps);
  caps = tmp2;
  tmp = gst_caps_new_simple ("audio/x-dts", NULL);
  tmp2 = gst_caps_subtract (caps, tmp);
  gst_caps_unref (tmp);
  gst_caps_unref (caps);
  caps = tmp2;
#endif

  return caps;
}

static GstBuffer *
gst_directsound_sink_payload (GstBaseAudioSink * sink, GstBuffer * buf)
{
  if (gst_directsound_sink_is_spdif_format ((GstDirectSoundSink *) sink)) {
    gint framesize = gst_audio_iec61937_frame_size (&sink->ringbuffer->spec);
    GstBuffer *out;

    if (framesize <= 0)
      return NULL;

    out = gst_buffer_new_and_alloc (framesize);

    if (!gst_audio_iec61937_payload (GST_BUFFER_DATA (buf),
            GST_BUFFER_SIZE (buf), GST_BUFFER_DATA (out),
            GST_BUFFER_SIZE (out), &sink->ringbuffer->spec)) {
      gst_buffer_unref (out);
      return NULL;
    }

    gst_buffer_copy_metadata (out, buf, GST_BUFFER_COPY_ALL);
    /* Fix endianness */
    _swab ((gchar *) GST_BUFFER_DATA (buf), (gchar *) GST_BUFFER_DATA (buf),
        GST_BUFFER_SIZE (buf));
    return out;
  } else {
    return gst_buffer_ref (buf);
  }
}
