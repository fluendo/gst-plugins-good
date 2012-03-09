/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
#include "config.h"
#endif
#include "gstmikmod.h"

#include <stdlib.h>

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_SONGNAME,
  ARG_MODTYPE,
  ARG_MUSICVOLUME,
  ARG_PANSEP,
  ARG_REVERB,
  ARG_SNDFXVOLUME,
  ARG_VOLUME,
  ARG_INTERP,
  ARG_REVERSE,
  ARG_SURROUND,
  ARG_HQMIXER,
  ARG_SOFT_MUSIC,
  ARG_SOFT_SNDFX
};

MODULE *module;
MREADER *reader;
GstPad *srcpad;
GstClockTime timestamp;
int need_sync;

static GstStaticPadTemplate mikmod_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) BYTE_ORDER, "
        "signed = (boolean) TRUE, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "rate = (int) { 8000, 11025, 22050, 44100 }, "
        "channels = (int) [ 1, 2 ]; "
        "audio/x-raw-int, "
        "endianness = (int) BYTE_ORDER, "
        "signed = (boolean) FALSE, "
        "width = (int) 8, "
        "depth = (int) 8, "
        "rate = (int) { 8000, 11025, 22050, 44100 }, "
        "channels = (int) [ 1, 2 ]")
    );

static GstStaticPadTemplate mikmod_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-mod")
    );

static void gst_mikmod_base_init (gpointer g_class);
static void gst_mikmod_class_init (GstMikModClass * klass);
static void gst_mikmod_init (GstMikMod * filter);
static void gst_mikmod_set_property (GObject * object, guint id,
    const GValue * value, GParamSpec * pspec);
static void gst_mikmod_get_property (GObject * object, guint id, GValue * value,
    GParamSpec * pspec);
static GstPadLinkReturn gst_mikmod_srclink (GstPad * pad, const GstCaps * caps);
static GstCaps *gst_mikmod_srcfixate (GstPad * pad, const GstCaps * caps);
static void gst_mikmod_loop (GstElement * element);
static gboolean gst_mikmod_setup (GstMikMod * mikmod);
static GstStateChangeReturn gst_mikmod_change_state (GstElement * element,
    GstStateChange transition);



static GstElementClass *parent_class = NULL;

GType
gst_mikmod_get_type (void)
{
  static GType mikmod_type = 0;

  if (!mikmod_type) {
    static const GTypeInfo mikmod_info = {
      sizeof (GstMikModClass),
      gst_mikmod_base_init,
      NULL,
      (GClassInitFunc) gst_mikmod_class_init,
      NULL,
      NULL,
      sizeof (GstMikMod),
      0,
      (GInstanceInitFunc) gst_mikmod_init,
    };

    mikmod_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstMikmod", &mikmod_info, 0);
  }
  return mikmod_type;
}

static void
gst_mikmod_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class,
      &mikmod_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &mikmod_sink_factory);
  gst_element_class_set_details_simple (element_class, "MikMod audio decoder",
      "Codec/Decoder/Audio",
      "Module decoder based on libmikmod", "Jeremy SIMON <jsimon13@yahoo.fr>");
}

