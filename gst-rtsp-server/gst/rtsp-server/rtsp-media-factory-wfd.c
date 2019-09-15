/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:rtsp-media-factory
 * @short_description: A factory for media pipelines
 * @see_also: #GstRTSPMountPoints, #GstRTSPMedia
 *
 * The #GstRTSPMediaFactoryWFD is responsible for creating or recycling
 * #GstRTSPMedia objects based on the passed URL.
 *
 * The default implementation of the object can create #GstRTSPMedia objects
 * containing a pipeline created from a launch description set with
 * gst_rtsp_media_factory_wfd_set_launch().
 *
 * Media from a factory can be shared by setting the shared flag with
 * gst_rtsp_media_factory_wfd_set_shared(). When a factory is shared,
 * gst_rtsp_media_factory_wfd_construct() will return the same #GstRTSPMedia when
 * the url matches.
 *
 * Last reviewed on 2013-07-11 (1.0.0)
 */

#include <stdio.h>
#include "rtsp-media-factory-wfd.h"
#include "gstwfdmessage.h"
#ifdef COMMERCIAL_FEATURE
#include <stdio.h>
#include <gst/app/gstappsrc.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include "wfdhalconfig.h"
#define LIB_TIZEN_WFD_HAL "libwfdhalconfig.so"
#endif

#define GST_RTSP_MEDIA_FACTORY_WFD_GET_PRIVATE(obj)  \
       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_RTSP_MEDIA_FACTORY_WFD, GstRTSPMediaFactoryWFDPrivate))

#define GST_RTSP_MEDIA_FACTORY_WFD_GET_LOCK(f)       (&(GST_RTSP_MEDIA_FACTORY_WFD_CAST(f)->priv->lock))
#define GST_RTSP_MEDIA_FACTORY_WFD_LOCK(f)           (g_mutex_lock(GST_RTSP_MEDIA_FACTORY_WFD_GET_LOCK(f)))
#define GST_RTSP_MEDIA_FACTORY_WFD_UNLOCK(f)         (g_mutex_unlock(GST_RTSP_MEDIA_FACTORY_WFD_GET_LOCK(f)))

struct _GstRTSPMediaFactoryWFDPrivate
{
  GMutex lock;
  GstRTSPPermissions *permissions;
  gchar *launch;
  gboolean shared;
  GstRTSPLowerTrans protocols;
  guint buffer_size;
  guint mtu_size;
  guint8 videosrc_type;
  guint8 video_codec;
  gchar *video_encoder;
  guint video_bitrate;
  guint video_width;
  guint video_height;
  guint video_framerate;
  guint video_enc_skip_inbuf_value;
  GstElement *video_queue;

  gchar *audio_device;
  gchar *audio_encoder_aac;
  gchar *audio_encoder_ac3;
  guint8 audio_codec;
  guint64 audio_latency_time;
  guint64 audio_buffer_time;
  gboolean audio_do_timestamp;
  guint8 audio_channels;
  guint8 audio_freq;
  guint8 audio_bitrate;
  GstElement *audio_queue;
#ifdef COMMERCIAL_FEATURE
  gchar *file_path;
  gint media_mode;
  GstBin *srcbin;
  GstElement *filesrc;
  GstElement *xvimagesrc;
  GstElement *audio_src;
  GstElement *imagecast_bin;
  GstPad *imagecast_pad;
  GstElement *videosrc_bin;
  GstPad *videosrc_pad;
  GstPad *parser_pad;
#endif
  guint64 video_resolution_supported;

  gboolean dump_ts;
#ifdef COMMERCIAL_FEATURE
  struct {
    void *dl_handle;
    void *data;
    wfd_hal_interface_t intf;
  } wfd_hal_mgr;
  gboolean  hdcp_enabled;
  gboolean  content_protection;
#endif
};

#define DEFAULT_LAUNCH          NULL
#define DEFAULT_SHARED          FALSE
#define DEFAULT_PROTOCOLS       GST_RTSP_LOWER_TRANS_UDP | GST_RTSP_LOWER_TRANS_UDP_MCAST | \
                                        GST_RTSP_LOWER_TRANS_TCP
#define DEFAULT_BUFFER_SIZE     0x80000

enum
{
  PROP_0,
  PROP_LAUNCH,
  PROP_SHARED,
  PROP_SUSPEND_MODE,
  PROP_EOS_SHUTDOWN,
  PROP_PROTOCOLS,
  PROP_BUFFER_SIZE,
  PROP_LAST
};

enum
{
  SIGNAL_MEDIA_CONSTRUCTED,
  SIGNAL_MEDIA_CONFIGURE,
  SIGNAL_LAST
};

GST_DEBUG_CATEGORY_STATIC (rtsp_media_wfd_debug);
#define GST_CAT_DEFAULT rtsp_media_wfd_debug

static void gst_rtsp_media_factory_wfd_get_property (GObject * object,
    guint propid, GValue * value, GParamSpec * pspec);
static void gst_rtsp_media_factory_wfd_set_property (GObject * object,
    guint propid, const GValue * value, GParamSpec * pspec);

static void gst_rtsp_media_factory_wfd_finalize (GObject * obj);


static GstElement *rtsp_media_factory_wfd_create_element (GstRTSPMediaFactory *
    factory, const GstRTSPUrl * url);
static GstRTSPMedia *rtsp_media_factory_wfd_construct (GstRTSPMediaFactory *
    factory, const GstRTSPUrl * url);
#ifdef COMMERCIAL_FEATURE
static gboolean
_rtsp_media_factory_wfd_switch_video_capture_bin (GstRTSPMediaFactoryWFD * factory, GstBin * srcbin, gint media_mode);
#endif
G_DEFINE_TYPE (GstRTSPMediaFactoryWFD, gst_rtsp_media_factory_wfd,
    GST_TYPE_RTSP_MEDIA_FACTORY);

