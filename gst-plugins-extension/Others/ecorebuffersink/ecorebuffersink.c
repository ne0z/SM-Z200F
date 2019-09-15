/*
 * EcoreBufferSink
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jinsoo Kim <js1002.kim@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <tbm_bufmgr.h>
#include <sys/types.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <Eina.h>
#include <Ecore_Buffer.h>
#include <Ecore_Buffer_Queue.h>
#include "ecorebuffersink.h"

/* Debugging category */
#include <gst/gstinfo.h>
GST_DEBUG_CATEGORY_STATIC (gst_debug_ecorebuffersink);
#define GST_CAT_DEFAULT gst_debug_ecorebuffersink
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

enum
{
  PROP_0,
  PROP_VISIBLE,
  PROP_ECOREBUFFER_QUEUE_NAME,
  PROP_ECOREBUFFER_ROTATION_FLAG
};

#if 0
#define FUNCTION_ENTER()  GST_INFO("<ENTER>")
#else
#define FUNCTION_ENTER()
#endif
#define EPIPE_REQUEST_LIMIT                             2
#define COLOR_DEPTH                                     4
#define MIN_ECOREBUFFER_QUEUE_SIZE                      30
#define MAX_ECOREPIPE_BUFFER_CNT                        4
#define DEBUGLOG_DEFAULT_COUNT                          8
#define SIZE_FOR_BUFFERPOOL_INDEX                           sizeof(gint)

#define ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_ADD_CALLBACK( provider, usr_data ) \
do \
{ \
  if (provider) { \
    GST_WARNING_OBJECT (usr_data, "consumer add callback"); \
    ecore_buffer_provider_consumer_add_cb_set (provider, gst_ecorebuffersink_consumer_add, usr_data); \
  } \
} while (0)

#define ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_DEL_CALLBACK( provider, usr_data ) \
do \
{ \
  if (provider) { \
    GST_WARNING_OBJECT (usr_data, "consumer del callback"); \
    ecore_buffer_provider_consumer_del_cb_set (provider, gst_ecorebuffersink_consumer_del, usr_data); \
  } \
} while (0)

#define ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_RELEASE_CALLBACK( provider, usr_data ) \
do \
{ \
  if (provider) { \
    GST_WARNING_OBJECT (usr_data, "consumer release callback"); \
    ecore_buffer_provider_buffer_released_cb_set (provider, gst_ecorebuffersink_consumer_release, usr_data); \
  } \
} while (0)

gint gst_ecorebuffersink_ref_count (GstBuffer * buf);
static GstFlowReturn gst_ecorebuffersink_epipe_reset (GstEcoreBufferSink * esink);
static void gst_ecorebuffersink_navigation_init (GstNavigationInterface * iface);
static void gst_ecorebuffersink_colorbalance_init (GstColorBalanceInterface * iface);

/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_ecorebuffersink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string)SN12, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

#define gst_ecorebuffersink_parent_class parent_class

enum
{
  DEGREE_0,
  DEGREE_90,
  DEGREE_180,
  DEGREE_270,
  DEGREE_NUM,
};

#define GST_TYPE_ECOREBUFFERSINK_ROTATE_ANGLE (gst_ecorebuffersink_rotate_angle_get_type())

static GType
gst_ecorebuffersink_rotate_angle_get_type (void)
{
  static GType ecorebuffersink_rotate_angle_type = 0;
  static const GEnumValue rotate_angle_type[] = {
    {0, "No rotate", "DEGREE_0"},
    {1, "Rotate 90 degree", "DEGREE_90"},
    {2, "Rotate 180 degree", "DEGREE_180"},
    {3, "Rotate 270 degree", "DEGREE_270"},
    {4, NULL, NULL},
  };
    if (!ecorebuffersink_rotate_angle_type) {
    ecorebuffersink_rotate_angle_type =
        g_enum_register_static ("GstEcoreBufferSinkRotateAngleType",
        rotate_angle_type);
  }

  return ecorebuffersink_rotate_angle_type;
}

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