static void
gst_mikmod_class_init (GstMikModClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SONGNAME,
      g_param_spec_string ("songname", "songname", "songname",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MODTYPE,
      g_param_spec_string ("modtype", "modtype", "modtype",
          NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MUSICVOLUME,
      g_param_spec_int ("musicvolume", "musivolume", "musicvolume",
          0, 128, 128, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PANSEP,
      g_param_spec_int ("pansep", "pansep", "pansep",
          0, 128, 128, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_REVERB,
      g_param_spec_int ("reverb", "reverb", "reverb",
          0, 15, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SNDFXVOLUME,
      g_param_spec_int ("sndfxvolume", "sndfxvolume", "sndfxvolume",
          0, 128, 128, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VOLUME,
      g_param_spec_int ("volume", "volume", "volume",
          0, 128, 96, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_INTERP,
      g_param_spec_boolean ("interp", "interp", "interp",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_REVERSE,
      g_param_spec_boolean ("reverse", "reverse", "reverse",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SURROUND,
      g_param_spec_boolean ("surround", "surround", "surround",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_HQMIXER,
      g_param_spec_boolean ("hqmixer", "hqmixer", "hqmixer",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SOFT_MUSIC,
      g_param_spec_boolean ("soft-music", "soft music", "soft music",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_SOFT_SNDFX,
      g_param_spec_boolean ("soft-sndfx", "soft sndfx", "soft sndfx",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gobject_class->set_property = gst_mikmod_set_property;
  gobject_class->get_property = gst_mikmod_get_property;

  gstelement_class->change_state = gst_mikmod_change_state;
}


static void
gst_mikmod_init (GstMikMod * filter)
{
  filter->sinkpad =
      gst_pad_new_from_static_template (&mikmod_sink_factory, "sink");
  filter->srcpad =
      gst_pad_new_from_static_template (&mikmod_src_factory, "src");

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  gst_pad_set_link_function (filter->srcpad, gst_mikmod_srclink);
  gst_pad_set_fixate_function (filter->srcpad, gst_mikmod_srcfixate);

  gst_element_set_loop_function (GST_ELEMENT (filter), gst_mikmod_loop);

  filter->Buffer = NULL;

  filter->stereo = TRUE;
  filter->surround = TRUE;
  filter->_16bit = TRUE;
  filter->soft_music = TRUE;
  filter->soft_sndfx = TRUE;
  filter->mixfreq = 44100;
  filter->reverb = 0;
  filter->pansep = 128;
  filter->musicvolume = 128;
  filter->volume = 96;
  filter->sndfxvolume = 128;
  filter->songname = NULL;
  filter->modtype = NULL;

  filter->initialized = FALSE;
}

static GstCaps *
gst_mikmod_srcfixate (GstPad * pad, const GstCaps * caps)
{
  GstCaps *ret;
  GstStructure *structure;

  /* FIXME: select est caps here */
  if (gst_caps_get_size (caps) > 1)
    return NULL;

  ret = gst_caps_copy (caps);
  structure = gst_caps_get_structure (ret, 0);

  if (gst_structure_fixate_field_nearest_int (structure, "channels", 2))
    return ret;
  if (gst_structure_fixate_field_nearest_int (structure, "rate", 44100))
    return ret;

  gst_caps_free (ret);
  return NULL;
}

static GstPadLinkReturn
gst_mikmod_srclink (GstPad * pad, const GstCaps * caps)
{
  GstMikMod *filter;
  GstStructure *structure;
  gint depth;
  gint channels;
  gboolean result;

  filter = GST_MIKMOD (gst_pad_get_parent (pad));

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_get_int (structure, "depth", &depth);
  filter->_16bit = (depth == 16);
  gst_structure_get_int (structure, "channels", &channels);
  filter->stereo = (channels == 2);
  gst_structure_get_int (structure, "rate", &filter->mixfreq);

  result = gst_mikmod_setup (filter);
  gst_object_unref (filter);

  if (result) {
    return GST_PAD_LINK_OK;
  } else {
    return GST_PAD_LINK_REFUSED;
  }
}

static void
gst_mikmod_loop (GstElement * element)
{
  GstMikMod *mikmod;
  GstBuffer *buffer_in;

  g_return_if_fail (element != NULL);
  g_return_if_fail (GST_IS_MIKMOD (element));

  mikmod = GST_MIKMOD (element);
  srcpad = mikmod->srcpad;
  mikmod->Buffer = NULL;

  if (!mikmod->initialized) {
    while ((buffer_in = GST_BUFFER (gst_pad_pull (mikmod->sinkpad)))) {
      if (GST_IS_EVENT (buffer_in)) {
        GstEvent *event = GST_EVENT (buffer_in);

        if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
          break;
      } else {
        if (mikmod->Buffer) {
          GstBuffer *merge;

          merge = gst_buffer_merge (mikmod->Buffer, buffer_in);
          gst_buffer_unref (buffer_in);
          gst_buffer_unref (mikmod->Buffer);
          mikmod->Buffer = merge;
        } else {
          mikmod->Buffer = buffer_in;
        }
      }
    }

    if (!GST_PAD_CAPS (mikmod->srcpad)) {
      if (GST_PAD_LINK_SUCCESSFUL (gst_pad_renegotiate (mikmod->srcpad))) {
        GST_ELEMENT_ERROR (mikmod, CORE, NEGOTIATION, (NULL), (NULL));
        return;
      }
    }

    MikMod_RegisterDriver (&drv_gst);
    MikMod_RegisterAllLoaders ();

    MikMod_Init ("");
    reader = GST_READER_new (mikmod);
    module = Player_LoadGeneric (reader, 64, 0);

    gst_buffer_unref (mikmod->Buffer);

    if (!Player_Active ())
      Player_Start (module);

    mikmod->initialized = TRUE;
  }

  if (Player_Active ()) {
    timestamp = (module->sngtime / 1024.0) * GST_SECOND;
    drv_gst.Update ();
  } else {
    gst_element_set_eos (GST_ELEMENT (mikmod));
    gst_pad_push (mikmod->srcpad, GST_DATA (gst_event_new (GST_EVENT_EOS)));
  }
}


static gboolean
gst_mikmod_setup (GstMikMod * mikmod)
{
  md_musicvolume = mikmod->musicvolume;
  md_pansep = mikmod->pansep;
  md_reverb = mikmod->reverb;
  md_sndfxvolume = mikmod->sndfxvolume;
  md_volume = mikmod->volume;
  md_mixfreq = mikmod->mixfreq;

  md_mode = 0;

  if (mikmod->interp)
    md_mode = md_mode | DMODE_INTERP;

  if (mikmod->reverse)
    md_mode = md_mode | DMODE_REVERSE;

  if (mikmod->surround)
    md_mode = md_mode | DMODE_SURROUND;

  if (mikmod->_16bit)
    md_mode = md_mode | DMODE_16BITS;

  if (mikmod->hqmixer)
    md_mode = md_mode | DMODE_HQMIXER;

  if (mikmod->soft_music)
    md_mode = md_mode | DMODE_SOFT_MUSIC;

  if (mikmod->soft_sndfx)
    md_mode = md_mode | DMODE_SOFT_SNDFX;

  if (mikmod->stereo)
    md_mode = md_mode | DMODE_STEREO;

  return TRUE;
}


static GstStateChangeReturn
gst_mikmod_change_state (GstElement * element, GstStateChange transition)
{
  GstMikMod *mikmod;

  g_return_val_if_fail (GST_IS_MIKMOD (element), GST_STATE_CHANGE_FAILURE);

  mikmod = GST_MIKMOD (element);

  GST_DEBUG ("state pending %d", GST_STATE_PENDING (element));

  if (GST_STATE_PENDING (element) == GST_STATE_READY) {
    gst_mikmod_setup (mikmod);

    if (Player_Active ()) {
      Player_TogglePause ();
      Player_SetPosition (0);
    }
    mikmod->initialized = FALSE;
  }

  if (GST_STATE_PENDING (element) == GST_STATE_PLAYING) {
    if (Player_Active () && Player_Paused ())
      Player_TogglePause ();
    else if (!Player_Active ())
      Player_Start (module);

  }

  if (GST_STATE_PENDING (element) == GST_STATE_PAUSED)
    if (Player_Active () && !Player_Paused ())
      Player_TogglePause ();

  if (GST_STATE_PENDING (element) == GST_STATE_NULL)
    MikMod_Exit ();


  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return GST_STATE_CHANGE_SUCCESS;
}



static void
gst_mikmod_set_property (GObject * object, guint id, const GValue * value,
    GParamSpec * pspec)
{
  GstMikMod *filter;

  g_return_if_fail (GST_IS_MIKMOD (object));
  filter = GST_MIKMOD (object);

  switch (id) {
    case ARG_SONGNAME:
      g_free (filter->songname);
      filter->songname = g_strdup (g_value_get_string (value));
      break;
    case ARG_MODTYPE:
      g_free (filter->modtype);
      filter->modtype = g_strdup (g_value_get_string (value));
      break;
    case ARG_MUSICVOLUME:
      filter->musicvolume = g_value_get_int (value);
      break;
    case ARG_PANSEP:
      filter->pansep = g_value_get_int (value);
      break;
    case ARG_REVERB:
      filter->reverb = g_value_get_int (value);
      break;
    case ARG_SNDFXVOLUME:
      filter->sndfxvolume = g_value_get_int (value);
      break;
    case ARG_VOLUME:
      filter->volume = g_value_get_int (value);
      break;
    case ARG_INTERP:
      filter->interp = g_value_get_boolean (value);
      break;
    case ARG_REVERSE:
      filter->reverse = g_value_get_boolean (value);
      break;
    case ARG_SURROUND:
      filter->surround = g_value_get_boolean (value);
      break;
    case ARG_HQMIXER:
      filter->hqmixer = g_value_get_boolean (value);
      break;
    case ARG_SOFT_MUSIC:
      filter->soft_music = g_value_get_boolean (value);
      break;
    case ARG_SOFT_SNDFX:
      filter->soft_sndfx = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_mikmod_get_property (GObject * object, guint id, GValue * value,
    GParamSpec * pspec)
{
  GstMikMod *filter;

  g_return_if_fail (GST_IS_MIKMOD (object));
  filter = GST_MIKMOD (object);

  switch (id) {
    case ARG_MUSICVOLUME:
      g_value_set_int (value, filter->musicvolume);
      break;
    case ARG_PANSEP:
      g_value_set_int (value, filter->pansep);
      break;
    case ARG_REVERB:
      g_value_set_int (value, filter->reverb);
      break;
    case ARG_SNDFXVOLUME:
      g_value_set_int (value, filter->sndfxvolume);
      break;
    case ARG_VOLUME:
      g_value_set_int (value, filter->volume);
      break;
    case ARG_INTERP:
      g_value_set_boolean (value, filter->interp);
      break;
    case ARG_REVERSE:
      g_value_set_boolean (value, filter->reverse);
      break;
    case ARG_SURROUND:
      g_value_set_boolean (value, filter->surround);
      break;
    case ARG_HQMIXER:
      g_value_set_boolean (value, filter->hqmixer);
      break;
    case ARG_SOFT_MUSIC:
      g_value_set_boolean (value, filter->soft_music);
      break;
    case ARG_SOFT_SNDFX:
      g_value_set_boolean (value, filter->soft_sndfx);
      break;
    default:
      break;
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "mikmod", GST_RANK_SECONDARY,
          GST_TYPE_MIKMOD))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE2 (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    mikmod,
    "Mikmod plugin library",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