static void
gst_rtsp_media_factory_wfd_class_init (GstRTSPMediaFactoryWFDClass * klass)
{
  GObjectClass *gobject_class;
  GstRTSPMediaFactoryClass *factory_class;

  g_type_class_add_private (klass, sizeof (GstRTSPMediaFactoryWFDPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  factory_class = GST_RTSP_MEDIA_FACTORY_CLASS (klass);

  gobject_class->get_property = gst_rtsp_media_factory_wfd_get_property;
  gobject_class->set_property = gst_rtsp_media_factory_wfd_set_property;
  gobject_class->finalize = gst_rtsp_media_factory_wfd_finalize;

  factory_class->construct = rtsp_media_factory_wfd_construct;
  factory_class->create_element = rtsp_media_factory_wfd_create_element;

  GST_DEBUG_CATEGORY_INIT (rtsp_media_wfd_debug, "rtspmediafactorywfd", 0,
      "GstRTSPMediaFactoryWFD");
}

void  gst_rtsp_media_factory_wfd_set (GstRTSPMediaFactoryWFD * factory,
    guint8 videosrc_type, gchar *audio_device, guint64 audio_latency_time,
    guint64 audio_buffer_time, gboolean audio_do_timestamp, guint mtu_size)
{
  GstRTSPMediaFactoryWFDPrivate *priv =
      GST_RTSP_MEDIA_FACTORY_WFD_GET_PRIVATE (factory);
  factory->priv = priv;

  priv->videosrc_type = videosrc_type;
  priv->audio_device = audio_device;
  priv->audio_latency_time = audio_latency_time;
  priv->audio_buffer_time = audio_buffer_time;
  priv->audio_do_timestamp = audio_do_timestamp;
  priv->mtu_size = mtu_size;
}

void  gst_rtsp_media_factory_wfd_set_encoders (GstRTSPMediaFactoryWFD * factory,
    gchar *video_encoder, gchar *audio_encoder_aac, gchar *audio_encoder_ac3)
{
  GstRTSPMediaFactoryWFDPrivate *priv =
      GST_RTSP_MEDIA_FACTORY_WFD_GET_PRIVATE (factory);
  factory->priv = priv;

  priv->video_encoder = video_encoder;
  priv->audio_encoder_aac = audio_encoder_aac;
  priv->audio_encoder_ac3 = audio_encoder_ac3;
}

void  gst_rtsp_media_factory_wfd_set_dump_ts (GstRTSPMediaFactoryWFD * factory,
    gboolean dump_ts)
{
  GstRTSPMediaFactoryWFDPrivate *priv =
      GST_RTSP_MEDIA_FACTORY_WFD_GET_PRIVATE (factory);
  factory->priv = priv;

  priv->dump_ts = dump_ts;
}
void gst_rtsp_media_factory_wfd_set_negotiated_resolution (GstRTSPMediaFactory *factory,
   guint32 width, guint32 height)
{
  GstRTSPMediaFactoryWFD *factory_wfd = GST_RTSP_MEDIA_FACTORY_WFD (factory);
  GstRTSPMediaFactoryWFDPrivate *priv = factory_wfd->priv;

  priv->video_width = width;
  priv->video_height = height;
}
void gst_rtsp_media_factory_wfd_set_audio_codec (GstRTSPMediaFactory *factory,
   guint audio_codec)
{
  GstRTSPMediaFactoryWFD *factory_wfd = GST_RTSP_MEDIA_FACTORY_WFD (factory);
  GstRTSPMediaFactoryWFDPrivate *priv = factory_wfd->priv;

  priv->audio_codec = audio_codec;
}

#ifdef COMMERCIAL_FEATURE
void gst_rtsp_media_factory_wfd_set_content_protection(GstRTSPMediaFactory *factory,gboolean content_enabled)
{
  GstRTSPMediaFactoryWFD *factory_wfd = GST_RTSP_MEDIA_FACTORY_WFD (factory);
  GstRTSPMediaFactoryWFDPrivate *priv = factory_wfd->priv;
  priv->content_protection = content_enabled;
}

void gst_rtsp_media_factory_wfd_set_media_mode(GstRTSPMediaFactoryWFD *factory, int media_mode, char *file_path)
{
  GstRTSPMediaFactoryWFDPrivate *priv = factory->priv;

  GST_DEBUG("gst_rtsp_media_factory_wfd_set_media_mode set media mode as %d", media_mode);
  switch(media_mode){
    case 0:
    case 1:
      {
	  if(priv->media_mode == -1){
            priv->media_mode = media_mode;
            if(file_path) {
              priv->file_path = file_path;
              if(priv->filesrc) {
                GST_DEBUG("priv->file_path[%s]",priv->file_path);
                g_object_set (priv->filesrc, "location", priv->file_path, NULL);
              }
	    }
            break;
          } else if((priv->media_mode == 0) || (priv->media_mode == 1)) {
            priv->media_mode = media_mode;
            GST_DEBUG("Already in the same mode");
          } else {
            priv->media_mode = media_mode;
            _rtsp_media_factory_wfd_switch_video_capture_bin(factory, priv->srcbin, priv->media_mode);
            GST_ERROR("Switched to Imagecast pipeline mode");
          }
          if(file_path) {
            GST_DEBUG("gst_rtsp_media_factory_wfd_set_media_mode set filepath: %s", file_path);
            priv->file_path = file_path;
            if(priv->filesrc) {
              g_object_set (priv->filesrc, "location", priv->file_path, NULL);
            }
          }
      }
      break;
    case 2:
    default:
      {
	  if(priv->media_mode == -1) {
            priv->media_mode = media_mode;
            break;
          } else if(priv->media_mode == 2) {
            priv->media_mode = media_mode;
            GST_DEBUG("Already in the same mode");
          } else {
            priv->media_mode = media_mode;
            _rtsp_media_factory_wfd_switch_video_capture_bin(factory, priv->srcbin, priv->media_mode);
            GST_ERROR("Switched to Video pipeline mode");
          }
      }
      break;
  }
}
#endif

static void
gst_rtsp_media_factory_wfd_init (GstRTSPMediaFactoryWFD * factory)
{
  GstRTSPMediaFactoryWFDPrivate *priv =
      GST_RTSP_MEDIA_FACTORY_WFD_GET_PRIVATE (factory);
  factory->priv = priv;

  priv->launch = g_strdup (DEFAULT_LAUNCH);
  priv->shared = DEFAULT_SHARED;
  priv->protocols = DEFAULT_PROTOCOLS;
  priv->buffer_size = DEFAULT_BUFFER_SIZE;
#ifdef COMMERCIAL_FEATURE
  priv->media_mode = -1;
  priv->file_path = NULL;
  priv->video_queue = NULL;
  priv->videosrc_bin = NULL;
#endif
  //priv->videosrc_type = GST_WFD_VSRC_XIMAGESRC;
  //priv->videosrc_type = GST_WFD_VSRC_XVIMAGESRC;
  //priv->videosrc_type = GST_WFD_VSRC_CAMERASRC;
  priv->videosrc_type = GST_WFD_VSRC_VIDEOTESTSRC;
  priv->video_codec = GST_WFD_VIDEO_H264;
  priv->video_encoder = g_strdup ("omxh264enc");
  priv->video_bitrate = 200000;
  priv->video_width = 640;
  priv->video_height = 480;
  priv->video_framerate = 30;
  priv->video_enc_skip_inbuf_value = 5;

  priv->audio_device = g_strdup ("alsa_output.1.analog-stereo.monitor");
  priv->audio_codec = GST_WFD_AUDIO_AAC;
  priv->audio_encoder_aac = g_strdup ("avenc_aac");
  priv->audio_encoder_ac3 = g_strdup ("avenc_ac3");
  priv->audio_latency_time = 10000;
  priv->audio_buffer_time = 200000;
  priv->audio_do_timestamp = FALSE;
  priv->audio_channels = GST_WFD_CHANNEL_2;
  priv->audio_freq = GST_WFD_FREQ_44100;

#ifdef COMMERCIAL_FEATURE
  priv->wfd_hal_mgr.dl_handle = dlopen(LIB_TIZEN_WFD_HAL, RTLD_NOW);
  if (priv->wfd_hal_mgr.dl_handle) {
    priv->wfd_hal_mgr.intf.is_hdcp_supported = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_is_hdcp_supported");
    priv->wfd_hal_mgr.intf.get_audiosrc_type = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_audiosrc_type");
    priv->wfd_hal_mgr.intf.get_audiosrc_config = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_audiosrc_config");
    priv->wfd_hal_mgr.intf.get_audio_encoder_type = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_audio_encoder_type");
    priv->wfd_hal_mgr.intf.get_audio_encoder_config = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_audio_encoder_config");
    priv->wfd_hal_mgr.intf.get_video_encoder_config = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_video_encoder_config");
    priv->wfd_hal_mgr.intf.get_video_encoder_type = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_video_encoder_type");
    priv->wfd_hal_mgr.intf.get_hardware_color_converter_type = dlsym(priv->wfd_hal_mgr.dl_handle, "wfd_hal_get_hardware_color_converter_type");
    if (priv->wfd_hal_mgr.intf.is_hdcp_supported) {
      priv->hdcp_enabled = priv->wfd_hal_mgr.intf.is_hdcp_supported();
      priv->content_protection = 0;
    }
  } else {
    GST_ERROR("open audio_mgr failed :%s", dlerror());
  }
#endif
  g_mutex_init (&priv->lock);
}

static void
gst_rtsp_media_factory_wfd_finalize (GObject * obj)
{
  GstRTSPMediaFactoryWFD *factory = GST_RTSP_MEDIA_FACTORY_WFD (obj);
  GstRTSPMediaFactoryWFDPrivate *priv = factory->priv;

  if (priv->permissions)
    gst_rtsp_permissions_unref (priv->permissions);
  g_free (priv->launch);
  g_mutex_clear (&priv->lock);

  if (priv->audio_device)
    g_free (priv->audio_device);
  if (priv->audio_encoder_aac)
    g_free (priv->audio_encoder_aac);
  if (priv->audio_encoder_ac3)
    g_free (priv->audio_encoder_ac3);

  if (priv->video_encoder)
    g_free (priv->video_encoder);

  G_OBJECT_CLASS (gst_rtsp_media_factory_wfd_parent_class)->finalize (obj);
}

GstRTSPMediaFactoryWFD *
gst_rtsp_media_factory_wfd_new (void)
{
  GstRTSPMediaFactoryWFD *result;

  result = g_object_new (GST_TYPE_RTSP_MEDIA_FACTORY_WFD, NULL);

  return result;
}

static void
gst_rtsp_media_factory_wfd_get_property (GObject * object,
    guint propid, GValue * value, GParamSpec * pspec)
{
  //GstRTSPMediaFactoryWFD *factory = GST_RTSP_MEDIA_FACTORY_WFD (object);

  switch (propid) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
gst_rtsp_media_factory_wfd_set_property (GObject * object,
    guint propid, const GValue * value, GParamSpec * pspec)
{
  //GstRTSPMediaFactoryWFD *factory = GST_RTSP_MEDIA_FACTORY_WFD (object);

  switch (propid) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static GstPadProbeReturn
rtsp_media_wfd_dump_data (GstPad * pad, GstPadProbeInfo *info, gpointer u_data)
{
  guint8 *data;
  gsize size;
  FILE *f;
  GstMapInfo mapinfo;

  if (info->type == (GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_PUSH)) {
    GstBuffer *buffer = gst_pad_probe_info_get_buffer (info);

    gst_buffer_map (buffer, &mapinfo, GST_MAP_READ);
    data = mapinfo.data;
    size = gst_buffer_get_size (buffer);

    f = fopen ("/root/probe.ts", "a");
    if (f != NULL) {
      fwrite (data, size, 1, f);
      fclose (f);
    }
    gst_buffer_unmap (buffer, &mapinfo);
  }

  return GST_PAD_PROBE_OK;
}

static gboolean
_rtsp_media_factory_wfd_create_audio_capture_bin (GstRTSPMediaFactoryWFD *
    factory, GstBin * srcbin)
{
  GstElement *audiosrc = NULL;
  GstElement *acaps = NULL;
  GstElement *acaps2 = NULL;
  GstElement *aenc = NULL;
#ifdef COMMERCIAL_FEATURE
  GstElement *aparse = NULL;
#endif
  GstElement *audio_convert = NULL;
  GstElement *aqueue = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  guint channels = 0;
  gboolean is_enc_req = TRUE;
  guint freq = 0;
  gchar *acodec = NULL;
  GstStructure *props;

  priv = factory->priv;

  /* create audio src element */
#ifdef COMMERCIAL_FEATURE
  audiosrc = gst_element_factory_make (priv->wfd_hal_mgr.intf.get_audiosrc_type(), "audiosrc");
#else
  audiosrc = gst_element_factory_make ("pulsesrc", "audiosrc");
#endif
  if (!audiosrc) {
    GST_ERROR_OBJECT (factory, "failed to create audiosrc element");
    goto create_error;
  }
#ifdef COMMERCIAL_FEATURE
  priv->audio_src = audiosrc;
  props = gst_structure_from_string ("props,media.policy=mirroring", NULL);
  g_object_set (priv->audio_src, "stream-properties", props, NULL);
  gst_structure_free (props);
#endif
  GST_INFO_OBJECT (factory, "audio device : %s", priv->audio_device);
  GST_INFO_OBJECT (factory, "audio latency time  : %"G_GUINT64_FORMAT,
      priv->audio_latency_time);
  GST_INFO_OBJECT (factory, "audio_buffer_time  : %"G_GUINT64_FORMAT,
      priv->audio_buffer_time);
  GST_INFO_OBJECT (factory, "audio_do_timestamp  : %d",
      priv->audio_do_timestamp);

#ifdef COMMERCIAL_FEATURE
  wfd_hal_config_t *wfd_config = NULL;
  guint32 config_list_size = 0;
  priv->wfd_hal_mgr.intf.get_audiosrc_config(&wfd_config, &config_list_size);
  while (config_list_size >0 && wfd_config){
    g_object_set (audiosrc, wfd_config->property_name, wfd_config->value, NULL);
    config_list_size--;
    wfd_config++;
  }
/*  g_object_set (priv->audio_src, "do-timestamp", (gboolean) TRUE, NULL);
  g_object_set (priv->audio_src, "provide-clock", (gboolean) TRUE, NULL);
*/
#else
  g_object_set (audiosrc, "device", priv->audio_device, NULL);
  g_object_set (audiosrc, "buffer-time", (gint64) priv->audio_buffer_time,
      NULL);
  g_object_set (audiosrc, "latency-time", (gint64) priv->audio_latency_time,
      NULL);
  g_object_set (audiosrc, "do-timestamp", (gboolean) priv->audio_do_timestamp,
      NULL);
  g_object_set (audiosrc, "provide-clock", (gboolean) FALSE, NULL);
  g_object_set (audiosrc, "is-live", (gboolean) TRUE, NULL);
#endif

  if (priv->audio_codec == GST_WFD_AUDIO_LPCM) {
    /* To meet miracast certification */
    gint64 block_size = 1920;
    g_object_set (audiosrc, "blocksize", (gint64) block_size, NULL);

    audio_convert = gst_element_factory_make ("capssetter", "audio_convert");
    if (NULL == audio_convert) {
      GST_ERROR_OBJECT (factory, "failed to create audio convert element");
      goto create_error;
    }
    g_object_set (audio_convert, "caps", gst_caps_new_simple("audio/x-lpcm",
              "width", G_TYPE_INT, 16,
              "rate", G_TYPE_INT, 48000,
              "channels", G_TYPE_INT, 2,
              "dynamic_range", G_TYPE_INT, 0,
              "emphasis", G_TYPE_BOOLEAN, FALSE,
              "mute", G_TYPE_BOOLEAN, FALSE, NULL), NULL);
    g_object_set (audio_convert, "join", (gboolean)FALSE, NULL);
    g_object_set (audio_convert, "replace", (gboolean)TRUE, NULL);

    acaps2 = gst_element_factory_make ("capsfilter", "audiocaps2");
    if (NULL == acaps2) {
      GST_ERROR_OBJECT (factory, "failed to create audio capsilfter element");
      goto create_error;
    }
    /* In case of LPCM, uses big endian */
        g_object_set (G_OBJECT (acaps2), "caps",
            gst_caps_new_simple ("audio/x-raw", "format", G_TYPE_STRING, "S16BE",
                /* In case of LPCM, uses big endian */
                "rate", G_TYPE_INT, 48000,
                "channels", G_TYPE_INT, 2, NULL), NULL);
  }

  /* create audio caps element */
  acaps = gst_element_factory_make ("capsfilter", "audiocaps");
  if (NULL == acaps) {
    GST_ERROR_OBJECT (factory, "failed to create audio capsilfter element");
    goto create_error;
  }

  if (priv->audio_channels == GST_WFD_CHANNEL_2)
    channels = 2;
  else if (priv->audio_channels == GST_WFD_CHANNEL_4)
    channels = 4;
  else if (priv->audio_channels == GST_WFD_CHANNEL_6)
    channels = 6;
  else if (priv->audio_channels == GST_WFD_CHANNEL_8)
    channels = 8;
  else
    channels = 2;

  if (priv->audio_freq == GST_WFD_FREQ_44100)
    freq = 44100;
  else if (priv->audio_freq == GST_WFD_FREQ_48000)
    freq = 48000;
  else
    freq = 44100;

  if (priv->audio_codec == GST_WFD_AUDIO_LPCM) {
    g_object_set (G_OBJECT (acaps), "caps",
        gst_caps_new_simple ("audio/x-lpcm", "width", G_TYPE_INT, 16,
            "rate", G_TYPE_INT, 48000,
            "channels", G_TYPE_INT, 2,
            "dynamic_range", G_TYPE_INT, 0,
            "emphasis", G_TYPE_BOOLEAN, FALSE,
            "mute", G_TYPE_BOOLEAN, FALSE, NULL), NULL);
  } else if ((priv->audio_codec == GST_WFD_AUDIO_AAC)
      || (priv->audio_codec == GST_WFD_AUDIO_AC3)) {
    g_object_set (G_OBJECT (acaps), "caps", gst_caps_new_simple ("audio/x-raw",
            "endianness", G_TYPE_INT, 1234, "signed", G_TYPE_BOOLEAN, TRUE,
            "depth", G_TYPE_INT, 16, "rate", G_TYPE_INT, freq, "channels",
            G_TYPE_INT, channels, NULL), NULL);
  }

  if (priv->audio_codec == GST_WFD_AUDIO_AAC) {
#ifdef COMMERCIAL_FEATURE
  acodec = g_strdup(priv->wfd_hal_mgr.intf.get_audio_encoder_type(priv->content_protection));
#else
  acodec = g_strdup (priv->audio_encoder_aac);
#endif
    is_enc_req = TRUE;
  } else if (priv->audio_codec == GST_WFD_AUDIO_AC3) {
    acodec = g_strdup (priv->audio_encoder_ac3);
    is_enc_req = TRUE;
  } else if (priv->audio_codec == GST_WFD_AUDIO_LPCM) {
    GST_DEBUG_OBJECT (factory, "No codec required, raw data will be sent");
    is_enc_req = FALSE;
  } else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  if (is_enc_req) {
    aenc = gst_element_factory_make (acodec, "audioenc");
    if (NULL == aenc) {
      GST_ERROR_OBJECT (factory, "failed to create audio encoder element");
      goto create_error;
    }

#ifdef COMMERCIAL_FEATURE

    wfd_hal_config_t* audio_encoder_conf = NULL;
    guint32 size = 0;

    priv->wfd_hal_mgr.intf.get_audio_encoder_config (&audio_encoder_conf, &size,priv->content_protection);
    while (size >0 && audio_encoder_conf)
    {
      g_object_set (aenc, audio_encoder_conf->property_name, audio_encoder_conf->value, NULL);
      size--;
      audio_encoder_conf++;
    }
    g_object_set (aenc, "bitrare", 48000, NULL);
    g_object_set (aenc, "min-frame-size", 120, NULL);
    aparse = gst_element_factory_make ("aacparse", "audio-parse");
    if (!aparse) {
      GST_ERROR_OBJECT (factory, "failed to create audio parse element");
      goto create_error;
    }
#else
    g_object_set (aenc, "compliance", -2, NULL);
    g_object_set (aenc, "tolerance", 400000000, NULL);
    g_object_set (aenc, "bitrate", (guint) 128000, NULL);
    g_object_set (aenc, "rate-control", 2, NULL);
#endif
    aqueue = gst_element_factory_make ("queue", "audio-queue");
    if (!aqueue) {
      GST_ERROR_OBJECT (factory, "failed to create audio queue element");
      goto create_error;
    }
#ifdef COMMERCIAL_FEATURE
    gst_bin_add_many (srcbin, audiosrc, acaps, aenc, aparse, aqueue, NULL);

    if (!gst_element_link_many (audiosrc, acaps, aenc, aparse, aqueue, NULL)) {
#else
    gst_bin_add_many (srcbin, audiosrc, acaps, aenc, aqueue, NULL);

    if (!gst_element_link_many (audiosrc, acaps, aenc, aqueue, NULL)) {
#endif
      GST_ERROR_OBJECT (factory, "Failed to link audio src elements...");
      goto create_error;
    }
  } else {
    aqueue = gst_element_factory_make ("queue", "audio-queue");
    if (!aqueue) {
      GST_ERROR_OBJECT (factory, "failed to create audio queue element");
      goto create_error;
    }

    gst_bin_add_many (srcbin, audiosrc, acaps2, audio_convert, acaps, aqueue, NULL);

    if (!gst_element_link_many (audiosrc, acaps2, audio_convert, acaps, aqueue, NULL)) {
      GST_ERROR_OBJECT (factory, "Failed to link audio src elements...");
      goto create_error;
    }
  }

  priv->audio_queue = aqueue;
  if (acodec) g_free (acodec);

  return TRUE;

create_error:
  if (acodec) g_free (acodec);
  return FALSE;
}

static gboolean
_rtsp_media_factory_wfd_create_videotest_bin (GstRTSPMediaFactoryWFD * factory,
    GstBin * srcbin)
{
  GstElement *videosrc = NULL;
  GstElement *vcaps = NULL;
  GstElement *venc_caps = NULL;
  gchar *vcodec = NULL;
  GstElement *venc = NULL;
  GstElement *vparse = NULL;
  GstElement *vqueue = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  priv = factory->priv;

  GST_INFO_OBJECT (factory, "picked videotestsrc as video source");

  videosrc = gst_element_factory_make ("videotestsrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (factory, "failed to create ximagesrc element");
    goto create_error;
  }

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }

  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "I420",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "framerate", GST_TYPE_FRACTION, priv->video_framerate, 1, NULL),
      NULL);

  if (priv->video_codec == GST_WFD_VIDEO_H264)
  {
#ifdef COMMERCIAL_FEATURE
    vcodec = g_strdup("savsenc_h264");//To Do
#else
    vcodec = g_strdup ("x264enc");
#endif
  }
  else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  venc = gst_element_factory_make (vcodec, "videoenc");
  if (vcodec) g_free (vcodec);

  if (!venc) {
    GST_ERROR_OBJECT (factory, "failed to create video encoder element");
    goto create_error;
  }

#ifdef COMMERCIAL_FEATURE
  wfd_hal_config_t* video_encoder_conf;
  guint32 size;
  priv->wfd_hal_mgr.intf.get_video_encoder_config (&video_encoder_conf, &size, priv->content_protection);
  while (size >0 && video_encoder_conf){
    g_object_set (venc, video_encoder_conf->property_name, video_encoder_conf->value, NULL);
    size--;
    video_encoder_conf++;
  }
#else
  g_object_set (venc, "aud", 0, NULL);
  g_object_set (venc, "byte-stream", 1, NULL);
  g_object_set (venc, "bitrate", 512, NULL);
#endif
  venc_caps = gst_element_factory_make ("capsfilter", "venc_caps");
  if (NULL == venc_caps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }

  g_object_set (G_OBJECT (venc_caps), "caps",
      gst_caps_new_simple ("video/x-h264",
          "profile", G_TYPE_STRING, "baseline", NULL), NULL);

  vparse = gst_element_factory_make ("h264parse", "videoparse");
  if (NULL == vparse) {
    GST_ERROR_OBJECT (factory, "failed to create h264 parse element");
    goto create_error;
  }
  g_object_set (vparse, "config-interval", 1, NULL);

  vqueue = gst_element_factory_make ("queue", "video-queue");
  if (!vqueue) {
    GST_ERROR_OBJECT (factory, "failed to create video queue element");
    goto create_error;
  }

  gst_bin_add_many (srcbin, videosrc, vcaps, venc, venc_caps, vparse, vqueue, NULL);
  if (!gst_element_link_many (videosrc, vcaps, venc, venc_caps, vparse, vqueue, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }

  priv->video_queue = vqueue;

  return TRUE;

create_error:
  return FALSE;
}

static gboolean
_rtsp_media_factory_wfd_create_camera_capture_bin (GstRTSPMediaFactoryWFD *
    factory, GstBin * srcbin)
{
  GstElement *videosrc = NULL;
  GstElement *vcaps = NULL;
  GstElement *venc = NULL;
  GstElement *vparse = NULL;
  GstElement *vqueue = NULL;
  gchar *vcodec = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  priv = factory->priv;

  videosrc = gst_element_factory_make ("camerasrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (factory, "failed to create camerasrc element");
    goto create_error;
  }

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }

  GST_INFO_OBJECT (factory, "picked camerasrc as video source");
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "format", G_TYPE_STRING, "SN12",
          "framerate", GST_TYPE_FRACTION, priv->video_framerate, 1, NULL),
      NULL);

  if (priv->video_codec == GST_WFD_VIDEO_H264)
  {
#ifdef COMMERCIAL_FEATURE
    vcodec = g_strdup(priv->wfd_hal_mgr.intf.get_video_encoder_type(priv->content_protection));
#else
    vcodec = g_strdup (priv->video_encoder);
#endif
   }
  else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  venc = gst_element_factory_make (vcodec, "videoenc");
  if (!venc) {
    GST_ERROR_OBJECT (factory, "failed to create video encoder element");
    goto create_error;
  }
  if (vcodec) g_free (vcodec);

#ifdef COMMERCIAL_FEATURE
GST_ERROR_OBJECT (factory, "inside commercial feature");
  wfd_hal_config_t* video_encoder_conf;
  guint32 size;

  priv->wfd_hal_mgr.intf.get_video_encoder_config (&video_encoder_conf, &size,priv->content_protection);
  while (size >0 && video_encoder_conf){
          g_object_set (venc, video_encoder_conf->property_name, video_encoder_conf->value, NULL);
          size--;
          video_encoder_conf++;
        }
#else
  g_object_set (venc, "bitrate", priv->video_bitrate, NULL);
  g_object_set (venc, "byte-stream", 1, NULL);
  g_object_set (venc, "append-dci", 1, NULL);
#endif

  vparse = gst_element_factory_make ("h264parse", "videoparse");
  if (NULL == vparse) {
    GST_ERROR_OBJECT (factory, "failed to create h264 parse element");
    goto create_error;
  }
  g_object_set (vparse, "config-interval", 1, NULL);

  vqueue = gst_element_factory_make ("queue", "video-queue");
  if (!vqueue) {
    GST_ERROR_OBJECT (factory, "failed to create video queue element");
    goto create_error;
  }

  gst_bin_add_many (srcbin, videosrc, vcaps, venc, vparse, vqueue, NULL);

  if (!gst_element_link_many (videosrc, vcaps, venc, vparse, vqueue, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }

  priv->video_queue = vqueue;

  return TRUE;

create_error:
  return FALSE;
}

#ifdef COMMERCIAL_FEATURE
static void _gst_element_set_base_time(GstRTSPMediaFactoryWFDPrivate *priv ,GstElement *gst_src)
{
  GstClock* clock = NULL;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;

  if(priv->audio_src){
    clock = gst_element_get_clock (priv->audio_src);
  }
  if(clock){
    timestamp = gst_clock_get_time (clock);
    if (!gst_element_get_base_time (priv->audio_src)) {
      timestamp = GST_CLOCK_TIME_NONE;
    } else {
      timestamp -= gst_element_get_base_time (priv->audio_src);
      gst_element_set_base_time (gst_src,timestamp);
    }
    gst_object_unref (clock);

  }
}
static gboolean
_rtsp_media_factory_wfd_create_imagecast_bin (GstRTSPMediaFactoryWFD * factory,
    GstBin * srcbin)
{
  GST_INFO_OBJECT (factory, "Enter");
  GstElement *filesrc = NULL;
  GstElement *jpegparser = NULL;
  GstElement *jpegDecCaps = NULL;
  GstElement *jpegDec = NULL;
  GstElement *videoscale = NULL;
  GstElement *vcaps = NULL;
  GstPad *pad = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;
  GstElement *gspscaler = NULL;
  GstElement *vcaps_enc_sprd = NULL;
  GstElement *venc_sprd = NULL;

  priv = factory->priv;
  GST_INFO_OBJECT (factory, "picked Audio default image as video source");

  priv->imagecast_bin = gst_bin_new("Imagecast Bin");
  if(priv->imagecast_bin == NULL) {
    GST_ERROR("failed to create Imagecast bin element");
    goto create_error;
  }

  priv->filesrc = gst_element_factory_make ("multifilesrc", "multi-filesrc");
  if (NULL == priv->filesrc) {
    GST_ERROR_OBJECT (factory, "failed to create filesrc element");
    goto create_error;
  }
  g_object_set (priv->filesrc, "location", priv->file_path, NULL);
  g_object_set (priv->filesrc, "loop", TRUE, NULL);
  g_object_set (priv->filesrc, "framerate", 5, NULL);
  g_object_set (priv->filesrc, "do-timestamp", (gboolean)TRUE, NULL);

  jpegDecCaps = gst_element_factory_make ("capsfilter", "DecCaps");
  if (jpegDecCaps == NULL) {
    GST_ERROR_OBJECT (factory, "failed to create jpegdeccaps element");
    goto create_error;
  }
  g_object_set (G_OBJECT(jpegDecCaps), "caps",
  gst_caps_new_simple ("image/jpeg",
    "framerate", GST_TYPE_FRACTION, 5, 1, NULL), NULL);

  jpegDec = gst_element_factory_make ("jpegdec", "jpegDec");
  if (NULL == jpegDec) {
    GST_ERROR_OBJECT (factory, "failed to create jpegDec element");
    goto create_error;
  }

  videoscale = gst_element_factory_make ("videoscale", "videoscale");
  if (NULL == videoscale) {
    GST_ERROR_OBJECT (factory, "failed to create videoscale element");
    goto create_error;
  }

    /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "I420",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height, NULL),
      NULL);

  vcaps_enc_sprd = gst_element_factory_make ("capsfilter", "videocaps_enc_sprd");
  if (NULL == vcaps_enc_sprd) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }
  g_object_set (G_OBJECT (vcaps_enc_sprd), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "SN12",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "framerate", GST_TYPE_FRACTION, 30, 1, NULL),
      NULL);


  if (priv->video_codec == GST_WFD_VIDEO_H264) {
     venc_sprd = gst_element_factory_make (priv->wfd_hal_mgr.intf.get_video_encoder_type(priv->content_protection), "videoenc_sprd");
  } else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  if (!venc_sprd) {
    GST_ERROR_OBJECT (factory, "failed to create video sprd-encoder element");
    goto create_error;
  }

  wfd_hal_config_t* image_encoder_conf = NULL;
  guint32 size = 0;
  priv->wfd_hal_mgr.intf.get_video_encoder_config (&image_encoder_conf, &size, priv->content_protection);

  while(size > 0 && image_encoder_conf)
  {
    g_object_set (venc_sprd, image_encoder_conf->property_name, image_encoder_conf->value, NULL);
    size--;
    image_encoder_conf++;
  }
  g_object_set (venc_sprd, "bitrate", priv->video_bitrate, NULL);
  gspscaler = gst_element_factory_make (priv->wfd_hal_mgr.intf.get_hardware_color_converter_type(priv->content_protection), "video_gspscaler");
  if (!gspscaler) {
    GST_ERROR_OBJECT (factory, "failed to create video gspscaler");
    goto create_error;
  }

  gst_bin_add_many (priv->imagecast_bin, priv->filesrc, jpegDecCaps, jpegDec, videoscale, vcaps, gspscaler, vcaps_enc_sprd, venc_sprd, NULL);
  if (!gst_element_link_many (priv->filesrc, jpegDecCaps, jpegDec, videoscale, vcaps,  gspscaler, vcaps_enc_sprd, venc_sprd, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link till audio default image src elements...");
    goto create_error;
  }

  pad = gst_element_get_static_pad(venc_sprd, "src");
  if (pad == NULL)  {
    GST_ERROR("Could not get src pads from venc");
    goto create_error;
  }
  if(!gst_element_add_pad(priv->imagecast_bin, gst_ghost_pad_new("src", pad))) {
    GST_ERROR("Could not add ghost pad");
    goto create_error;
  }
  if (pad) {
    gst_object_unref(pad);
    pad = NULL;
  }
  priv->imagecast_pad = gst_element_get_static_pad(priv->imagecast_bin, "src");
  if (priv->imagecast_pad == NULL) {
    GST_ERROR("Could not get src pads");
    goto create_error;
  }
  _gst_element_set_base_time( priv, priv->filesrc );
  GST_INFO_OBJECT (factory, "Exit");
  return TRUE;

create_error:
  GST_INFO_OBJECT (factory, "Error");
  return FALSE;
}
#endif