static void
gst_ecorebuffersink_cb_pipe (void * data, int * buffer_index, unsigned int nbyte)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *)data;

  GST_DEBUG_OBJECT (esink, "[ENTER]");

  if (!esink) {
    GST_WARNING_OBJECT (esink, "esink : %p. Just returning", esink);
    return;
  }

  GST_LOG_OBJECT (esink, "esink : %p", esink);

  if (!(esink->is_consumer_connected)) {
    GST_WARNING_OBJECT (esink, "Not connected to consumer.");
    return;
  }

  gint index = 0;
  index = *buffer_index;
  if ((index < 0 || index >= ECOREBUFFER_POOL_NUM) || nbyte != SIZE_FOR_BUFFERPOOL_INDEX) {
    GST_WARNING_OBJECT (esink, "index : %d, nbyte : %d", index, nbyte);
    return;
  }

  if (GST_STATE (esink) < GST_STATE_PAUSED) {
    GST_WARNING_OBJECT (esink, "WRONG-STATE(%d) for rendering, skip this frame", GST_STATE (esink));
    return;
  }

  g_mutex_lock (&(esink->display_buffer_lock));

  if (esink->tbm_surface_format == TBM_FORMAT_NV12) {
    if (!esink->displaying_buffer[index].n_surface) {
      GST_ERROR_OBJECT (esink, "the index's nbuffer was already NULL, so return");
      g_mutex_unlock (&(esink->display_buffer_lock));
      return;
    }
  }

  if (esink->displaying_buffer[index].ecore_buffer == NULL) {
      GST_WARNING_OBJECT (esink, "Creating new instance of ecore_buffer, and rotate flag is %d",esink->rotate_flag);
      esink->displaying_buffer[index].ecore_buffer =
      ecore_buffer_new_with_tbm_surface (NULL, esink->displaying_buffer[index].n_surface, esink->rotate_flag);

      if (esink->displaying_buffer[index].ecore_buffer == NULL) {
            GST_ERROR_OBJECT (esink, "ecore_buffer_new_with_tbm_surface is failed.");
            goto err_ecore_buffer;
    }
  } else if(ecore_buffer_flags_get(esink->displaying_buffer[index].ecore_buffer)!=esink->rotate_flag) {
      ecore_buffer_free (esink->displaying_buffer[index].ecore_buffer);
      esink->displaying_buffer[index].ecore_buffer = NULL;
      GST_WARNING_OBJECT (esink, "Creating new instance of ecore_buffer, and rotate flag is changed to %d",esink->rotate_flag);
      esink->displaying_buffer[index].ecore_buffer =
      ecore_buffer_new_with_tbm_surface (NULL, esink->displaying_buffer[index].n_surface, esink->rotate_flag);

      if (esink->displaying_buffer[index].ecore_buffer == NULL) {
            GST_ERROR_OBJECT (esink, "ecore_buffer_new_with_tbm_surface is failed.");
            goto err_ecore_buffer;
      }
  }

  GST_DEBUG_OBJECT (esink, "Enqueuing ecore_buffer (%p)", esink->displaying_buffer[index].ecore_buffer);

  g_mutex_lock (&(esink->instance_lock));

  if (ecore_buffer_provider_buffer_enqueue (esink->provider, esink->displaying_buffer[index].ecore_buffer) == EINA_FALSE) {
    GST_ERROR_OBJECT (esink, "ecore_buffer_provider_buffer_enqueue is failed.");
    g_mutex_unlock (&(esink->instance_lock));
    goto err_ecore_buffer;
  }

  g_mutex_unlock (&(esink->instance_lock));

  GST_DEBUG_OBJECT (esink, "ecore buffer enqueue finish");

  g_mutex_unlock (&(esink->display_buffer_lock));

  GST_DEBUG_OBJECT (esink, "[LEAVE]");
  return;

err_ecore_buffer:
  if (esink->displaying_buffer[index].ref_count) {
     while (esink->displaying_buffer[index].ref_count) {
       GST_ERROR_OBJECT (esink, "index[%d]'s buffer ref count = %d", index,
         gst_ecorebuffersink_ref_count (esink->displaying_buffer[index].buffer));
       esink->displaying_buffer[index].ref_count--;
       gst_buffer_unref (esink->displaying_buffer[index].buffer);
     }
     esink->sent_buffer_cnt--;
  }
  g_mutex_unlock (&(esink->display_buffer_lock));
  return;

}

static void
gst_ecorebuffersink_consumer_add (Ecore_Buffer_Provider * provider, int queue_size, int w, int h, void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *)data;

  if (queue_size < MIN_ECOREBUFFER_QUEUE_SIZE) {
    GST_ERROR_OBJECT (esink, "queue_size(%d) is too small. At least, queue_size should be %d.", queue_size, MIN_ECOREBUFFER_QUEUE_SIZE);
    esink->is_consumer_connected = FALSE;
  } else {
    GST_WARNING_OBJECT (esink, "Consumer is connected. queue_size is %d", queue_size);
    esink->is_consumer_connected = TRUE;
  }

  return;
}

static void
gst_ecorebuffersink_consumer_del (Ecore_Buffer_Provider * provider, void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *)data;

  GST_WARNING_OBJECT (esink, "Consumer is disconnected.");
  esink->is_consumer_connected = FALSE;

  return;
}

static void
gst_ecorebuffersink_consumer_release (Ecore_Buffer_Provider * provider, void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *)data;
  Ecore_Buffer *buffer = NULL;
  Ecore_Buffer_Return ret;

  GST_DEBUG_OBJECT (esink, "[ENTER]");

  g_mutex_lock (&(esink->display_buffer_lock));
  g_mutex_lock (&(esink->instance_lock));

  ret = ecore_buffer_provider_buffer_acquire (provider, &buffer);

  g_mutex_unlock (&(esink->instance_lock));

  if (ret == ECORE_BUFFER_RETURN_SUCCESS) {
    guint8 index = 0;

    while (index < ECOREBUFFER_POOL_NUM) {
      if (esink->displaying_buffer[index].ecore_buffer == buffer) {
        GST_DEBUG_OBJECT (esink, "index %d, ecore buffer instance %p", index, buffer);
        break;
      }

      index++;
    }

    if (index == ECOREBUFFER_POOL_NUM) {
      GST_ERROR_OBJECT (esink, "Ecore buffer doesn't match with any displaying buffers.");
      g_mutex_unlock (&(esink->display_buffer_lock));
      return;
    }

    if (esink->displaying_buffer[index].buffer) {
      if (esink->displaying_buffer[index].ref_count) {
        GST_LOG_OBJECT (esink, "[reset] unreffing gst %p", esink->displaying_buffer[index].buffer);

        while (esink->displaying_buffer[index].ref_count) {
          GST_LOG_OBJECT (esink, "index[%d]'s buffer ref count = %d", index,
            gst_ecorebuffersink_ref_count (esink->displaying_buffer[index].buffer));
          esink->displaying_buffer[index].ref_count--;
          gst_buffer_unref (esink->displaying_buffer[index].buffer);
        }
      }
      /* Print debug log for 8 frame */
      if (esink->debuglog_cnt_ecoreCbPipe > 0) {
        GST_WARNING_OBJECT (esink, "(%d) ecore_cb_pipe unref index[%d].. gst_buf %p", DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_ecoreCbPipe), index, esink->displaying_buffer[index].buffer);
        esink->debuglog_cnt_ecoreCbPipe--;
      }

      if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT) {
        GST_WARNING_OBJECT (esink, "sent_buffer cnt 4->3. so skip will be stop");
      }
      esink->sent_buffer_cnt--;

      esink->displaying_buffer[index].buffer = NULL;
    }
  } else {
    GST_WARNING_OBJECT (esink, "Buffer acquire failed.");
  }
  g_mutex_unlock (&(esink->display_buffer_lock));
  return;
}

