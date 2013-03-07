/*
 * GStreamer
 * Copyright (C) 2012 Fluendo S.A. <support@fluendo.com>
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
 */

#ifndef __GST_CORE_AUDIO_H__
#define __GST_CORE_AUDIO_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#ifdef HAVE_IOS
  #include <CoreAudio/CoreAudioTypes.h>
  #define AudioDeviceID gint
  #define kAudioDeviceUnknown 0
#else
  #include <CoreAudio/CoreAudio.h>
  #include <AudioToolbox/AudioToolbox.h>
  #if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_5
    #include <CoreServices/CoreServices.h>
    #define AudioComponentFindNext FindNextComponent
    #define AudioComponentInstanceNew OpenAComponent
    #define AudioComponentInstanceDispose CloseComponent
    #define AudioComponent Component
    #define AudioComponentDescription ComponentDescription
  #endif
#endif
#include <AudioUnit/AudioUnit.h>
#include "gstosxaudioelement.h"


G_BEGIN_DECLS

#define GST_TYPE_CORE_AUDIO \
  (gst_core_audio_get_type())
#define GST_CORE_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CORE_AUDIO,GstCoreAudio))
#define GST_CORE_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CORE_AUDIO,GstCoreAudioClass))
#define GST_CORE_AUDIO_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_CORE_AUDIO,GstCoreAudioClass))
#define GST_IS_CORE_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CORE_AUDIO))
#define GST_IS_CORE_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CORE_AUDIO))

#define CORE_AUDIO_FORMAT_IS_SPDIF(f) ((f).mFormat.mFormatID == 'IAC3' || (f).mFormat.mFormatID == 'iac3' || (f).mFormat.mFormatID == kAudioFormat60958AC3 || (f).mFormat.mFormatID == kAudioFormatAC3)

#define CORE_AUDIO_FORMAT "FormatID: %" GST_FOURCC_FORMAT " rate: %f flags: 0x%x BytesPerPacket: %u FramesPerPacket: %u BytesPerFrame: %u ChannelsPerFrame: %u BitsPerChannel: %u"
#define CORE_AUDIO_FORMAT_ARGS(f) GST_FOURCC_ARGS((f).mFormatID),(f).mSampleRate,(unsigned)(f).mFormatFlags,(unsigned)(f).mBytesPerPacket,(unsigned)(f).mFramesPerPacket,(unsigned)(f).mBytesPerFrame,(unsigned)(f).mChannelsPerFrame,(unsigned)(f).mBitsPerChannel

typedef struct _GstCoreAudio GstCoreAudio;
typedef struct _GstCoreAudioClass GstCoreAudioClass;

struct _GstCoreAudio
{
  GObject object;

  GstObject *osxbuf;
  GstOsxAudioElementInterface *element;

  gboolean is_src;
  gboolean is_passthrough;
  AudioDeviceID device_id;
  AudioStreamBasicDescription stream_format;
  gint stream_idx;
  gboolean io_proc_active;
  gboolean io_proc_needs_deactivation;

  /* For LPCM in/out */
  AudioUnit audiounit;
  AudioBufferList *recBufferList;

#ifndef HAVE_IOS
  /* For SPDIF out */
  pid_t hog_pid;
  gboolean disabled_mixing;
  AudioStreamID stream_id;
  gboolean revert_format;
  AudioStreamBasicDescription original_format;
  AudioDeviceIOProcID procID;
#endif
};

struct _GstCoreAudioClass
{
  GObjectClass parent_class;
};

GType gst_core_audio_get_type                                (void);

void gst_core_audio_init_debug (void);

GstCoreAudio * gst_core_audio_new                            (GstObject *osxbuf);

gboolean gst_core_audio_open                                 (GstCoreAudio *core_audio);

gboolean gst_core_audio_close                                (GstCoreAudio *core_audio);

gboolean gst_core_audio_initialize                           (GstCoreAudio *core_audio,
                                                              AudioStreamBasicDescription format,
                                                              GstCaps *caps,
                                                              gboolean is_passthrough);

void gst_core_audio_unitialize                               (GstCoreAudio *core_audio);

gboolean gst_core_audio_start_processing                     (GstCoreAudio *core_audio);

gboolean gst_core_audio_pause_processing                     (GstCoreAudio *core_audio);

gboolean gst_core_audio_stop_processing                      (GstCoreAudio *core_audio);

gboolean gst_core_audio_get_samples_and_latency              (GstCoreAudio * core_audio,
                                                              gdouble rate,
                                                              guint *samples,
                                                              gdouble *latency);

void  gst_core_audio_set_volume                              (GstCoreAudio *core_audio,
                                                              gfloat volume);

gboolean gst_core_audio_audio_device_is_spdif_avail          (AudioDeviceID device_id);


gboolean gst_core_audio_select_device                        (AudioDeviceID *device_id);

gboolean gst_core_audio_select_source_device                        (AudioDeviceID *device_id);

AudioChannelLayout * gst_core_audio_audio_device_get_channel_layout (AudioDeviceID device_id);


G_END_DECLS

#endif /* __GST_CORE_AUDIO_H__ */