static gboolean
_rtsp_media_factory_wfd_create_xcapture_bin (GstRTSPMediaFactoryWFD * factory,
    GstBin * srcbin)
{
  GstElement *videosrc = NULL;
  GstElement *vcaps = NULL;
  GstElement *venc_caps = NULL;
  GstElement *videoconvert = NULL, *videoscale = NULL;
  gchar *vcodec = NULL;
  GstElement *venc = NULL;
  GstElement *vparse = NULL;
  GstElement *vqueue = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  priv = factory->priv;

  GST_INFO_OBJECT (factory, "picked ximagesrc as video source");

  videosrc = gst_element_factory_make ("ximagesrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (factory, "failed to create ximagesrc element");
    goto create_error;
  }

  videoscale = gst_element_factory_make ("videoscale", "videoscale");
  if (NULL == videoscale) {
    GST_ERROR_OBJECT (factory, "failed to create videoscale element");
    goto create_error;
  }

  videoconvert = gst_element_factory_make ("videoconvert", "videoconvert");
  if (NULL == videoconvert) {
    GST_ERROR_OBJECT (factory, "failed to create videoconvert element");
    goto create_error;
  }

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }

  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "framerate", GST_TYPE_FRACTION, priv->video_framerate, 1, NULL),
      NULL);

  if (priv->video_codec == GST_WFD_VIDEO_H264)
    vcodec = g_strdup (priv->video_encoder);
  else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  venc = gst_element_factory_make (vcodec, "videoenc");
  if (vcodec) g_free (vcodec);

  if (!venc) {
    GST_ERROR_OBJECT (factory, "failed to create video encoder element");
    goto create_error;
  }

  g_object_set (venc, "aud", 0, NULL);
  g_object_set (venc, "byte-stream", 1, NULL);
  g_object_set (venc, "bitrate", 512, NULL);

  venc_caps = gst_element_factory_make ("capsfilter", "venc_caps");
  if (NULL == venc_caps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }

  g_object_set (G_OBJECT (venc_caps), "caps",
      gst_caps_new_simple ("video/x-h264",
          "profile", G_TYPE_STRING, "baseline", NULL), NULL);

  vparse = gst_element_factory_make ("h264parse", "videoparse");
  if (NULL == vparse) {
    GST_ERROR_OBJECT (factory, "failed to create h264 parse element");
    goto create_error;
  }
  g_object_set (vparse, "config-interval", 1, NULL);

  vqueue = gst_element_factory_make ("queue", "video-queue");
  if (!vqueue) {
    GST_ERROR_OBJECT (factory, "failed to create video queue element");
    goto create_error;
  }

  gst_bin_add_many (srcbin, videosrc, videoscale, videoconvert, vcaps, venc,
      venc_caps, vparse, vqueue, NULL);
  if (!gst_element_link_many (videosrc, videoscale, videoconvert, vcaps, venc,
          venc_caps, vparse, vqueue, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }

  priv->video_queue = vqueue;

  return TRUE;

create_error:
  return FALSE;
}