gint
gst_ecorebuffersink_ref_count (GstBuffer * buf)
{
  FUNCTION_ENTER ();

  return GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER_CAST(buf));
}

static int
gst_ecorebuffersink_get_size_from_caps (GstCaps * caps, int * w, int * h)
{
  gboolean r;
  int width, height;
  GstStructure *s;

  if (!caps || !w || !h) {
    return -1;
  }

  s = gst_caps_get_structure (caps, 0);
  if (!s) {
    return -1;
  }

  r = gst_structure_get_int (s, "width", &width);
  if (r == FALSE) {
    GST_DEBUG ("fail to get width from caps");
    return -1;
  }

  r = gst_structure_get_int (s, "height", &height);
  if (r == FALSE) {
    GST_DEBUG ("fail to get height from caps");
    return -1;
  }

  *w = width;
  *h = height;
  GST_DEBUG ("size w(%d), h(%d)", width, height);

  return 0;
}


static void
gst_ecorebuffersink_fini (gpointer data, GObject * obj)
{
  FUNCTION_ENTER ();

  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (obj);
  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return;
  }

  g_mutex_clear(&(esink->display_buffer_lock));
  g_mutex_clear(&(esink->flow_lock));

  esink->epipe = NULL;
  esink->is_consumer_connected = FALSE;

  g_mutex_lock (&(esink->instance_lock));
  esink->instance_lock_count--;
  g_mutex_unlock (&(esink->instance_lock));
  if (esink->instance_lock_count == 0) {
    g_mutex_clear (&(esink->instance_lock));
  }

  GST_WARNING_OBJECT (esink, "[LEAVE]");
}

static void
gst_ecorebuffersink_reset (GstEcoreBufferSink * esink)
{
  FUNCTION_ENTER ();

  guint8 index = 0;

  g_mutex_lock (&(esink->display_buffer_lock));

  for (index = 0; index < ECOREBUFFER_POOL_NUM; index++) {
    if (esink->displaying_buffer[index].ecore_buffer) {
      ecore_buffer_free (esink->displaying_buffer[index].ecore_buffer);
      esink->displaying_buffer[index].ecore_buffer = NULL;
    }

    if (esink->displaying_buffer[index].n_surface) {
      tbm_surface_destroy (esink->displaying_buffer[index].n_surface);
      esink->displaying_buffer[index].n_surface = NULL;
    }

    if (esink->displaying_buffer[index].buffer) {
      if (esink->displaying_buffer[index].ref_count) {
        GST_WARNING_OBJECT (esink, "[reset] unreffing gst %p", esink->displaying_buffer[index].buffer);

        while (esink->displaying_buffer[index].ref_count) {
          GST_WARNING_OBJECT (esink, "index[%d]'s buffer ref count = %d", index,
            gst_ecorebuffersink_ref_count (esink->displaying_buffer[index].buffer));
          esink->displaying_buffer[index].ref_count--;
          gst_buffer_unref (esink->displaying_buffer[index].buffer);
        }
      }
      esink->displaying_buffer[index].buffer = NULL;
    }

    if (esink->displaying_buffer[index].bo)
      esink->displaying_buffer[index].bo = NULL;
  }

  esink->sent_buffer_cnt = 0;

  g_mutex_unlock (&(esink->display_buffer_lock));
}


static GstFlowReturn
gst_ecorebuffersink_epipe_reset (GstEcoreBufferSink * esink)
{
  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return GST_FLOW_ERROR;
  }

  if (esink->epipe) {
    GST_DEBUG_OBJECT (esink, "ecore-pipe will delete");
    ecore_pipe_del (esink->epipe);
    esink->epipe = NULL;
  }

  if (!esink->epipe) {
    esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)gst_ecorebuffersink_cb_pipe, esink);
    if (!esink->epipe) {
      GST_ERROR_OBJECT (esink, "ecore-pipe create failed");
      return GST_FLOW_ERROR;
    }
    GST_DEBUG_OBJECT (esink, "ecore-pipe create success");
  }

  return GST_FLOW_OK;
}

static void *
gst_ecorebuffersink_ecore_init_all (void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *) data;

  if (!ecore_buffer_init ())
    goto err_buffer;

  esink->buffer_init = TRUE;

  if (!ecore_buffer_queue_init ())
    goto err_ebq;

  esink->queue_init = TRUE;

  GST_WARNING_OBJECT (data, "ecore_init_Done!!!");
  return data;