static gboolean
_rtsp_media_factory_wfd_create_xvcapture_bin (GstRTSPMediaFactoryWFD * factory,
    GstBin * srcbin)
{
#ifndef COMMERCIAL_FEATURE
  GstElement *videosrc = NULL;
  GstElement *vparse = NULL;
  GstElement *vqueue = NULL;
#else
  GstPad *pad = NULL;
  int framerate = 30;
#endif
  GstElement *vcaps = NULL;
  gchar *vcodec = NULL;
  GstElement *venc = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  priv = factory->priv;

  GST_INFO_OBJECT (factory, "picked xvimagesrc as video source");
#ifdef COMMERCIAL_FEATURE
  if(priv->videosrc_bin == NULL){
    GST_ERROR("creating Videosrc bin element");
    priv->videosrc_bin = gst_bin_new("Videosrc Bin");
  } else {
    return TRUE;
  }

  if(priv->videosrc_bin == NULL) {
    GST_ERROR("failed to create Videosrc bin element");
    goto create_error;
  }
  priv->xvimagesrc = gst_element_factory_make ("xvimagesrc", "videosrc");
  if (NULL == priv->xvimagesrc) {
    GST_ERROR_OBJECT (factory, "failed to create xvimagesrc element");
    goto create_error;
  }
  g_object_set(priv->xvimagesrc,"do-timestamp",(gboolean)TRUE,NULL);
#else

  videosrc = gst_element_factory_make ("xvimagesrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (factory, "failed to create xvimagesrc element");
    goto create_error;
  }
#endif

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (factory, "failed to create video capsilfter element");
    goto create_error;
  }
#ifdef COMMERCIAL_FEATURE
  if(priv->video_framerate >=30) framerate = 30;  /* Due to chipset not able to support high fps, restricting it to 30fps */
  else framerate = priv->video_framerate;
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "format", G_TYPE_STRING, "SN12",
          "framerate", GST_TYPE_FRACTION, framerate, 1, NULL),
      NULL);
#else
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw",
          "width", G_TYPE_INT, priv->video_width,
          "height", G_TYPE_INT, priv->video_height,
          "format", G_TYPE_STRING, "SN12",
          "framerate", GST_TYPE_FRACTION, priv->video_framerate, 1, NULL),
      NULL);
#endif
  if (priv->video_codec == GST_WFD_VIDEO_H264) {
#ifdef COMMERCIAL_FEATURE
    vcodec = g_strdup(priv->wfd_hal_mgr.intf.get_video_encoder_type(priv->content_protection));
#else
    vcodec = g_strdup (priv->video_encoder);
#endif
  } else {
    GST_ERROR_OBJECT (factory, "Yet to support other than H264 format");
    goto create_error;
  }

  venc = gst_element_factory_make (vcodec, "videoenc");
  if (!venc) {
    GST_ERROR_OBJECT (factory, "failed to create video encoder element");
    goto create_error;
  }