err_ebq:
  GST_ERROR_OBJECT (data, "ecore_buffer_queue_init error");
  ecore_buffer_shutdown ();
  esink->buffer_init = FALSE;

err_buffer:
  GST_ERROR_OBJECT (data, "ecore_init_Failed!!!");
  return NULL;
}

static void *
gst_ecorebuffersink_ecore_shutdown_all (void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *) data;

  if(esink->queue_init) {
    ecore_buffer_queue_shutdown ();
    esink->queue_init = FALSE;
  }  else {
    GST_WARNING_OBJECT (data, "queue_init is false, we don't need to call queue shutdown");
  }

  if(esink->buffer_init) {
    ecore_buffer_shutdown ();
    esink->buffer_init = FALSE;
  }  else {
    GST_WARNING_OBJECT (data, "buffer_init is false, we don't need to call queue shutdown");
  }

  GST_WARNING_OBJECT (data, "ecore_shutdown_Done!!!");

  return data;
}

static void *
gst_ecorebuffersink_main_loop_provider_new (void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *) data;
  Ecore_Buffer_Provider *provider = NULL;

  GST_WARNING_OBJECT (esink, "Creating new provider instance.");

  provider = ecore_buffer_provider_new (esink->ecorebuffer_queue_name);

  if (provider == NULL) {
    GST_ERROR_OBJECT (esink, "Provider instance creation failed.");
    return NULL;
  } else {
    ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_ADD_CALLBACK (provider, esink);
    ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_DEL_CALLBACK (provider, esink);
    ECOREBUFFERSINK_PROVIDER_SET_CONSUMER_RELEASE_CALLBACK (provider, esink);

    GST_LOG_OBJECT (esink, "Provider instance %p", provider);

    return provider;
  }
}

static void *
gst_ecorebuffersink_main_loop_provider_free (void * data)
{
  GstEcoreBufferSink *esink = (GstEcoreBufferSink *) data;

  ecore_buffer_provider_free (esink->provider);

  GST_WARNING_OBJECT (data, "ecore buffer provider freed");

  return data;
}

static gboolean
gst_ecorebuffersink_provider_new (GstEcoreBufferSink * esink)
{
  GST_WARNING_OBJECT (esink, "Enter!!!");

  if (esink->ecorebuffer_queue_name == NULL) {
    GST_WARNING_OBJECT(esink, "queue_name is NULL!!!");
    goto err;
  }

  if (ecore_main_loop_thread_safe_call_sync (gst_ecorebuffersink_ecore_init_all, esink) == NULL) {
    GST_ERROR_OBJECT(esink, "gst_ecorebuffersink_ecore_init_all is failed!");
    goto err;
  }

  if (!(esink->provider = ecore_main_loop_thread_safe_call_sync (gst_ecorebuffersink_main_loop_provider_new, esink))) {
    GST_ERROR_OBJECT(esink, "ecore_buffer_provider_new is failed!");
    goto err_provider;
  }

  GST_WARNING_OBJECT (esink, "Leave!!!");
  return TRUE;

err_provider:
  ecore_main_loop_thread_safe_call_sync (gst_ecorebuffersink_ecore_shutdown_all, esink);
  GST_ERROR_OBJECT(esink, "ecore shutdown all because of ecore init all fail!");

err:
  GST_ERROR_OBJECT(esink, "ecore_buffer_provider_new is failed!");
  return FALSE;
}

static void
gst_ecorebuffersink_ecore_provider_shutdown (GstEcoreBufferSink * esink)
{
  g_mutex_lock (&(esink->instance_lock));
  if (esink->provider) {
    ecore_main_loop_thread_safe_call_sync (gst_ecorebuffersink_main_loop_provider_free, esink);
    esink->provider = NULL;
  }
  g_mutex_unlock (&(esink->instance_lock));
  GST_WARNING_OBJECT (esink, "provider_shutdown_Done!!!");
}

G_DEFINE_TYPE_EXTENDED (GstEcoreBufferSink, gst_ecorebuffersink,
    GST_TYPE_VIDEO_SINK, 0, G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_ecorebuffersink_navigation_init)
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_ecorebuffersink_colorbalance_init));

/* Element stuff */
static GstCaps *
gst_ecorebuffersink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  FUNCTION_ENTER ();

  GST_WARNING ("Not implemented!!!");

  return NULL;
}