#ifdef COMMERCIAL_FEATURE
  wfd_hal_config_t* video_encoder_conf = NULL;
  guint32 size = 0;
  priv->wfd_hal_mgr.intf.get_video_encoder_config (&video_encoder_conf, &size, priv->content_protection);
  while(size > 0 && video_encoder_conf)
  {
    g_object_set (venc, video_encoder_conf->property_name, video_encoder_conf->value, NULL);
    size--;
    video_encoder_conf++;
  }
#else
  g_object_set (venc, "byte-stream", 1, NULL);
  g_object_set (venc, "append-dci", 1, NULL);
  g_object_set (venc, "idr-period", 120, NULL);
  g_object_set (venc, "skip-inbuf", priv->video_enc_skip_inbuf_value, NULL);
#endif
  g_object_set (venc, "bitrate", priv->video_bitrate, NULL);

#ifdef COMMERCIAL_FEATURE
  gst_bin_add_many (priv->videosrc_bin, priv->xvimagesrc, vcaps, venc, NULL);
  if (!gst_element_link_many (priv->xvimagesrc, vcaps, venc, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }

  if (vcodec){
    g_free (vcodec);
    vcodec = NULL;
  }
  pad = gst_element_get_static_pad(venc, "src");
  if (pad == NULL)  {
    GST_ERROR("Could not get src pads from venc");
    goto create_error;
  }
  if(!gst_element_add_pad(priv->videosrc_bin, gst_ghost_pad_new("src", pad))) {
    GST_ERROR("Could not add ghost pad");
    goto create_error;
  }
  if (pad) {
    gst_object_unref(pad);
    pad = NULL;
  }
  priv->videosrc_pad = gst_element_get_static_pad(priv->videosrc_bin, "src");
  if (priv->videosrc_pad == NULL) {
    GST_ERROR("Could not get src pads");
    goto create_error;
  }
  _gst_element_set_base_time(priv, priv->xvimagesrc);
#else
  vparse = gst_element_factory_make ("h264parse", "videoparse");
  if (NULL == vparse) {
    GST_ERROR_OBJECT (factory, "failed to create h264 parse element");
    goto create_error;
  }
  g_object_set (vparse, "config-interval", 1, NULL);

  vqueue = gst_element_factory_make ("queue", "video-queue");
  if (!vqueue) {
    GST_ERROR_OBJECT (factory, "failed to create video queue element");
    goto create_error;
  }

  gst_bin_add_many (srcbin, videosrc, vcaps, venc, vparse, vqueue, NULL);
  if (!gst_element_link_many (videosrc, vcaps, venc, vparse, vqueue, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }

  priv->video_queue = vqueue;
  if (vcodec) {
    g_free (vcodec);
    vcodec = NULL;
  }
#endif
  return TRUE;

create_error:
  if (vcodec) g_free (vcodec);
  return FALSE;
}

#ifdef COMMERCIAL_FEATURE
static GstPadProbeReturn
video_event_data_drop(GstPad * pad, GstPadProbeInfo *info, gpointer u_data)
{
  GstRTSPMediaFactoryWFD * factory = (GstRTSPMediaFactoryWFD*)u_data;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;
  priv = factory->priv;
  GST_INFO ("data probe dropping event");
  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_DATA (info)) != GST_EVENT_EOS)
    return GST_PAD_PROBE_OK;
  return GST_PAD_PROBE_DROP;
}

static gboolean
_rtsp_media_factory_wfd_switch_video_capture_bin (GstRTSPMediaFactoryWFD * factory, GstBin * srcbin, gint media_mode)
{
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;
  priv = factory->priv;
  GST_DEBUG("set media mode as %d", media_mode);
  switch(media_mode){
    case 0:
    case 1:
      {
	if(!priv->imagecast_bin) {
          if (!_rtsp_media_factory_wfd_create_imagecast_bin (factory, srcbin)) {
            GST_ERROR_OBJECT (factory, "failed to create imagecast_bin...");
            goto create_error;
          }
        }
        if(priv->imagecast_bin) {
          gst_pad_add_probe (priv->videosrc_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, video_event_data_drop, factory, NULL);
          gst_element_set_state (priv->videosrc_bin, GST_STATE_NULL);
          gst_element_get_state (priv->videosrc_bin, NULL, NULL, GST_CLOCK_TIME_NONE);
          if (!gst_pad_unlink (priv->videosrc_pad, priv->parser_pad))
            GST_ERROR("Could not unlink tee src pad1 and tee qpad1");
          gst_bin_remove(srcbin, priv->videosrc_bin);
          priv->videosrc_bin = NULL;
          GST_DEBUG("removed videosrc from bin but still videosrc_bin exists");
          gst_bin_add (srcbin, priv->imagecast_bin);
          if (gst_pad_link (priv->imagecast_pad, priv->parser_pad) != GST_PAD_LINK_OK) {
            GST_ERROR("Could not link pads");
            goto create_error;
          }
          gst_element_set_state (priv->imagecast_bin, GST_STATE_PLAYING);
          gst_element_get_state (priv->imagecast_bin, NULL, NULL, GST_CLOCK_TIME_NONE);
          _gst_element_set_base_time(priv, priv->filesrc);
        } else {
          GST_ERROR("Imagecast pipeline not yet created");
        }
      }
      break;
    case 2:
    default:
      {
        if(!priv->videosrc_bin) {
          if (!_rtsp_media_factory_wfd_create_xvcapture_bin (factory, srcbin)) {
            GST_ERROR_OBJECT (factory, "failed to create xvcapture bin...");
            goto create_error;
          }
        }

       if(priv->videosrc_bin) {
          gst_pad_add_probe (priv->imagecast_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, video_event_data_drop, factory, NULL);
          gst_element_set_state (priv->imagecast_bin, GST_STATE_NULL);
          gst_element_get_state (priv->imagecast_bin, NULL, NULL, GST_CLOCK_TIME_NONE);

          if (!gst_pad_unlink (priv->imagecast_pad, priv->parser_pad))
            GST_ERROR("Could not unlink tee src pad1 and tee qpad1");
          gst_bin_remove(srcbin, priv->imagecast_bin);
          priv->imagecast_bin = NULL;

          GST_DEBUG("removed imagecast from bin but still imagecast exists");
          gst_bin_add (srcbin, priv->videosrc_bin);
          if (gst_pad_link (priv->videosrc_pad, priv->parser_pad) != GST_PAD_LINK_OK) {
            GST_ERROR("Could not link pads");
            goto create_error;
          }
          gst_element_set_state (priv->videosrc_bin, GST_STATE_PLAYING);
          gst_element_get_state (priv->videosrc_bin, NULL, NULL, GST_CLOCK_TIME_NONE);
          _gst_element_set_base_time(priv, priv->xvimagesrc);
        } else {
          GST_ERROR("videosrc pipeline not yet created");
        }
      }
      break;
  }
  GST_DEBUG("Successfully set requested Media Mode");
  return TRUE;
create_error:
  GST_ERROR("Failed to set requested Media Mode");
  return FALSE;
}