static gboolean
gst_ecorebuffersink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  FUNCTION_ENTER ();

  int r;
  int w, h;
  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (bsink);
  GstStructure *structure = NULL;
  guint32 format = 0;
  int ret = 0;
  const gchar *fmt;
  GstVideoInfo info;

  GST_DEBUG_OBJECT (esink, "In setcaps. setting caps %"GST_PTR_FORMAT, caps);

  r = gst_ecorebuffersink_get_size_from_caps (caps, &w, &h);
  if (!r) {
    g_mutex_lock (&(esink->instance_lock));
    if ((w != 0 && h != 0) && (esink->w != w || esink->h != h)) {
      if (GST_STATE (esink) >= GST_STATE_PAUSED) {
        GST_WARNING_OBJECT (esink, "new caps (%d,%d), esink (%d,%d)", w, h, esink->w, esink->h);

        ret = gst_ecorebuffersink_epipe_reset (esink);
        if (ret != GST_FLOW_OK) {
          GST_ERROR_OBJECT (esink, "epipe reset ret=%d, need to check", ret);
          g_mutex_unlock (&(esink->instance_lock));
          return FALSE;
        }
        gst_ecorebuffersink_reset (esink);
      }
    } else {
      GST_WARNING_OBJECT (esink, "Invalid resolution (%d,%d), old dimensions (%d,%d)", w, h, esink->w, esink->h);

      if (w == 0 || h == 0) {
        GST_ERROR_OBJECT (esink, "Width and height are not proper [%dx%d]", w, h);
        g_mutex_unlock (&(esink->instance_lock));
        return FALSE;
      }
    }

    esink->w = w;
    esink->h = h;

    g_mutex_unlock (&(esink->instance_lock));
    GST_DEBUG_OBJECT (esink, "set size w(%d), h(%d)", w, h);
  }

  structure = gst_caps_get_structure (caps, 0);
  if (!structure) {
    GST_ERROR_OBJECT (esink, "caps structure is invalid.");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_ERROR_OBJECT (esink, "caps setting failed.");
    return FALSE;
  }

  esink->fps_n = info.fps_n;
  esink->fps_d = info.fps_d;

  if ((fmt = gst_structure_get_string (structure, "format"))) {
    format = gst_video_format_from_string (fmt);
  }

  GST_WARNING_OBJECT (esink, "source color format is %d", format);

  if (format == GST_VIDEO_FORMAT_SN12 || format == GST_VIDEO_FORMAT_NV12) {
    esink->tbm_surface_format = TBM_FORMAT_NV12;
  } else if (format == GST_VIDEO_FORMAT_I420) {
    esink->tbm_surface_format = TBM_FORMAT_YUV420;
  } else {
    GST_ERROR_OBJECT (esink, "cannot parse fourcc format from caps.");
    return FALSE;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_ecorebuffersink_change_state (GstElement * element,
    GstStateChange transition)
{
  FUNCTION_ENTER ();

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstEcoreBufferSink *esink;
  GstFlowReturn flowRet = GST_FLOW_OK;

  esink = GST_ECOREBUFFERSINK (element);

  if (!esink) {
    GST_ERROR_OBJECT (esink, "can not get esink from element");
    return GST_STATE_CHANGE_FAILURE;
  }

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_NULL_TO_READY");

      if (esink->provider == NULL) {
        g_mutex_lock (&(esink->instance_lock));
        if (!(gst_ecorebuffersink_provider_new (esink))) {
          GST_WARNING_OBJECT (esink, "gst_ecorebuffersink_provider_new is failed. It will be tried again.");
        }
        g_mutex_unlock (&(esink->instance_lock));
      }
      break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_READY_TO_PAUSED");
      break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");

      if (esink->provider == NULL) {
        GST_WARNING_OBJECT (esink, "Provider instance is NULL. It will be tried again.");
      }

      /* Print debug log for 8 frame */
      esink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
      esink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;
      break;

    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
      break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_PAUSED_TO_READY");

      flowRet = gst_ecorebuffersink_epipe_reset (esink);
      if (flowRet != GST_FLOW_OK) {
        GST_ERROR_OBJECT (esink, "epipe reset ret=%d, need to check", flowRet);
        return GST_STATE_CHANGE_FAILURE;
      }
      gst_ecorebuffersink_reset (esink);

      esink->fps_n = 0;
      esink->fps_d = 1;
      break;

    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_WARNING_OBJECT (esink, "GST_STATE_CHANGE_READY_TO_NULL");

      gst_ecorebuffersink_ecore_provider_shutdown (esink);

      g_mutex_lock (&(esink->instance_lock));
      g_free (esink->ecorebuffer_queue_name);
      esink->ecorebuffer_queue_name= NULL;
      g_mutex_unlock (&(esink->instance_lock));
      ecore_main_loop_thread_safe_call_sync (gst_ecorebuffersink_ecore_shutdown_all, esink);

      if (esink->epipe) {
        GST_WARNING_OBJECT (esink, "ecore-pipe will delete");
        ecore_pipe_del (esink->epipe);
        esink->epipe = NULL;
      }
      break;

    default:
      break;
  }
  return ret;
}

static void
gst_ecorebuffersink_get_times (GstBaseSink * bsink, GstBuffer * buf, GstClockTime * start, GstClockTime * end)
{
  FUNCTION_ENTER ();

  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (esink->fps_n > 0) {
        *end = *start + gst_util_uint64_scale_int (GST_SECOND, esink->fps_d, esink->fps_n);
      }
    }
  }
}

static GstFlowReturn
gst_ecorebuffersink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  FUNCTION_ENTER ();
  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (vsink);
  Eina_Bool r;
  GstMemory *mem;

  GST_LOG_OBJECT (esink, "[ENTER] show frame");

  if (!gst_ecorebuffersink_ref_count (buf)) {
    GST_WARNING_OBJECT (esink, "ref count is 0.. skip show frame");
    return GST_FLOW_OK;
  }

  if (!esink->visible) {
    GST_WARNING_OBJECT (esink, "visible[%d]. Skipping show frame", esink->visible);
    return GST_FLOW_OK;
  }

  g_mutex_lock (&(esink->display_buffer_lock));
  g_mutex_lock (&(esink->instance_lock));
  if (esink->provider == NULL) {
    GST_WARNING_OBJECT (esink, "Provider instance should be valid by now. Initializing provider again");
    if (!(gst_ecorebuffersink_provider_new (esink))) {
      GST_ERROR_OBJECT (esink, "gst_ecorebuffersink_provider_new is failed!!!");
      g_mutex_unlock (&(esink->instance_lock));
      g_mutex_unlock (&(esink->display_buffer_lock));
      return GST_FLOW_ERROR;
    }
  }

  if (!esink->epipe) {
    esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)gst_ecorebuffersink_cb_pipe, esink);
    if (!esink->epipe) {
      GST_ERROR_OBJECT (esink, "ecore-pipe create failed");
      g_mutex_unlock (&(esink->instance_lock));
      g_mutex_unlock (&(esink->display_buffer_lock));
      return GST_FLOW_ERROR;
    }
    GST_DEBUG_OBJECT (esink, "ecore-pipe create success");
  }

  if (!esink->is_consumer_connected) {
    GST_WARNING_OBJECT (esink, "Consumer is not connected.");
    g_mutex_unlock (&(esink->instance_lock));
    g_mutex_unlock (&(esink->display_buffer_lock));
    return GST_FLOW_OK;
  }

  int i = 0;
  int index = -1;
  MMVideoBuffer *mm_video_buf = NULL;
  gboolean exist_bo = FALSE;
  int bo_num = 0;
  GstMapInfo buf_info = GST_MAP_INFO_INIT;

  if (esink->tbm_surface_format == TBM_FORMAT_NV12) {
    /* get received buffer informations */
    mem = gst_buffer_peek_memory (buf, 1);
    gst_memory_map (mem, &buf_info, GST_MAP_READ);
    mm_video_buf = (MMVideoBuffer *) buf_info.data;
    gst_memory_unmap (mem, &buf_info);

    if (!mm_video_buf) {
      GST_WARNING_OBJECT (esink, "mm_video_buf is NULL. Skip..." );
      g_mutex_unlock (&(esink->instance_lock));
      g_mutex_unlock (&(esink->display_buffer_lock));
      return GST_FLOW_OK;
    }

    if (mm_video_buf->flush_request) {
      GstFlowReturn ret = GST_FLOW_OK;
      GST_WARNING_OBJECT (esink, "Flush Request. Releasing all other displaying buffers");

      g_mutex_unlock (&(esink->instance_lock));
      g_mutex_unlock (&(esink->display_buffer_lock));

      ret = gst_ecorebuffersink_epipe_reset (esink);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (esink, "epipe reset ret = %d, need to check", ret);
        return ret;
      }

      gst_ecorebuffersink_reset (esink);

      GST_DEBUG_OBJECT (esink, "Flushed");
      return GST_FLOW_OK;
    } else {
      if (mm_video_buf->type == MM_VIDEO_BUFFER_TYPE_TBM_BO) {
        /* check whether bo is new or not */
        for (i = 0; i < ECOREBUFFER_POOL_NUM; i++) {
          if (esink->displaying_buffer[i].bo == mm_video_buf->handle.bo[0]) {
            index = i;
            exist_bo = TRUE;
            break;
          } else {
            exist_bo = FALSE;
          }
        }
        /* keep bo */
        if (!exist_bo) {
          /* find empty buffer space for indexing */
          for (i = 0; i < ECOREBUFFER_POOL_NUM; i++) {
            if (!esink->displaying_buffer[i].n_surface) {
              index = i;
              break;
            }
          }

          if (index != -1) {
            tbm_surface_info_s tsurf_info = { 0 };
            /* keep informations */
            esink->displaying_buffer[index].buffer = buf;
            esink->displaying_buffer[index].bo = mm_video_buf->handle.bo[0];
            GST_WARNING_OBJECT (esink, "TBM gst_buf %p, bo %p", esink->displaying_buffer[index].buffer, esink->displaying_buffer[index].bo);

            /* create new tbm_surface buffer */
            for (i = 0, bo_num = 0; i < MM_VIDEO_BUFFER_PLANE_MAX; i++) {
              if (mm_video_buf->handle.bo[i]) {
                bo_num++;
              }
            }

            tsurf_info.width = mm_video_buf->width[0];
            tsurf_info.height = mm_video_buf->height[0];
            tsurf_info.format = TBM_FORMAT_NV12;
            tsurf_info.bpp = tbm_surface_internal_get_bpp (tsurf_info.format);
            tsurf_info.num_planes = tbm_surface_internal_get_num_planes (tsurf_info.format);

            tsurf_info.planes[0].size = mm_video_buf->stride_width[0] * mm_video_buf->stride_height[0];
            tsurf_info.planes[1].size = mm_video_buf->stride_width[0] * mm_video_buf->stride_height[1];

            tsurf_info.planes[0].offset = 0;
            tsurf_info.planes[0].stride = mm_video_buf->stride_width[0];
            tsurf_info.planes[1].stride = mm_video_buf->stride_width[1];

            if (bo_num == 1)
              tsurf_info.planes[1].offset = tsurf_info.planes[0].size;

            tsurf_info.size = tsurf_info.planes[0].size + tsurf_info.planes[1].size;

            esink->displaying_buffer[index].n_surface = tbm_surface_internal_create_with_bos (&tsurf_info,
              (tbm_bo *)mm_video_buf->handle.bo, bo_num);

            if (!esink->displaying_buffer[index].n_surface) {
              GST_WARNING_OBJECT (esink, "there is no tbm_surface buffer.. bo : %p,  gst_buf : %p", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer);
              g_mutex_unlock (&(esink->instance_lock));
              g_mutex_unlock (&(esink->display_buffer_lock));
              return GST_FLOW_OK;
            }

            GST_WARNING_OBJECT (esink, "create tbm_surface n_surface : %p", esink->displaying_buffer[index].n_surface);
          }
        } else {
          if (index != -1) {
            esink->displaying_buffer[index].buffer = buf;

            GST_DEBUG_OBJECT (esink, "existing tbm_surface n_surface %p,  gst_buf %p", esink->displaying_buffer[index].n_surface, esink->displaying_buffer[index].buffer);
            exist_bo = FALSE;
          }
        }

        /* if it couldn't find proper index */
        if (index == -1)
          GST_WARNING_OBJECT (esink, "all spaces are using!!!");
        else
          GST_DEBUG_OBJECT (esink, "selected buffer index %d", index);
      } else {
        GST_ERROR_OBJECT (esink, "it is not TBM buffer.. %d", mm_video_buf->type);
      }
    }
  } else {
    GST_ERROR_OBJECT (esink, "unsupported color format");
    g_mutex_unlock (&(esink->instance_lock));
    g_mutex_unlock (&(esink->display_buffer_lock));
    return GST_FLOW_ERROR;
  }

  if (index != -1) {
    static int skip_count = 0;

    if (esink->sent_buffer_cnt < MAX_ECOREPIPE_BUFFER_CNT) {
      GST_LOG_OBJECT (esink, "[show_frame] before refcount : %d .. gst_buf : %p", gst_ecorebuffersink_ref_count (buf), buf);
      gst_buffer_ref (buf);
      esink->displaying_buffer[index].ref_count++;
      GST_LOG_OBJECT (esink, "index %d set refcount increase as %d", index,esink->displaying_buffer[index].ref_count);
      GST_LOG_OBJECT (esink, "[show_frame] after refcount : %d .. gst_buf : %p", gst_ecorebuffersink_ref_count (buf), buf);

      /* Print debug log for 8 frame */
      if (esink->debuglog_cnt_showFrame > 0) {
        GST_WARNING_OBJECT (esink, "(%d) ecore_pipe_write index[%d]  gst_buf : %p", DEBUGLOG_DEFAULT_COUNT - (esink->debuglog_cnt_showFrame),
          index, esink->displaying_buffer[index].buffer);
        esink->debuglog_cnt_showFrame--;
      }

      esink->sent_buffer_cnt++;
      skip_count = 0;

      r = ecore_pipe_write (esink->epipe, &index , SIZE_FOR_BUFFERPOOL_INDEX);

      if (r == EINA_FALSE)  {
        GST_LOG_OBJECT (esink, "[show_frame] before refcount : %d .. gst_buf : %p", gst_ecorebuffersink_ref_count (buf), buf);
        if (esink->displaying_buffer[index].ref_count) {
          esink->displaying_buffer[index].ref_count--;
          esink->sent_buffer_cnt--;
          gst_buffer_unref (buf);
          GST_ERROR_OBJECT (esink, "finish unreffing");
        }
      }
    } else {
      /* If buffer count which is sent to ecore pipe, is upper 3, Print Error log */
      if (!(skip_count++ % 20)) {
        GST_WARNING_OBJECT (esink, "[%d]EA buffer was already sent to ecore pipe, and %d frame skipped", esink->sent_buffer_cnt,skip_count);
      }
    }
  } else {
    GST_WARNING_OBJECT (esink, "skip ecore_pipe_write(). because of index(%d)", index);
  }
  g_mutex_unlock (&(esink->instance_lock));
  g_mutex_unlock (&(esink->display_buffer_lock));
  GST_DEBUG_OBJECT (esink, "Leave");
  return GST_FLOW_OK;
}