static gboolean
_rtsp_media_factory_wfd_create_video_capture_bin (GstRTSPMediaFactoryWFD * factory,
    GstBin * srcbin)
{
  GstElement *vparse = NULL;
  GstElement *vqueue = NULL;
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;
  priv = factory->priv;

  if (!_rtsp_media_factory_wfd_create_xvcapture_bin (factory, srcbin)) {
    GST_ERROR_OBJECT (factory, "failed to create xvcapture bin...");
    goto create_error;
  }
  if (!_rtsp_media_factory_wfd_create_imagecast_bin(factory, srcbin)) {
    GST_ERROR_OBJECT (factory, "failed to create image cast bin...");
    goto create_error;
  }
  vparse = gst_element_factory_make ("h264parse", "videoparse");
  if (NULL == vparse) {
    GST_ERROR_OBJECT (factory, "failed to create h264 parse element");
    goto create_error;
  }
  g_object_set (vparse, "config-interval", 1, NULL);
  priv->parser_pad = gst_element_get_static_pad(vparse, "sink");
  if (priv->parser_pad == NULL) {
    GST_ERROR("Could not get sink pads");
    goto create_error;
  }
  vqueue = gst_element_factory_make ("queue", "video-queue");
  if (!vqueue) {
    GST_ERROR_OBJECT (factory, "failed to create video queue element");
    goto create_error;
  }
  gst_bin_add_many (srcbin, vparse, vqueue, NULL);
  if (!gst_element_link_many (vparse, vqueue, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link video src elements...");
    goto create_error;
  }
  priv->video_queue = vqueue;
  switch(priv->media_mode){
    case 0:
    case 1:
      {
        gst_bin_add (srcbin, priv->imagecast_bin);
        if (gst_pad_link (priv->imagecast_pad, priv->parser_pad) != GST_PAD_LINK_OK) {
          GST_ERROR("Could not link pads");
          goto create_error;
        }
      }
      break;
    case 2:
    default:
      {
        gst_bin_add (srcbin, priv->videosrc_bin);
        if (gst_pad_link (priv->videosrc_pad, priv->parser_pad) != GST_PAD_LINK_OK) {
          GST_ERROR("Could not link pads");
          goto create_error;
        }
      }
      break;
  }
  return TRUE;
create_error:
  GST_ERROR_OBJECT (factory, "failed to create video src pipelines...");
  return FALSE;
}
#endif

static GstElement *
_rtsp_media_factory_wfd_create_srcbin (GstRTSPMediaFactoryWFD * factory)
{
  GstRTSPMediaFactoryWFDPrivate *priv = NULL;

  GstBin *srcbin = NULL;
  GstElement *mux = NULL;
  GstElement *mux_queue = NULL;
  GstElement *payload = NULL;
  GstPad *srcpad = NULL;
  GstPad *mux_vsinkpad = NULL;
  GstPad *mux_asinkpad = NULL;
#ifdef COMMERCIAL_FEATURE
  GstClock *clock;
#endif

  priv = factory->priv;

  /* create source bin */
  srcbin = GST_BIN (gst_bin_new ("srcbin"));
  if (!srcbin) {
    GST_ERROR_OBJECT (factory, "failed to create source bin...");
    goto create_error;
  }
  /* create video src element */
  switch (priv->videosrc_type) {
    case GST_WFD_VSRC_XIMAGESRC:
      if (!_rtsp_media_factory_wfd_create_xcapture_bin (factory, srcbin)) {
        GST_ERROR_OBJECT (factory, "failed to create xcapture bin...");
        goto create_error;
      }
      break;
    case GST_WFD_VSRC_XVIMAGESRC:
#ifdef COMMERCIAL_FEATURE
      if (!_rtsp_media_factory_wfd_create_video_capture_bin (factory, srcbin)) {
#else
      if (!_rtsp_media_factory_wfd_create_xvcapture_bin (factory, srcbin)) {
#endif
        GST_ERROR_OBJECT (factory, "failed to create xvcapture bin...");
        goto create_error;
      }
      break;
    case GST_WFD_VSRC_CAMERASRC:
      if (!_rtsp_media_factory_wfd_create_camera_capture_bin (factory, srcbin)) {
        GST_ERROR_OBJECT (factory, "failed to create camera capture bin...");
        goto create_error;
      }
      break;
    case GST_WFD_VSRC_VIDEOTESTSRC:
      if (!_rtsp_media_factory_wfd_create_videotest_bin (factory, srcbin)) {
        GST_ERROR_OBJECT (factory, "failed to create videotestsrc bin...");
        goto create_error;
      }
      break;
    default:
      GST_ERROR_OBJECT (factory, "unknow mode selected...");
      goto create_error;
  }

  mux = gst_element_factory_make ("mpegtsmux", "tsmux");
  if (!mux) {
    GST_ERROR_OBJECT (factory, "failed to create muxer element");
    goto create_error;
  }

  g_object_set (mux, "wfd-mode", TRUE, NULL);

  mux_queue = gst_element_factory_make ("queue", "muxer-queue");
  if (!mux_queue) {
    GST_ERROR_OBJECT (factory, "failed to create muxer-queue element");
    goto create_error;
  }

  g_object_set (mux_queue, "max-size-buffers", 20000, NULL);

  payload = gst_element_factory_make ("rtpmp2tpay", "pay0");
  if (!payload) {
    GST_ERROR_OBJECT (factory, "failed to create payload element");
    goto create_error;
  }

  g_object_set (payload, "pt", 33, NULL);
  g_object_set (payload, "mtu", priv->mtu_size, NULL);
  g_object_set (payload, "rtp-flush", (gboolean) TRUE, NULL);

  gst_bin_add_many (srcbin, mux, mux_queue, payload, NULL);

  if (!gst_element_link_many (mux, mux_queue, payload, NULL)) {
    GST_ERROR_OBJECT (factory, "Failed to link muxer & payload...");
    goto create_error;
  }

  /* create audio source elements & add to pipeline */
  if (!_rtsp_media_factory_wfd_create_audio_capture_bin (factory, srcbin))
    goto create_error;

  /* request audio sink pad from muxer, which has elementary pid 0x1100 */
  mux_asinkpad = gst_element_get_request_pad (mux, "sink_4352");
  if (!mux_asinkpad) {
    GST_ERROR_OBJECT (factory, "Failed to get sinkpad from muxer...");
    goto create_error;
  }

  /* request srcpad from audio queue */
  srcpad = gst_element_get_static_pad (priv->audio_queue, "src");
  if (!srcpad) {
    GST_ERROR_OBJECT (factory, "Failed to get srcpad from audio queue...");
    goto create_error;
  }

  /* link audio queue's srcpad & muxer sink pad */
  if (gst_pad_link (srcpad, mux_asinkpad) != GST_PAD_LINK_OK) {
    GST_ERROR_OBJECT (factory,
        "Failed to link audio queue src pad & muxer audio sink pad...");
    goto create_error;
  }
  gst_object_unref (mux_asinkpad);
  gst_object_unref (srcpad);
  /* request video sink pad from muxer, which has elementary pid 0x1011 */
  mux_vsinkpad = gst_element_get_request_pad (mux, "sink_4113");
  if (!mux_vsinkpad) {
    GST_ERROR_OBJECT (factory, "Failed to get sink pad from muxer...");
    goto create_error;
  }

  /* request srcpad from video queue */
  srcpad = gst_element_get_static_pad (priv->video_queue, "src");
  if (!srcpad) {
    GST_ERROR_OBJECT (factory, "Failed to get srcpad from video queue...");
    goto create_error;
  }

  if (gst_pad_link (srcpad, mux_vsinkpad) != GST_PAD_LINK_OK) {
    GST_ERROR_OBJECT (factory,
        "Failed to link video queue src pad & muxer video sink pad...");
    goto create_error;
  }
#ifdef COMMERCIAL_FEATURE
  clock = gst_system_clock_obtain ();
  if (!clock && !GST_IS_CLOCK (clock))
    GST_ERROR_OBJECT (factory, "clock creation failed");
  gst_pipeline_use_clock (GST_PIPELINE (srcbin), clock);
  if (gst_element_get_start_time (GST_PIPELINE (srcbin)) != 0)
    GST_ERROR_OBJECT (factory, "clock stream time doesn't start off at 0");
#endif
  gst_object_unref (mux_vsinkpad);
  gst_object_unref (srcpad);
  srcpad = NULL;

  if (priv->dump_ts)
  {
    GstPad *pad_probe = NULL;
    pad_probe = gst_element_get_static_pad (mux, "src");

    if (NULL == pad_probe) {
      GST_INFO_OBJECT (factory, "pad for probe not created");
    } else {
      GST_INFO_OBJECT (factory, "pad for probe SUCCESSFUL");
    }
    gst_pad_add_probe (pad_probe, GST_PAD_PROBE_TYPE_BUFFER,
        rtsp_media_wfd_dump_data, factory, NULL);
  }

  GST_DEBUG_OBJECT (factory, "successfully created source bin...");
#ifdef COMMERCIAL_FEATURE
  priv->srcbin = srcbin;
#endif
  return GST_ELEMENT_CAST (srcbin);

create_error:
  GST_ERROR_OBJECT (factory, "Failed to create pipeline");
  return NULL;
}

static GstElement *
rtsp_media_factory_wfd_create_element (GstRTSPMediaFactory * factory,
    const GstRTSPUrl * url)
{
  GstRTSPMediaFactoryWFD *_factory = GST_RTSP_MEDIA_FACTORY_WFD_CAST (factory);
  GstElement *element = NULL;

  GST_RTSP_MEDIA_FACTORY_WFD_LOCK (factory);

  element = _rtsp_media_factory_wfd_create_srcbin (_factory);

  GST_RTSP_MEDIA_FACTORY_WFD_UNLOCK (factory);

  return element;
}

static GstRTSPMedia *
rtsp_media_factory_wfd_construct (GstRTSPMediaFactory * factory,
    const GstRTSPUrl * url)
{
  GstRTSPMedia *media;
  GstElement *element, *pipeline;
  GstRTSPMediaFactoryClass *klass;

  klass = GST_RTSP_MEDIA_FACTORY_GET_CLASS (factory);

  if (!klass->create_pipeline)
    goto no_create;

  element = gst_rtsp_media_factory_create_element (factory, url);
  if (element == NULL)
    goto no_element;

  /* create a new empty media */
  media = gst_rtsp_media_new (element);

  gst_rtsp_media_collect_streams (media);

  pipeline = klass->create_pipeline (factory, media);
  if (pipeline == NULL)
    goto no_pipeline;

  return media;

  /* ERRORS */
no_create:
  {
    g_critical ("no create_pipeline function");
    return NULL;
  }
no_element:
  {
    g_critical ("could not create element");
    return NULL;
  }
no_pipeline:
  {
    g_critical ("can't create pipeline");
    g_object_unref (media);
    return NULL;
  }
}