static gboolean
gst_ecorebuffersink_event (GstBaseSink * sink, GstEvent * event)
{
  FUNCTION_ENTER ();

  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (sink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (esink, "GST_EVENT_FLUSH_START");
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (esink, "GST_EVENT_FLUSH_STOP");
      break;
    case GST_EVENT_EOS:
      GST_DEBUG_OBJECT (esink, "GST_EVENT_EOS");
      break;
    default:
      GST_DEBUG_OBJECT (esink, "%s event is received.", GST_EVENT_TYPE_NAME (event));
      break;
  }

  return GST_BASE_SINK_CLASS (parent_class)->event (sink, event);
}

/* Interfaces stuff */
static void
gst_ecorebuffersink_navigation_init (GstNavigationInterface * iface)
{
  FUNCTION_ENTER ();

}

static void
gst_ecorebuffersink_colorbalance_init (GstColorBalanceInterface * iface)
{
  FUNCTION_ENTER ();

}

static void
gst_ecorebuffersink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (object);
  int rotate = 0;
  g_mutex_lock (&(esink->instance_lock));

  GST_LOG_OBJECT (esink, "property index is %d", prop_id);

  switch (prop_id) {
    case PROP_VISIBLE:
      esink->visible = g_value_get_boolean (value);
      GST_WARNING_OBJECT (esink, "Visibility set to %d", esink->visible);
      break;

    case PROP_ECOREBUFFER_QUEUE_NAME:
      g_free (esink->ecorebuffer_queue_name);
      esink->ecorebuffer_queue_name = g_strdup (g_value_get_string (value));
      GST_WARNING_OBJECT (esink, "Queue name is %s", esink->ecorebuffer_queue_name);
      break;

    case PROP_ECOREBUFFER_ROTATION_FLAG:
      rotate= g_value_get_enum (value);
      switch (rotate) {
        case DEGREE_0:
          esink->rotate_flag = 0;
          break;
        case DEGREE_90:
          esink->rotate_flag = 90;
          break;
        case DEGREE_180:
          esink->rotate_flag = 180;
          break;
        case DEGREE_270:
          esink->rotate_flag = 270;
          break;
        default:
          break;
      }
      GST_WARNING_OBJECT (esink, "Rotation flag is %d", esink->rotate_flag);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&(esink->instance_lock));
}

static void
gst_ecorebuffersink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstEcoreBufferSink *esink = GST_ECOREBUFFERSINK (object);
  int rotate = 0;

  switch (prop_id) {
    case PROP_VISIBLE:
      g_value_set_boolean (value, esink->visible);
      break;
    case PROP_ECOREBUFFER_QUEUE_NAME:
      g_value_set_string (value, esink->ecorebuffer_queue_name);
      break;
    case PROP_ECOREBUFFER_ROTATION_FLAG:
      switch (esink->rotate_flag) {
      case 0:
            rotate = DEGREE_0;
            break;
      case 90:
            rotate = DEGREE_90;
            break;
      case 180:
            rotate = DEGREE_180;
            break;
      case 270:
            rotate = DEGREE_270;
            break;
      default:
            break;
      }
      g_value_set_enum (value, rotate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Finalize is called only once, dispose can be called multiple times.
 * We use mutexes and don't reset stuff to NULL here so let's register
 * as a finalize. */
static void
gst_ecorebuffersink_finalize (GObject * object)
{
  FUNCTION_ENTER ();

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ecorebuffersink_init (GstEcoreBufferSink * esink)
{
  FUNCTION_ENTER ();
  gint i = 0;

  esink->epipe = NULL;

  g_mutex_init (&(esink->display_buffer_lock));
  g_mutex_init (&(esink->flow_lock));
  if (esink->instance_lock_count == 0)
    g_mutex_init(&(esink->instance_lock));

  g_mutex_lock (&(esink->instance_lock));
  esink->visible = TRUE;
  esink->buffer_init = FALSE;
  esink->queue_init = FALSE;
  esink->ecorebuffer_queue_name = NULL;
  esink->rotate_flag=0;
  g_mutex_unlock (&(esink->instance_lock));
  esink->provider = NULL;

  for (i = 0; i < ECOREBUFFER_POOL_NUM; i++) {
    esink->displaying_buffer[i].n_surface = NULL;
    esink->displaying_buffer[i].ecore_buffer = NULL;
    esink->displaying_buffer[i].buffer = NULL;
    esink->displaying_buffer[i].bo = NULL;
    esink->displaying_buffer[i].ref_count = 0;
  }

  esink->sent_buffer_cnt = 0;
  esink->w = esink->h = 0;
  esink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
  esink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;

  esink->fps_n = 0;
  esink->fps_d = 0;

  esink->is_consumer_connected = FALSE;

  g_mutex_lock (&(esink->instance_lock));
  esink->instance_lock_count++;
  g_mutex_unlock (&(esink->instance_lock));

  g_object_weak_ref (G_OBJECT (esink), gst_ecorebuffersink_fini, NULL);
}

static void
gst_ecorebuffersink_class_init (GstEcoreBufferSinkClass * klass)
{
  FUNCTION_ENTER ();

  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_ecorebuffersink_set_property;
  gobject_class->get_property = gst_ecorebuffersink_get_property;

  g_object_class_install_property (gobject_class, PROP_VISIBLE,
      g_param_spec_boolean ("visible", "sets visible property",
          "Draws screen or no frame update. visible when true",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class, PROP_ECOREBUFFER_QUEUE_NAME,
    g_param_spec_string ("ecorebuffer-queue-name", "Name of ecore buffer queue",
        "Ecore buffer queue Name", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ECOREBUFFER_ROTATION_FLAG,
    g_param_spec_enum ("rotate-flag", "Rotation of ecore buffer",
        "Ecore buffer rotate flag",  GST_TYPE_ECOREBUFFERSINK_ROTATE_ANGLE, DEGREE_0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_ecorebuffersink_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ecorebuffersink_change_state);
  gst_element_class_set_details_simple (gstelement_class, "EcorebufferSink",
      "Sink/Video", "ecore buffer videosink",
      "Samsung");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_ecorebuffersink_sink_template_factory));
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_ecorebuffersink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_ecorebuffersink_setcaps);
  gstbasesink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_ecorebuffersink_get_times);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_ecorebuffersink_event);
  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_ecorebuffersink_show_frame);
}

/* Object typing & Creation */

static gboolean
plugin_init (GstPlugin * plugin)
{
  FUNCTION_ENTER ();

  if (!gst_element_register (plugin, "ecorebuffersink", GST_RANK_NONE,
          GST_TYPE_ECOREBUFFERSINK)) {
    return FALSE;
  }
  GST_DEBUG_CATEGORY_INIT (gst_debug_ecorebuffersink, "ecorebuffersink", 0,
      "ecorebuffersink element");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    ecorebuffersink, "Ecore Buffer sender plugin using Ecore buffer queue",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
