/*
 * EvasImageSink
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sangchul Lee <sc11.lee@samsung.com>
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

/* headers for drm */
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xlibint.h>
#include <dri2/dri2.h>
#include <libdrm/drm.h>
#include <tbm_bufmgr.h>

#include <sys/types.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include "evasimagesink.h"

/* Debugging category */
#include <gst/gstinfo.h>
GST_DEBUG_CATEGORY_STATIC (gst_debug_evasimagesink);
#define GST_CAT_DEFAULT gst_debug_evasimagesink
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);

enum
{
  UPDATE_FALSE,
  UPDATE_TRUE
};

enum
{
  PROP_0,
  PROP_EVAS_OBJECT,
  PROP_EVAS_OBJECT_SHOW,
  PROP_ROTATE_ANGLE,
  PROP_DISPLAY_GEOMETRY_METHOD,
  PROP_ENABLE_FLUSH_BUFFER,
  PROP_FLIP
};

#if 0
#define FUNCTION_ENTER()  GST_INFO("<ENTER>")
#else
#define FUNCTION_ENTER()
#endif
#define DEF_DISPLAY_GEOMETRY_METHOD                     DISP_GEO_METHOD_LETTER_BOX
#define DEF_DISPLAY_FLIP                                FLIP_NONE
#define SIZE_FOR_UPDATE_VISIBILITY                      sizeof(gchar)
#define EPIPE_REQUEST_LIMIT                             2
#define COLOR_DEPTH                                     4
#define GL_X11_ENGINE                                   "gl_x11"
#define DO_RENDER_FROM_FIMC                             1
#define SIZE_FOR_UPDATE_VISIBILITY                      sizeof(gchar)
#define MAX_ECOREPIPE_BUFFER_CNT                        4
#define DEBUGLOG_DEFAULT_COUNT                          8
#define SIZE_FOR_NATIVE_INDEX                           sizeof(gint)

#define EVASIMAGESINK_SET_EVAS_OBJECT_EVENT_CALLBACK( x_evas_image_object, x_usr_data ) \
do \
{ \
  if (x_evas_image_object) { \
    GST_LOG("object callback add"); \
    evas_object_event_callback_add (x_evas_image_object, EVAS_CALLBACK_DEL, gst_evasimagesink_callback_del_event, x_usr_data); \
    evas_object_event_callback_add (x_evas_image_object, EVAS_CALLBACK_RESIZE, gst_evasimagesink_callback_resize_event, x_usr_data); \
  } \
} while (0)

#define EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK( x_evas_image_object ) \
do \
{ \
  if (x_evas_image_object) { \
    GST_LOG("object callback del"); \
    evas_object_event_callback_del (x_evas_image_object, EVAS_CALLBACK_DEL, gst_evasimagesink_callback_del_event); \
    evas_object_event_callback_del (x_evas_image_object, EVAS_CALLBACK_RESIZE, gst_evasimagesink_callback_resize_event); \
  } \
} while (0)

#define EVASIMAGESINK_SET_EVAS_EVENT_CALLBACK( x_evas, x_usr_data ) \
do \
{ \
  if (x_evas) { \
    GST_DEBUG("callback add... gst_evasimagesink_callback_render_pre.. evas : %p esink : %p", x_evas, x_usr_data); \
    evas_event_callback_add (x_evas, EVAS_CALLBACK_RENDER_PRE, gst_evasimagesink_callback_render_pre, x_usr_data); \
  } \
} while (0)

#define EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK( x_evas ) \
do \
{ \
  if (x_evas) { \
    GST_DEBUG("callback del... gst_evasimagesink_callback_render_pre"); \
    evas_event_callback_del (x_evas, EVAS_CALLBACK_RENDER_PRE, gst_evasimagesink_callback_render_pre); \
  } \
} while (0)

GMutex instance_lock;
guint instance_lock_count;

static void gst_evasimagesink_callback_render_pre (void * data, Evas * e, void * event_info);
static GstFlowReturn gst_evasimagesink_epipe_reset (GstEvasImageSink * esink);
static gboolean gst_evasimagesink_make_flush_buffer (GstEvasImageSink * esink);
static void gst_evasimagesink_release_flush_buffer (GstEvasImageSink * esink);
static void gst_evasimagesink_update_geometry (GstEvasImageSink * esink, GstVideoRectangle * result);
static void gst_evasimagesink_apply_geometry (GstEvasImageSink * esink);

static void gst_evasimagesink_navigation_init (GstNavigationInterface * iface);
static void gst_evasimagesink_colorbalance_init (GstColorBalanceInterface * iface);

#ifdef DUMP_IMG
int util_write_rawdata (const char * file, const void * data, unsigned int size);
int g_cnt = 0;
typedef struct _BufferInfo
{
  Display *dpy;
  Pixmap pixmap;
  int width;
  int height;

  /* Dri2 */
  int drm_fd;
  tbm_bufmgr bufmgr;
  void *virtual;
  DRI2Buffer *dri2_buffers;
  tbm_bo bo;
} BufferInfo;
#endif
/* Default template - initiated with class struct to allow gst-register to work
   without X running */
static GstStaticPadTemplate gst_evasimagesink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

#define gst_evasimagesink_parent_class parent_class

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */

static void
gst_evasimagesink_cb_pipe (void * data, int * buffer_index, unsigned int nbyte)
{
  GstEvasImageSink *esink = data;

  FUNCTION_ENTER ();
  if (!esink || !esink->eo) {
    GST_WARNING_OBJECT (esink, "esink : %p, or eo is NULL returning", esink);
    return;
  }

  GST_LOG_OBJECT (esink, "esink : %p, esink->eo : %p", esink, esink->eo);

  if (nbyte == SIZE_FOR_UPDATE_VISIBILITY) {
    if (!esink->object_show) {
      evas_object_hide (esink->eo);
      GST_INFO_OBJECT (esink, "object hide..");
    } else {
      evas_object_show (esink->eo);
      GST_INFO_OBJECT (esink, "object show..");
    }
    GST_DEBUG_OBJECT (esink, "[LEAVE]");
    return;
  }

  int index = 0;
  index = *buffer_index;
  if ((index < 0 || index >= TBM_SURFACE_NUM) || nbyte != SIZE_FOR_NATIVE_INDEX) {
    GST_WARNING_OBJECT (esink, "index : %d, nbyte : %d", index, nbyte);
    return;
  }

  if (GST_STATE (esink) < GST_STATE_PAUSED) {
    GST_WARNING_OBJECT (esink, "WRONG-STATE(%d) for rendering, skip this frame", GST_STATE (esink));
    return;
  }

  g_mutex_lock (&(esink->display_buffer_lock));

  if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
    if (!esink->displaying_buffer[index].n_surface) {
      GST_ERROR_OBJECT (esink, "the index's nbuffer was already NULL, so return");
      g_mutex_unlock (&(esink->display_buffer_lock));
      return;
    }
  } else if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
    GST_LOG_OBJECT (esink, "received (bo %p) index num : %d", esink->displaying_buffer[index].bo, index);
  } else {
    GST_ERROR_OBJECT (esink, "Format (%d) is not supported", esink->tbm_surface_format);
  }

  Evas_Native_Surface surf;
  surf.type = EVAS_NATIVE_SURFACE_TBM;
  surf.version = EVAS_NATIVE_SURFACE_VERSION;
  surf.data.tizen.buffer = esink->displaying_buffer[index].n_surface;
  surf.data.tizen.rot = esink->rotate_angle;
  surf.data.tizen.flip = esink->flip;

  /*Added to make same rotation and flip with xvimagesink*/
  if (esink->rotate_angle == 0 || esink->rotate_angle == 180) {
    if (esink->flip == 1)
        surf.data.tizen.flip = 2;
    else if (esink->flip ==  2)
        surf.data.tizen.flip = 1;
  }

  GST_LOG_OBJECT (esink, "received (gst %p) index num : %d", esink->displaying_buffer[index].buffer, index);

  GstVideoRectangle result = { 0 };

  evas_object_geometry_get (esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
  if (!esink->eo_size.w || !esink->eo_size.h) {
    GST_ERROR_OBJECT (esink, "there is no information for evas object size");
    goto FAILED;
  }

  gst_evasimagesink_update_geometry (esink, &result);
  if (!result.w || !result.h) {
    GST_ERROR_OBJECT (esink, "no information about geometry (%d, %d)", result.w, result.h);
    goto FAILED;
  }

  if (esink->use_ratio) {
    surf.data.tizen.ratio = (float) esink->w / esink->h;
    GST_LOG_OBJECT (esink, "set ratio for letter mode");
  } else {
    surf.data.tizen.ratio = 0;
  }

  evas_object_size_hint_align_set (esink->eo, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set (esink->eo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

  if (!esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
    evas_object_image_size_set (esink->eo, esink->w, esink->h);
    esink->is_evas_object_size_set = TRUE;
  }
  evas_object_image_native_surface_set (esink->eo, &surf);

  GST_DEBUG_OBJECT (esink, "native surface set finish");

  if (result.x || result.y) {
    GST_LOG_OBJECT (esink, "coordinate x, y (%d, %d) for locating video to center", result.x, result.y);
  }

  evas_object_image_fill_set (esink->eo, result.x, result.y, result.w, result.h);
  evas_object_image_pixels_dirty_set (esink->eo, EINA_TRUE);

  GST_DEBUG_OBJECT (esink, "GEO_METHOD : src(%dx%d), dst(%dx%d), dst_x(%d), dst_y(%d), rotate(%d), flip(%d)",
    esink->w, esink->h, esink->eo_size.w, esink->eo_size.h, esink->eo_size.x, esink->eo_size.y, esink->rotate_angle, esink->flip);

  /* unref previous buffer */
  if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
    if (esink->prev_buf && esink->displaying_buffer[esink->prev_index].ref_count) {
      GST_DEBUG_OBJECT (esink, "before index %d's ref_count = %d, gst_buf %p", esink->prev_index, esink->displaying_buffer[esink->prev_index].ref_count, esink->prev_buf);
      esink->displaying_buffer[esink->prev_index].ref_count--;
      GST_DEBUG_OBJECT (esink, "after index %d's ref_count = %d, gst_buf %p", esink->prev_index, esink->displaying_buffer[esink->prev_index].ref_count, esink->prev_buf);

      /* Print debug log for 8 frame */
      if (esink->debuglog_cnt_ecoreCbPipe > 0) {
        GST_WARNING_OBJECT (esink, "(%d) ecore_cb_pipe unref index[%d].. gst_buf %p", DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_ecoreCbPipe), esink->prev_index, esink->prev_buf);
        esink->debuglog_cnt_ecoreCbPipe--;
      }

      if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT) {
        GST_WARNING ("sent buffer cnt 4->3 so skip will be stop");
      }

      esink->sent_buffer_cnt--;
      GST_DEBUG_OBJECT (esink, "prev gst_buffer %p's unref Start!!", esink->prev_buf);
      gst_buffer_unref (esink->prev_buf);
      GST_DEBUG_OBJECT (esink, "prev gst_buffer %p's unref End!!", esink->prev_buf);
    } else {
      GST_DEBUG_OBJECT (esink, "ref_count=%d  unref prev gst_buffer %p", esink->displaying_buffer[esink->prev_index].ref_count,esink->prev_buf);
    }

    GST_DEBUG_OBJECT (esink, "Current gst_buf %p and index %d is overwrited to Prev gst_buf %p & index %d",
      esink->displaying_buffer[index].buffer, index, esink->prev_buf, esink->prev_index );
    esink->prev_buf = esink->displaying_buffer[index].buffer;
    esink->prev_index = index;
  } else if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
    /* Print debug log for 8 frame */
    if (esink->debuglog_cnt_ecoreCbPipe > 0) {
      GST_WARNING_OBJECT (esink, "(%d) ecore_cb_pipe set tbm surface [%d] n_surface[%p]",
        DEBUGLOG_DEFAULT_COUNT-(esink->debuglog_cnt_ecoreCbPipe), index, esink->displaying_buffer[index].n_surface);
      esink->debuglog_cnt_ecoreCbPipe--;
    }

    if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT) {
      GST_WARNING_OBJECT (esink, "sent buffer cnt 4->3 so skip will be stop");
    }

    esink->sent_buffer_cnt--;
  }
  g_mutex_unlock (&(esink->display_buffer_lock));

  GST_DEBUG_OBJECT (esink, "[LEAVE]");
  return;

FAILED:
  if (esink->sent_buffer_cnt == MAX_ECOREPIPE_BUFFER_CNT)
    GST_WARNING_OBJECT (esink, "sent buffer cnt 4->3 so skip will be stop");

  esink->sent_buffer_cnt--;

  if ((esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) &&
        esink->displaying_buffer[index].buffer&& esink->displaying_buffer[index].ref_count) {
    GST_ERROR_OBJECT (esink, "before index %d's ref_count =%d, gst_buf %p",index,esink->displaying_buffer[index].ref_count, esink->displaying_buffer[index].buffer);
    esink->displaying_buffer[index].ref_count--;
    GST_ERROR_OBJECT (esink, "after index %d's ref_count =%d, gst_buf %p",index,esink->displaying_buffer[index].ref_count, esink->displaying_buffer[index].buffer);

    GST_ERROR_OBJECT (esink, "prev gst_buffer %p's unref Start!!", esink->displaying_buffer[index].buffer);
    gst_buffer_unref (esink->displaying_buffer[index].buffer);
    GST_ERROR_OBJECT (esink, "prev gst_buffer %p's unref End!!", esink->displaying_buffer[index].buffer);
  }
  g_mutex_unlock (&(esink->display_buffer_lock));

  GST_DEBUG_OBJECT (esink, "[LEAVE]");
}

static void
gst_evasimagesink_callback_resize_event (void * data, Evas * e, Evas_Object * obj, void *event_info)
{
  FUNCTION_ENTER ();

  int x, y, w, h;
  x = y = w = h = 0;

  GstEvasImageSink *esink = data;
  if (!esink || !esink->eo) {
    GST_WARNING ("esink : %p, or eo is NULL returning", esink);
    return;
  }

  evas_object_geometry_get (esink->eo, &x, &y, &w, &h);
  if (!w || !h) {
    GST_WARNING_OBJECT (esink, "evas object size (w:%d,h:%d) was not set", w, h);
  } else {
    esink->eo_size.x = x;
    esink->eo_size.y = y;
    esink->eo_size.w = w;
    esink->eo_size.h = h;
    GST_WARNING_OBJECT (esink, "resize (x:%d, y:%d, w:%d, h:%d)", x, y, w, h);
    gst_evasimagesink_apply_geometry (esink);
  }
}

static void
gst_evasimagesink_callback_render_pre (void * data, Evas * e, void * event_info)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = data;
  if (!esink || !esink->eo) {
    GST_WARNING ("esink : %p, or eo is NULL returning", esink);
    return;
  }

  if (esink->need_flush && esink->flush_buffer) {
    g_mutex_lock (&(esink->display_buffer_lock));
    Evas_Native_Surface surf;
    GstVideoRectangle result = { 0 };

    evas_object_geometry_get (esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
    if (!esink->eo_size.w || !esink->eo_size.h) {
      GST_ERROR_OBJECT (esink, "there is no information for evas object size");
      return;
    }

    gst_evasimagesink_update_geometry (esink, &result);
    if (!result.w || !result.h) {
      GST_ERROR_OBJECT (esink, "no information about geometry (%d, %d)", result.w, result.h);
      return;
    }

    if (esink->use_ratio) {
      surf.data.tizen.ratio = (float) esink->w / esink->h;
      GST_LOG_OBJECT (esink, "set ratio for letter mode");
    } else {
      surf.data.tizen.ratio = 0;
    }


    evas_object_size_hint_align_set (esink->eo, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_weight_set (esink->eo, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    if (!esink->is_evas_object_size_set && esink->w > 0 && esink->h > 0) {
      evas_object_image_size_set (esink->eo, esink->w, esink->h);
      esink->is_evas_object_size_set = TRUE;
    }

    if (result.x || result.y) {
      GST_LOG_OBJECT (esink, "coordinate x, y (%d, %d) for locating video to center", result.x, result.y);
    }

    evas_object_image_fill_set (esink->eo, result.x, result.y, result.w, result.h);

    surf.type = EVAS_NATIVE_SURFACE_TBM;
    surf.data.tizen.buffer = esink->flush_buffer->n_surface;
    surf.version = EVAS_NATIVE_SURFACE_VERSION;
    surf.data.tizen.rot = esink->rotate_angle;
    surf.data.tizen.flip = esink->flip;

    GST_DEBUG_OBJECT (esink, "use esink->flush buffer->n_surface (%p), rotate(%d), flip(%d)",
      esink->flush_buffer->n_surface, esink->rotate_angle, esink->flip);

    evas_object_image_native_surface_set (esink->eo, &surf);
    g_mutex_unlock (&(esink->display_buffer_lock));
    esink->need_flush = FALSE;
  }
  evas_object_image_pixels_dirty_set (esink->eo, EINA_TRUE);
  GST_LOG_OBJECT (esink, "dirty set finish");
}

gint
gst_evasimagesink_ref_count (GstBuffer * buf)
{
  FUNCTION_ENTER ();

  return GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER_CAST(buf));
}

static gboolean
gst_evasimagesink_release_source_buffer (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();

  int i = 0;

  g_mutex_lock (&(esink->display_buffer_lock));

  for (i = 0; i < TBM_SURFACE_NUM; i++) {
    GST_WARNING_OBJECT (esink, "[reset] reset gst %p", esink->displaying_buffer[i].buffer);
    esink->displaying_buffer[i].bo = NULL;
    esink->displaying_buffer[i].n_surface = NULL;
    esink->displaying_buffer[i].ref_count = 0;
  }

  for (i = 0; i < SOURCE_BUFFER_NUM; i++) {
    if (esink->src_buffer_info[i].n_surface) {
      tbm_surface_destroy (esink->src_buffer_info[i].n_surface);
      esink->src_buffer_info[i].n_surface = NULL;
    }
  }

  esink->is_buffer_allocated = FALSE;
  esink->src_buf_idx = 0;
  esink->prev_buf = NULL;
  esink->prev_index = -1;
  esink->cur_index = -1;

  esink->eo_size.x = esink->eo_size.y =
  esink->eo_size.w = esink->eo_size.h = 0;
  esink->use_ratio = FALSE;
  esink->sent_buffer_cnt = 0;

  g_mutex_unlock (&(esink->display_buffer_lock));

  return TRUE;
}

static gboolean
gst_evasimagesink_allocate_source_buffer (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();

  int idx = 0;

  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return FALSE;
  }

  /* create native buffer provider for making native buffer */
  for (idx = 0; idx < SOURCE_BUFFER_NUM; idx++) {
    /* create native buffer */
    if (!esink->src_buffer_info[idx].n_surface)
      esink->src_buffer_info[idx].n_surface = tbm_surface_create (esink->w, esink->h, esink->tbm_surface_format);

    if (!esink->src_buffer_info[idx].n_surface) {
      GST_ERROR_OBJECT (esink, "n_surface is NULL!!");
      goto ALLOC_FAILED;
    }

    GST_WARNING_OBJECT (esink, "src buffer index:%d , native surface : %p", idx, esink->src_buffer_info[idx].n_surface);
  }

  return TRUE;

ALLOC_FAILED:
  gst_evasimagesink_release_source_buffer (esink);
  return FALSE;
}

static int
gst_evasimagesink_get_size_from_caps (GstCaps * caps, int * w, int * h)
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

static inline gboolean
gst_evasimagesink_is_evas_image_object (Evas_Object * obj)
{
  FUNCTION_ENTER ();

  const char *type;
  if (!obj) {
    GST_WARNING ("Invalid evas object.");
    return FALSE;
  }
  type = evas_object_type_get (obj);
  if (!type) {
    GST_WARNING ("Invalid evas object type.");
    return FALSE;
  }
  if (strcmp (type, "image") != 0) {
    GST_WARNING ("Evas object is not of image type.");
    return FALSE;
  }
  return TRUE;
}

static void
gst_evasimagesink_callback_del_event (void * data, Evas * e, Evas_Object * obj, void * event_info)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = data;
  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return;
  }

  if (esink->eo) {
    EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get (esink->eo));
    GST_DEBUG_OBJECT (esink, "unset EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK esink : %p, esink->eo : %p", esink, esink->eo);

    EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
    GST_DEBUG_OBJECT (esink, "unset EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK esink : %p, esink->eo : %p", esink, esink->eo);

    evas_object_image_data_set (esink->eo, NULL);
    evas_object_image_pixels_dirty_set (esink->eo, EINA_TRUE);
    esink->eo = NULL;
  }
}

#ifdef DUMP_IMG
int
util_write_rawdata (const char * file, const void * data, unsigned int size)
{
  FUNCTION_ENTER ();

  FILE *fp;

  fp = fopen (file, "wb");
  if (fp == NULL) {
    GST_WARNING ("fopen fail... size : %d", size);
    return -1;
  }
  fwrite ((char *) data, sizeof (char), size, fp);
  fclose (fp);

  return 0;
}

static inline void
input_dump (MMVideoBuffer * inbuf)
{
  FUNCTION_ENTER ();

  char *temp = (char *)inbuf->data[0];
  int i = 0;
  char filename[100]={0};
  FILE *fp = NULL;

  GST_WARNING ("IN_DUMP_%2.2d : width=%d, height=%d, stride_width=%d, stride_height=%d",
    g_cnt, inbuf->width[0], inbuf->height[0], inbuf->stride_width[0], inbuf->stride_height[0]);

  sprintf (filename, "/opt/usr/media/IN_DUMP_%2.2d.dump", g_cnt++);
  fp = fopen (filename, "ab");

  if (!fp) {
    GST_ERROR ("%s open failed", filename);
    return;
  }

  for (i = 0; i < inbuf->height[0]; i++) {
    fwrite (temp, inbuf->width[0], 1, fp);
    temp += inbuf->stride_width[0];
  }

  temp = (char*)inbuf->data[0] + inbuf->stride_width[0] * inbuf->stride_height[0];

  for (i = 0; i < (inbuf->height[0]/2); i++) {
    fwrite (temp, inbuf->width[0], 1, fp);
    temp += inbuf->stride_width[0];
  }
  fclose (fp);
}
#endif

static gboolean
gst_evasimagesink_drm_init (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();

  Display *dpy;
  int eventBase = 0;
  int errorBase = 0;
  int dri2Major = 0;
  int dri2Minor = 0;
  char *driverName = NULL;
  char *deviceName = NULL;
  struct drm_auth auth_arg = { 0 };

  esink->drm_fd = -1;

  GST_INFO_OBJECT (esink, "gst_evasimagesink_drm_init enter");

  dpy = XOpenDisplay (0);

  /* DRI2 */
  if (!DRI2QueryExtension (dpy, &eventBase, &errorBase)) {
    GST_ERROR_OBJECT (esink, "failed to DRI2QueryExtension()");
    goto ERROR_CASE;
  }

  if (!DRI2QueryVersion (dpy, &dri2Major, &dri2Minor)) {
    GST_ERROR_OBJECT (esink, "failed to DRI2QueryVersion");
    goto ERROR_CASE;
  }

  if (!DRI2Connect (dpy, RootWindow (dpy, DefaultScreen (dpy)), &driverName, &deviceName)) {
    GST_ERROR_OBJECT (esink, "failed to DRI2Connect");
    goto ERROR_CASE;
  }

  /* get the drm_fd though opening the deviceName */
  esink->drm_fd = open (deviceName, O_RDWR);
  if (esink->drm_fd < 0) {
    GST_ERROR_OBJECT (esink, "cannot open drm device (%s)", deviceName);
    goto ERROR_CASE;
  }
  GST_INFO_OBJECT (esink, "Open drm device : %s, fd(%d)", deviceName, esink->drm_fd);

  /* get magic from drm to authentication */
  if (ioctl (esink->drm_fd, DRM_IOCTL_GET_MAGIC, &auth_arg)) {
    GST_ERROR_OBJECT (esink, "cannot get drm auth magic");
    close (esink->drm_fd);
    esink->drm_fd = -1;
    goto ERROR_CASE;
  }

  if (!DRI2Authenticate (dpy, RootWindow (dpy, DefaultScreen (dpy)), auth_arg.magic)) {
    GST_ERROR_OBJECT (esink, "cannot get drm authentication from X");
    close (esink->drm_fd);
    esink->drm_fd = -1;
    goto ERROR_CASE;
  }

  /* drm slp buffer manager init */
  esink->bufmgr = tbm_bufmgr_init (esink->drm_fd);
  if (!esink->bufmgr) {
    GST_ERROR_OBJECT (esink, "fail to init buffer manager");
    close (esink->drm_fd);
    esink->drm_fd = -1;
    goto ERROR_CASE;
  }

  XCloseDisplay (dpy);
  free (driverName);
  free (deviceName);

  GST_INFO_OBJECT (esink, "gst_evasimagesink_drm_init leave");
  return TRUE;

ERROR_CASE:
  XCloseDisplay (dpy);
  if (!driverName) {
    free (driverName);
  }

  if (!deviceName) {
    free (deviceName);
  }

  return FALSE;
}

static void
gst_evasimagesink_drm_fini (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();

  if (esink->bufmgr >= 0) {
    GST_INFO_OBJECT (esink, "destroying tbm buffer manager");
    tbm_bufmgr_deinit (esink->bufmgr);
    esink->bufmgr = NULL;
  }

  if (esink->drm_fd >= 0) {
    GST_INFO_OBJECT (esink, "closing drm_fd(%d)", esink->drm_fd);
    close (esink->drm_fd);
    esink->drm_fd = -1;
  }
}

static void
gst_evasimagesink_fini (gpointer data, GObject * obj)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = GST_EVASIMAGESINK (obj);
  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return;
  }

  if (esink->eo) {
    EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get (esink->eo));
    GST_DEBUG_OBJECT (esink, "unset EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK esink : %p, esink->eo : %p", esink, esink->eo);

    EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
    GST_DEBUG_OBJECT (esink, "unset EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK esink : %p, esink->eo : %p", esink, esink->eo);

    evas_object_image_data_set (esink->eo, NULL);
    evas_object_image_pixels_dirty_set (esink->eo, EINA_TRUE);
  }

  g_mutex_clear(&(esink->display_buffer_lock));
  g_mutex_clear(&(esink->flow_lock));

  esink->eo = NULL;
  esink->epipe = NULL;

  g_mutex_lock (&instance_lock);
  instance_lock_count--;
  g_mutex_unlock (&instance_lock);
  if (instance_lock_count == 0) {
    g_mutex_clear (&instance_lock);
  }

  GST_DEBUG_OBJECT (esink, "[LEAVE]");
}

static void
gst_evasimagesink_reset (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();

  int i = 0;

  g_mutex_lock (&(esink->display_buffer_lock));

  for (i = 0; i < TBM_SURFACE_NUM; i++) {
    if (esink->displaying_buffer[i].n_surface) {
      tbm_surface_destroy (esink->displaying_buffer[i].n_surface);
      esink->displaying_buffer[i].n_surface = NULL;
    }

    if (esink->displaying_buffer[i].buffer) {
      if (esink->displaying_buffer[i].ref_count) {
        GST_WARNING_OBJECT (esink, "[reset] unreffing gst %p", esink->displaying_buffer[i].buffer);

        while (esink->displaying_buffer[i].ref_count) {
          GST_WARNING_OBJECT (esink, "index[%d]'s buffer ref count = %d", i,
            gst_evasimagesink_ref_count (esink->displaying_buffer[i].buffer));
          esink->displaying_buffer[i].ref_count--;
          gst_buffer_unref (esink->displaying_buffer[i].buffer);
        }
      }
      esink->displaying_buffer[i].buffer = NULL;
    }
    if (esink->displaying_buffer[i].bo)
      esink->displaying_buffer[i].bo = NULL;
  }

  esink->prev_buf = NULL;
  esink->prev_index = -1;
  esink->cur_index = -1;

  esink->eo_size.x = esink->eo_size.y =
  esink->eo_size.w = esink->eo_size.h = 0;
  esink->use_ratio = FALSE;
  esink->sent_buffer_cnt = 0;

  g_mutex_unlock (&(esink->display_buffer_lock));
}

static GstFlowReturn
gst_evasimagesink_epipe_reset (GstEvasImageSink * esink)
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
    esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)gst_evasimagesink_cb_pipe, esink);
    if (!esink->epipe) {
      GST_ERROR_OBJECT (esink, "ecore-pipe create failed");
      return GST_FLOW_ERROR;
    }
    GST_DEBUG_OBJECT (esink, "ecore-pipe create success");
  }

  return GST_FLOW_OK;
}

static gboolean
gst_evasimagesink_make_flush_buffer (GstEvasImageSink * esink)
{
  GstEvasImageTbmFlushBuffer *flush_buffer = NULL;
  GstEvasImageTbmDisplayingBuffer *display_buffer = NULL;
  tbm_surface_info_s surfaceInfoSrc = { 0 };
  tbm_surface_info_s surfaceInfoDst = { 0 };
  GstFlowReturn ret = GST_FLOW_OK;
  int i = 0, j = 0;

  if (esink == NULL) {
    GST_ERROR ("handle is NULL");
    return FALSE;
  }

  if (esink->cur_index == -1) {
    GST_WARNING_OBJECT (esink, "there is no remained buffer");
    return FALSE;
  }

  if (esink->drm_fd < 0 || esink->bufmgr == NULL) {
    GST_ERROR_OBJECT (esink, "drm fd[%d] or bufmgr[%p] is invalid", esink->drm_fd, esink->bufmgr);
    return FALSE;
  }

  if (esink->flush_buffer)
    gst_evasimagesink_release_flush_buffer (esink);

  /* malloc buffer */
  flush_buffer = (GstEvasImageTbmFlushBuffer *) malloc (sizeof(GstEvasImageTbmFlushBuffer));
  if (flush_buffer == NULL) {
    GST_ERROR_OBJECT (esink, "GstEvasImageTbmFlushBuffer alloc failed");
    return FALSE;
  }

  memset (flush_buffer, 0x0, sizeof (GstEvasImageTbmFlushBuffer));

  display_buffer = &(esink->displaying_buffer[esink->cur_index]);
  GST_WARNING_OBJECT (esink, "cur_index [%d]", esink->cur_index);

  if (!display_buffer || !display_buffer->n_surface) {
    GST_WARNING_OBJECT (esink, "display_buffer(%p) or n_surface (%p) is NULL!!", display_buffer, display_buffer->n_surface);
    goto FLUSH_BUFFER_FAILED;
  }

  /* create tbm surface with allocated bos */
  if (tbm_surface_get_info (display_buffer->n_surface, &surfaceInfoSrc) == TBM_SURFACE_ERROR_NONE) {
    GST_WARNING_OBJECT (esink, "Source surface info present");
    flush_buffer->bo_num = tbm_surface_internal_get_num_bos (display_buffer->n_surface);

    for (i = 0; i < flush_buffer->bo_num; i++) {
      flush_buffer->flush_buffer_bo[i] = tbm_bo_alloc (esink->bufmgr, surfaceInfoSrc.size, TBM_BO_DEFAULT);

      if (!flush_buffer->flush_buffer_bo[i]) {
        GST_ERROR_OBJECT (esink, "failed to tbm_bo_alloc size = %d, index = %d", surfaceInfoSrc.size, i);
        for (j = 0; j < i; j++) {
          tbm_bo_unref (flush_buffer->flush_buffer_bo[j]);
          flush_buffer->flush_buffer_bo[j] = NULL;
        }
        flush_buffer->bo_num = 0;
        goto FLUSH_BUFFER_FAILED;
      }
    }
  } else {
    GST_ERROR_OBJECT (esink, "Source surface info not present!!!");
    goto FLUSH_BUFFER_FAILED;
  }

  flush_buffer->n_surface = tbm_surface_internal_create_with_bos (&surfaceInfoSrc, flush_buffer->flush_buffer_bo,
    flush_buffer->bo_num);

  if (!flush_buffer->n_surface) {
    GST_ERROR_OBJECT (esink, "n_surface is NULL!!");
    for (i = 0; i < flush_buffer->bo_num; i++) {
      tbm_bo_unref (flush_buffer->flush_buffer_bo[i]);
      flush_buffer->flush_buffer_bo[i] = NULL;
    }
    flush_buffer->bo_num = 0;
    goto FLUSH_BUFFER_FAILED;
  }

  /* get surface info */
  if (tbm_surface_map (flush_buffer->n_surface, TBM_SURF_OPTION_WRITE, &surfaceInfoDst) == TBM_SURFACE_ERROR_NONE) {
    GST_WARNING_OBJECT (esink, "Destination surface info present");
  } else {
    GST_WARNING_OBJECT (esink, "Destination surface info not present!!!");
  }

  /* copy buffer */
  for (i = 0; i < surfaceInfoDst.num_planes; i++) {
    if (surfaceInfoDst.planes[i].ptr && surfaceInfoSrc.planes[i].ptr) {
      memcpy(surfaceInfoDst.planes[i].ptr, surfaceInfoSrc.planes[i].ptr, surfaceInfoSrc.planes[i].size);
    }
  }

  tbm_surface_unmap (flush_buffer->n_surface);

  GST_WARNING_OBJECT (esink, "copy done.. TBM surface : %p", flush_buffer->n_surface);

  esink->flush_buffer = flush_buffer;

  /* initialize buffer list */
  if (esink->object_show)
    esink->need_flush = TRUE;

  ret = gst_evasimagesink_epipe_reset (esink);
  if (ret) {
    GST_ERROR_OBJECT (esink, "evas epipe reset ret = %d, need to check", ret);
    return FALSE;
  } else {
    GST_DEBUG_OBJECT (esink, "evas epipe reset success");
  }

  if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
    gst_evasimagesink_release_source_buffer (esink);
  } else if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
    gst_evasimagesink_reset (esink);
  }

  return TRUE;

FLUSH_BUFFER_FAILED:
  if (flush_buffer) {
    if (flush_buffer->n_surface) {
      tbm_surface_destroy (flush_buffer->n_surface);
      flush_buffer->n_surface = NULL;
    }

    for (i = 0; i < flush_buffer->bo_num; i++) {
      if (flush_buffer->flush_buffer_bo[i]) {
        tbm_bo_unref (flush_buffer->flush_buffer_bo[i]);
        flush_buffer->flush_buffer_bo[i] = NULL;
      }
    }

    flush_buffer->bo_num = 0;
    free (flush_buffer);
    flush_buffer = NULL;
  }
  return FALSE;
}

static void
gst_evasimagesink_release_flush_buffer (GstEvasImageSink * esink)
{
  int index = 0;

  if (esink == NULL || esink->flush_buffer == NULL) {
    GST_WARNING ("handle is NULL");
    return;
  }

  GST_WARNING_OBJECT (esink, "release FLUSH BUFFER start");

  if (esink->flush_buffer->n_surface) {
    tbm_surface_destroy (esink->flush_buffer->n_surface);
    esink->flush_buffer->n_surface = NULL;
  }
  for (index = 0; index < esink->flush_buffer->bo_num; index++) {
    if (esink->flush_buffer->flush_buffer_bo[index]) {
      tbm_bo_unref (esink->flush_buffer->flush_buffer_bo[index]);
      esink->flush_buffer->flush_buffer_bo[index] = NULL;
    }
  }
  esink->flush_buffer->bo_num = 0;

  GST_WARNING_OBJECT (esink, "release FLUSH BUFFER done");

  free (esink->flush_buffer);
  esink->flush_buffer = NULL;

  return;
}

static void
gst_evasimagesink_update_geometry (GstEvasImageSink * esink, GstVideoRectangle * result)
{
  if (!esink || !esink->eo) {
    GST_WARNING ("handle is NULL");
    return;
  }

  int tmp = esink->w;

  if (esink->rotate_angle == 90 || esink->rotate_angle == 270) {
    esink->w = esink->h;
    esink->h = tmp;
  }
  result->x = 0;
  result->y = 0;

  switch (esink->display_geometry_method)
  {
    case DISP_GEO_METHOD_LETTER_BOX:  // 0
      /* set black padding for letter box mode */
      GST_DEBUG_OBJECT (esink, "letter box mode");
      esink->use_ratio = TRUE;
      result->w = esink->eo_size.w;
      result->h = esink->eo_size.h;
      break;
    case DISP_GEO_METHOD_ORIGIN_SIZE: // 1
      GST_DEBUG_OBJECT (esink, "origin size mode");
      esink->use_ratio = FALSE;
      /* set coordinate for each case */
      result->x = (esink->eo_size.w-esink->w) / 2;
      result->y = (esink->eo_size.h-esink->h) / 2;
      result->w = esink->w;
      result->h = esink->h;
      break;
    case DISP_GEO_METHOD_FULL_SCREEN: // 2
      GST_DEBUG_OBJECT (esink, "full screen mode");
      esink->use_ratio = FALSE;
      result->w = esink->eo_size.w;
      result->h = esink->eo_size.h;
      break;
    case DISP_GEO_METHOD_CROPPED_FULL_SCREEN:       // 3
      GST_DEBUG_OBJECT (esink, "cropped full screen mode");
      esink->use_ratio = FALSE;
      /* compare evas object's ratio with video's */
      if (((gdouble)esink->eo_size.w/esink->eo_size.h) > ((gdouble)esink->w/esink->h)) {
        result->w = esink->eo_size.w;
        result->h = esink->eo_size.w * esink->h / esink->w;
        result->y = -(result->h-esink->eo_size.h) / 2;
      } else {
        result->w = esink->eo_size.h * esink->w / esink->h;
        result->h = esink->eo_size.h;
        result->x = -(result->w-esink->eo_size.w) / 2;
      }
      break;
    case DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX: // 4
      GST_DEBUG_OBJECT (esink, "origin size or letter box mode");
      /* if video size is smaller than evas object's, it will be set to origin size mode */
      if ((esink->eo_size.w > esink->w) && (esink->eo_size.h > esink->h)) {
        GST_DEBUG_OBJECT (esink, "origin size mode");
        esink->use_ratio = FALSE;
        /* set coordinate for each case */
        result->x = (esink->eo_size.w-esink->w) / 2;
        result->y = (esink->eo_size.h-esink->h) / 2;
        result->w = esink->w;
        result->h = esink->h;
      } else {
        GST_DEBUG_OBJECT (esink, "letter box mode");
        esink->use_ratio = TRUE;
        result->w = esink->eo_size.w;
        result->h = esink->eo_size.h;
      }
      break;
    default:
      GST_WARNING_OBJECT (esink, "unsupported mode.");
      break;
  }

  if (esink->rotate_angle == 90 || esink->rotate_angle == 270) {
    esink->h = esink->w;
    esink->w = tmp;
  }

  GST_DEBUG_OBJECT (esink, "geometry result [%d, %d, %d, %d]", result->x, result->y, result->w, result->h);
}

static void
gst_evasimagesink_apply_geometry (GstEvasImageSink * esink)
{
  if (!esink || !esink->eo) {
    GST_WARNING ("there is no esink");
    return;
  }

  Evas_Native_Surface *surf = evas_object_image_native_surface_get (esink->eo);
  GstVideoRectangle result = { 0 };

  if (surf) {
    GST_DEBUG_OBJECT (esink, "native surface exists");
    surf->data.tizen.rot = esink->rotate_angle;
    surf->data.tizen.flip = esink->flip;
    evas_object_image_native_surface_set (esink->eo, surf);

    gst_evasimagesink_update_geometry (esink, &result);

    if (esink->use_ratio) {
      surf->data.tizen.ratio = (float) esink->w / esink->h;
      GST_LOG_OBJECT (esink, "set ratio for letter mode");
    } else {
      surf->data.tizen.ratio = 0;
    }

    if (result.x || result.y)
      GST_LOG_OBJECT (esink, "coordinate x, y (%d, %d) for locating video to center", result.x, result.y);

    evas_object_image_fill_set (esink->eo, result.x, result.y, result.w, result.h);
  }
  else
    GST_WARNING_OBJECT (esink, "there is no native surface");
}

G_DEFINE_TYPE_EXTENDED (GstEvasImageSink, gst_evasimagesink,
    GST_TYPE_VIDEO_SINK, 0, G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_evasimagesink_navigation_init)
    G_IMPLEMENT_INTERFACE (GST_TYPE_COLOR_BALANCE,
        gst_evasimagesink_colorbalance_init));

/* Element stuff */
static GstCaps *
gst_evasimagesink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  FUNCTION_ENTER ();

  GST_WARNING ("Not implemented!!!");

  return NULL;
}

static gboolean
gst_evasimagesink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  FUNCTION_ENTER ();

  int r;
  int w, h;
  GstEvasImageSink *esink = GST_EVASIMAGESINK (bsink);
  GstStructure *structure = NULL;
  guint32 format = 0;
  int ret = 0;
  const gchar *fmt;
  GstVideoInfo info;

  GST_DEBUG_OBJECT (esink, "In setcaps. setting caps %"GST_PTR_FORMAT, caps);

  esink->is_evas_object_size_set = FALSE;
  r = gst_evasimagesink_get_size_from_caps (caps, &w, &h);
  if (!r) {
    g_mutex_lock (&instance_lock);
    if ((esink->w != 0 && esink->h != 0) && (esink->w != w || esink->h != h)) {
      if (GST_STATE (esink) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT (esink, "new caps (%d,%d), esink (%d,%d)", w, h, esink->w, esink->h);

        if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
          /* Zero copy formats */
          if (gst_evasimagesink_make_flush_buffer (esink) == FALSE) {
            ret = gst_evasimagesink_epipe_reset (esink);
            if (ret) {
              GST_ERROR_OBJECT (esink, "evas epipe reset ret=%d, need to check", ret);
              g_mutex_unlock (&instance_lock);
              return FALSE;
            }

            gst_evasimagesink_reset (esink);
          }
        } else if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
          ret = gst_evasimagesink_epipe_reset (esink);
          if (ret) {
            GST_ERROR_OBJECT (esink, "evas epipe reset failed ret = %d", ret);
            g_mutex_unlock (&instance_lock);
            return FALSE;
          }

          gst_evasimagesink_release_source_buffer (esink);
        } else {
          GST_WARNING_OBJECT (esink, "Unknown surface format %d", esink->tbm_surface_format);
        }
      }
    }
    esink->w = w;
    esink->h = h;
    g_mutex_unlock (&instance_lock);
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

  GST_DEBUG_OBJECT (esink, "source color format is %d", format);

  if (format == GST_VIDEO_FORMAT_SN12 || format == GST_VIDEO_FORMAT_NV12) {
    esink->tbm_surface_format = TBM_FORMAT_NV12;
  } else if (format == GST_VIDEO_FORMAT_I420) {
    esink->tbm_surface_format = TBM_FORMAT_YUV420;
  } else if (format == GST_VIDEO_FORMAT_SR32) {
    esink->tbm_surface_format = TBM_FORMAT_BGRA8888;
  } else {
    GST_ERROR_OBJECT (esink, "cannot parse fourcc format from caps.");
    return FALSE;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_evasimagesink_change_state (GstElement * element,
    GstStateChange transition)
{
  FUNCTION_ENTER ();

  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstEvasImageSink *evasimagesink;
  GstFlowReturn flowRet = GST_FLOW_OK;

  evasimagesink = GST_EVASIMAGESINK (element);

  if (!evasimagesink) {
    GST_ERROR_OBJECT (evasimagesink, "can not get evasimagesink from element");
    return GST_STATE_CHANGE_FAILURE;
  }

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_NULL_TO_READY");

      g_mutex_lock (&(evasimagesink->flow_lock));

      /* open drm to use gem */
      if (!gst_evasimagesink_drm_init (evasimagesink)) {
        GST_ERROR_OBJECT (evasimagesink,"gst_evasimagesink_drm_init() failure");
        g_mutex_unlock (&(evasimagesink->flow_lock));
        return GST_STATE_CHANGE_FAILURE;
      }

      if (!gst_evasimagesink_is_evas_image_object (evasimagesink->eo)) {
        GST_ERROR_OBJECT (evasimagesink, "There is no evas image object..");
        g_mutex_unlock (&(evasimagesink->flow_lock));
        return GST_STATE_CHANGE_FAILURE;
      }
      g_mutex_unlock (&(evasimagesink->flow_lock));
      break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_READY_TO_PAUSED");
      break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
      /* Print debug log for 8 frame */
      evasimagesink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
      evasimagesink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;
      break;

    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
      break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_PAUSED_TO_READY");
      /* flush buffer, we will copy last buffer to keep image data and reset buffer list */
      GST_WARNING("evasimagesink->enable_flush_buffer : %d", evasimagesink->enable_flush_buffer);
      if (evasimagesink->enable_flush_buffer &&
            (evasimagesink->tbm_surface_format == TBM_FORMAT_NV12 ||
             evasimagesink->tbm_surface_format == TBM_FORMAT_BGRA8888)) {
         if (gst_evasimagesink_make_flush_buffer (evasimagesink) == FALSE) {
          flowRet = gst_evasimagesink_epipe_reset (evasimagesink);
          if (flowRet) {
            GST_ERROR_OBJECT (evasimagesink, "evas epipe reset flowRet=%d, need to check", flowRet);
            return GST_STATE_CHANGE_FAILURE;
          }
          gst_evasimagesink_reset (evasimagesink);
        }
      } else {
        flowRet = gst_evasimagesink_epipe_reset (evasimagesink);
        if (flowRet) {
          GST_ERROR_OBJECT (evasimagesink, "evas epipe reset ret=%d, need to check", flowRet);
          return GST_STATE_CHANGE_FAILURE;
        }

        if (evasimagesink->tbm_surface_format == TBM_FORMAT_NV12 ||
            evasimagesink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
          gst_evasimagesink_reset (evasimagesink);
        } else if (evasimagesink->tbm_surface_format == TBM_FORMAT_YUV420) {
          gst_evasimagesink_release_source_buffer (evasimagesink);
        }
      }
      evasimagesink->fps_n = 0;
      evasimagesink->fps_d = 1;
      break;

    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG_OBJECT (evasimagesink, "GST_STATE_CHANGE_READY_TO_NULL");

      if (evasimagesink->flush_buffer)
        gst_evasimagesink_release_flush_buffer (evasimagesink);

      EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get (evasimagesink->eo));
      EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (evasimagesink->eo);

      if (evasimagesink->epipe) {
        GST_DEBUG_OBJECT (evasimagesink, "ecore-pipe will delete");
        ecore_pipe_del (evasimagesink->epipe);
        evasimagesink->epipe = NULL;
      }
      /* close drm */
      gst_evasimagesink_drm_fini (evasimagesink);
      break;

    default:
      break;
  }
  return ret;
}

static void
gst_evasimagesink_get_times (GstBaseSink * bsink, GstBuffer * buf, GstClockTime * start, GstClockTime * end)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = GST_EVASIMAGESINK (bsink);

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
gst_evasimagesink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  FUNCTION_ENTER ();
  GstEvasImageSink *esink = GST_EVASIMAGESINK (vsink);
  Eina_Bool r;
  GstMemory *mem;

  GST_LOG_OBJECT (esink, "[ENTER] show frame");

  if (!gst_evasimagesink_ref_count (buf)) {
    GST_WARNING_OBJECT (esink, "ref count is 0.. skip show frame");
    return GST_FLOW_OK;
  }

  g_mutex_lock (&instance_lock);
  if (!esink->epipe) {
    esink->epipe = ecore_pipe_add ((Ecore_Pipe_Cb)gst_evasimagesink_cb_pipe, esink);
    if (!esink->epipe) {
      GST_ERROR_OBJECT (esink, "ecore-pipe create failed");
      g_mutex_unlock (&instance_lock);
      return GST_FLOW_ERROR;
    }
    GST_DEBUG_OBJECT (esink, "ecore-pipe create success");
  }

  int i = 0;
  int index = -1;
  MMVideoBuffer *mm_video_buf = NULL;
  gboolean exist_bo = FALSE;
  tbm_surface_info_s info = { 0 };
  int offset = 0;
  int bo_num = 0;
  GstMapInfo buf_info = GST_MAP_INFO_INIT;

  if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
    /* get received buffer informations */
    mem = gst_buffer_peek_memory (buf, 1);
    gst_memory_map (mem, &buf_info, GST_MAP_READ);
    mm_video_buf = (MMVideoBuffer *) buf_info.data;
    gst_memory_unmap (mem, &buf_info);

    if (!mm_video_buf) {
      GST_WARNING_OBJECT (esink, "mm_video_buf is NULL. Skip..." );
      g_mutex_unlock (&instance_lock);
      return GST_FLOW_OK;
    }

    if (mm_video_buf->type == MM_VIDEO_BUFFER_TYPE_TBM_BO) {
      /* check whether bo is new or not */
      for (i = 0; i < TBM_SURFACE_NUM; i++) {
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
        for (i = 0; i < TBM_SURFACE_NUM; i++) {
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

          /* create new native buffer */
          for (i = 0, bo_num = 0; i < MM_VIDEO_BUFFER_PLANE_MAX; i++) {
            if (mm_video_buf->handle.bo[i]) {
              bo_num++;
            }
          }

          tsurf_info.width = mm_video_buf->width[0];
          tsurf_info.height = mm_video_buf->height[0];
          tsurf_info.format = esink->tbm_surface_format;
          tsurf_info.bpp = tbm_surface_internal_get_bpp(tsurf_info.format);
          tsurf_info.num_planes = tbm_surface_internal_get_num_planes(tsurf_info.format);

          if (esink->tbm_surface_format == TBM_FORMAT_NV12) {
            tsurf_info.planes[0].size = mm_video_buf->stride_width[0] * mm_video_buf->stride_height[0];
            tsurf_info.planes[1].size = mm_video_buf->stride_width[0] * mm_video_buf->stride_height[1];

            tsurf_info.planes[0].offset = 0;
            tsurf_info.planes[0].stride = mm_video_buf->stride_width[0];
            tsurf_info.planes[1].stride = mm_video_buf->stride_width[1];

            if (bo_num == 1)
              tsurf_info.planes[1].offset = tsurf_info.planes[0].size;

            tsurf_info.size = tsurf_info.planes[0].size + tsurf_info.planes[1].size;
          } else {
            // BGRA8888 has only 1 plane.
            tsurf_info.planes[0].size = mm_video_buf->size[0];
            tsurf_info.planes[0].stride = mm_video_buf->stride_width[0];
            tsurf_info.planes[0].offset = 0;
            tsurf_info.size = tsurf_info.planes[0].size;
          }

          esink->displaying_buffer[index].n_surface = tbm_surface_internal_create_with_bos (&tsurf_info,
            (tbm_bo *)mm_video_buf->handle.bo, bo_num);

          if (!esink->displaying_buffer[index].n_surface) {
            GST_WARNING_OBJECT (esink, "there is no native buffer.. bo : %p,  gst_buf : %p", esink->displaying_buffer[index].bo, esink->displaying_buffer[index].buffer);
            g_mutex_unlock (&instance_lock);
            return GST_FLOW_OK;
          }

          GST_WARNING_OBJECT (esink, "create native n_surface : %p", esink->displaying_buffer[index].n_surface);
        }
      } else {
        if (index != -1) {
          esink->displaying_buffer[index].buffer = buf;

          GST_DEBUG_OBJECT (esink, "existing native n_surface %p,  gst_buf %p", esink->displaying_buffer[index].n_surface, esink->displaying_buffer[index].buffer);
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
  } else if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
    static int skip_count_i420 = 0;
    if (esink->sent_buffer_cnt >= MAX_ECOREPIPE_BUFFER_CNT) {
      if (!(skip_count_i420++ % 20)) {
        GST_WARNING_OBJECT (esink, "[%d]EA buffer was already sent to ecore pipe, and %d frame skipped", esink->sent_buffer_cnt,skip_count_i420);
      }
      g_mutex_unlock (&instance_lock);
      return GST_FLOW_OK;
    }

    if (!esink->is_buffer_allocated) {
      /* Allocate TBM buffer for non-zero copy case */
      if (!gst_evasimagesink_allocate_source_buffer (esink)) {
        GST_ERROR_OBJECT (esink, "Buffer allocation failed");
        g_mutex_unlock (&instance_lock);
        return GST_FLOW_ERROR;
      }
      esink->is_buffer_allocated = TRUE;
    }

    skip_count_i420 = 0; //for skip log in I420

    /* check whether surface is new or not */
    for (i = 0; i < TBM_SURFACE_NUM; i++) {
      if (esink->displaying_buffer[i].n_surface == esink->src_buffer_info[esink->src_buf_idx].n_surface) {
        GST_DEBUG_OBJECT (esink, "it is already saved bo %p (index num : %d)", esink->displaying_buffer[i].bo, i);
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
      for (i = 0; i < TBM_SURFACE_NUM; i++) {
        if (!esink->displaying_buffer[i].n_surface) {
          index = i;
          break;
        }
      }

      if (index != -1) {
        /* keep informations */
        esink->displaying_buffer[index].n_surface = esink->src_buffer_info[esink->src_buf_idx].n_surface;
        tbm_surface_map (esink->displaying_buffer[index].n_surface, TBM_SURF_OPTION_WRITE, &info);

        offset = 0;
        for (i = 0; i < info.num_planes; i++) {
          gst_buffer_map (buf, &buf_info, GST_MAP_READ);
          if (info.planes[i].ptr)
            memcpy (info.planes[i].ptr, buf_info.data + offset, info.planes[i].size);
          gst_buffer_unmap (buf, &buf_info);
          offset = offset + info.planes[i].size;
        }

        tbm_surface_unmap (esink->displaying_buffer[index].n_surface);
      }
    } else {
      /* because it has same bo, use existing native buffer */
      if (index != -1) {
        tbm_surface_map (esink->displaying_buffer[index].n_surface, TBM_SURF_OPTION_WRITE, &info);

        offset = 0;
        for (i = 0; i < info.num_planes; i++) {
          gst_buffer_map (buf, &buf_info, GST_MAP_READ);
          if (info.planes[i].ptr)
            memcpy (info.planes[i].ptr, buf_info.data + offset, info.planes[i].size);
          gst_buffer_unmap (buf, &buf_info);
          offset = offset + info.planes[i].size;
        }

        tbm_surface_unmap (esink->displaying_buffer[index].n_surface);
        GST_DEBUG_OBJECT (esink, "existing tbm surface %p",  esink->displaying_buffer[index].n_surface);

        exist_bo = FALSE;
      }
    }

    /* if it couldn't find proper index */
    if (index == -1)
      GST_WARNING_OBJECT (esink, "all spaces are being used!!!");
    else
      GST_DEBUG_OBJECT (esink, "selected buffer index %d", index);
  } else {
    GST_ERROR_OBJECT (esink, "unsupported color format");
    g_mutex_unlock (&instance_lock);
    return GST_FLOW_ERROR;
  }

  if (esink->object_show && index != -1) {
    int old_curidx = esink->cur_index;
    static int skip_count = 0;
    g_mutex_lock (&(esink->display_buffer_lock));

    if (esink->tbm_surface_format == TBM_FORMAT_NV12 || esink->tbm_surface_format == TBM_FORMAT_BGRA8888) {
      if (esink->sent_buffer_cnt < MAX_ECOREPIPE_BUFFER_CNT) {
        GST_LOG_OBJECT (esink, "[show_frame] before refcount : %d .. gst_buf : %p", gst_evasimagesink_ref_count (buf), buf);
        gst_buffer_ref (buf);
        esink->displaying_buffer[index].ref_count++;
        esink->cur_index = index;
        GST_LOG_OBJECT (esink, "index %d set refcount increase as %d", index,esink->displaying_buffer[index].ref_count);
        GST_LOG_OBJECT (esink, "[show_frame] after refcount : %d .. gst_buf : %p", gst_evasimagesink_ref_count (buf), buf);

        /* Print debug log for 8 frame */
        if (esink->debuglog_cnt_showFrame > 0) {
          GST_WARNING_OBJECT (esink, "(%d) ecore_pipe_write index[%d]  gst_buf : %p", DEBUGLOG_DEFAULT_COUNT - (esink->debuglog_cnt_showFrame),
            esink->cur_index, esink->displaying_buffer[esink->cur_index].buffer);
          esink->debuglog_cnt_showFrame--;
        }

        esink->sent_buffer_cnt++;
        skip_count = 0;

        r = ecore_pipe_write (esink->epipe, &esink->cur_index , SIZE_FOR_NATIVE_INDEX);

        if (r == EINA_FALSE)  {
          GST_LOG_OBJECT (esink, "[show_frame] before refcount : %d .. gst_buf : %p", gst_evasimagesink_ref_count (buf), buf);
          esink->cur_index = old_curidx;
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
    } else if (esink->tbm_surface_format == TBM_FORMAT_YUV420) {
      esink->cur_index = index;
      GST_LOG_OBJECT (esink, "index %d", index);
      GST_LOG_OBJECT (esink, "[show_frame] tbm surface %p", esink->src_buffer_info[esink->src_buf_idx].n_surface);

      /* Print debug log for 8 frame */
      if (esink->debuglog_cnt_showFrame > 0) {
        GST_WARNING_OBJECT (esink, "(%d) ecore_pipe_write : index[%d], n_surface[%p], gst_buf[%p]", DEBUGLOG_DEFAULT_COUNT - (esink->debuglog_cnt_showFrame),
          esink->cur_index, esink->displaying_buffer[index].n_surface, buf);
        esink->debuglog_cnt_showFrame--;
      }

      esink->sent_buffer_cnt++;

      esink->src_buf_idx++;
      r = ecore_pipe_write (esink->epipe, &esink->cur_index , SIZE_FOR_NATIVE_INDEX);

      if (r == EINA_FALSE) {
        esink->cur_index = old_curidx;
        esink->sent_buffer_cnt--;
        esink->src_buf_idx--;
        GST_ERROR_OBJECT (esink, "ecore_pipe_write is failed. index[%d], n_surface[%p]",
          index, esink->displaying_buffer[index].n_surface);
      }
      esink->src_buf_idx = esink->src_buf_idx % SOURCE_BUFFER_NUM;
    }

    g_mutex_unlock(&(esink->display_buffer_lock));
  } else {
    GST_WARNING_OBJECT (esink, "skip ecore_pipe_write(). because of esink->object_show(%d) index(%d)", esink->object_show, index);
  }
  g_mutex_unlock (&instance_lock);
  GST_DEBUG_OBJECT (esink, "Leave");
  return GST_FLOW_OK;
}

static gboolean
gst_evasimagesink_event (GstBaseSink * sink, GstEvent * event)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = GST_EVASIMAGESINK (sink);

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
gst_evasimagesink_navigation_init (GstNavigationInterface * iface)
{
  FUNCTION_ENTER ();

}

static void
gst_evasimagesink_colorbalance_init (GstColorBalanceInterface * iface)
{
  FUNCTION_ENTER ();

}

static void
gst_evasimagesink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = GST_EVASIMAGESINK (object);
  Evas_Object *eo;

  g_mutex_lock (&instance_lock);

  GST_LOG_OBJECT (esink, "property index is %d", prop_id);

  switch (prop_id) {
    case PROP_EVAS_OBJECT:
      eo = g_value_get_pointer (value);
      if (gst_evasimagesink_is_evas_image_object (eo)) {
        if (eo != esink->eo) {
          Eina_Bool r;
          EVASIMAGESINK_UNSET_EVAS_EVENT_CALLBACK (evas_object_evas_get (esink->eo));
          /* delete evas object callbacks registrated on a previous evas image object */
          EVASIMAGESINK_UNSET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo);
          esink->eo = eo;
          /* add evas object callbacks on a new evas image object */
          EVASIMAGESINK_SET_EVAS_OBJECT_EVENT_CALLBACK (esink->eo, esink);

          GST_WARNING_OBJECT (esink, "register render callback [esink : %p, esink->eo : %p]", esink, esink->eo);
          EVASIMAGESINK_SET_EVAS_EVENT_CALLBACK (evas_object_evas_get(esink->eo), esink);
          evas_object_geometry_get (esink->eo, &esink->eo_size.x, &esink->eo_size.y, &esink->eo_size.w, &esink->eo_size.h);
          GST_WARNING_OBJECT (esink, "evas object size (x:%d, y:%d, w:%d, h:%d)", esink->eo_size.x, esink->eo_size.y,
            esink->eo_size.w, esink->eo_size.h);

          esink->is_evas_object_size_set = FALSE;
          esink->object_show = TRUE;
          esink->update_visibility = UPDATE_TRUE;
          if (esink->epipe) {
            r = ecore_pipe_write (esink->epipe, &esink->update_visibility, SIZE_FOR_UPDATE_VISIBILITY);
            if (r == EINA_FALSE) {
              GST_WARNING_OBJECT (esink, "Failed to ecore_pipe_write() for updating visibility");
            }
          }
        }
      } else {
        GST_ERROR_OBJECT (esink, "Cannot set evas-object property: value is not an evas image object");
      }
      break;

    case PROP_EVAS_OBJECT_SHOW:
    {
      Eina_Bool r;
      if (esink->object_show != g_value_get_boolean (value)) {
        esink->object_show = g_value_get_boolean (value);
        if (!gst_evasimagesink_is_evas_image_object (esink->eo) ) {
          GST_WARNING_OBJECT (esink, "Cannot apply visible(show-object) property: cannot get an evas object");
          break;
        }
        esink->update_visibility = UPDATE_TRUE;
        GST_WARNING_OBJECT (esink, "esink->update_visibility = %d, esink->object_show = %d",
          esink->update_visibility, esink->object_show);

        if (esink->epipe) {
          r = ecore_pipe_write (esink->epipe, &esink->update_visibility, SIZE_FOR_UPDATE_VISIBILITY);
          if (r == EINA_FALSE)  {
            GST_WARNING_OBJECT (esink, "Failed to ecore_pipe_write() for updating visibility)\n");
          }
        }
        if (GST_STATE (esink) == GST_STATE_PAUSED && esink->object_show) {
          gboolean enable_last_buffer = FALSE;
          g_object_get (G_OBJECT (esink), "enable-last-sample", &enable_last_buffer, NULL);
          if (enable_last_buffer) {
            GstSample *last_sample = NULL;
            GstBuffer *last_buffer = NULL;
            g_object_get (G_OBJECT (esink), "last-sample", &last_sample, NULL);
            if (last_sample) {
              last_buffer = gst_sample_get_buffer (last_sample);
              GST_WARNING_OBJECT (esink, "PAUSED state: visible is updated. last buffer %p", last_buffer);
              if (last_buffer) {
                gst_buffer_ref (last_buffer);
                g_mutex_unlock (&instance_lock);
                gst_evasimagesink_show_frame ((GstVideoSink *)esink, last_buffer);
                g_mutex_lock (&instance_lock);
                gst_buffer_unref (last_buffer);
                last_buffer = NULL;
              }
              gst_sample_unref (last_sample);
              last_sample = NULL;
            }
          }
        }
      } else {
        GST_WARNING_OBJECT (esink, "visible property is same with previous value, so skip");
      }
      break;
    }

    case PROP_ROTATE_ANGLE:
    {
      int rotate = 0;
      rotate = g_value_get_int (value);
      switch (rotate) {
        case DEGREE_0:
          esink->rotate_angle = 0;
          break;
        case DEGREE_90:
          esink->rotate_angle = 90;
          break;
        case DEGREE_180:
          esink->rotate_angle = 180;
          break;
        case DEGREE_270:
          esink->rotate_angle = 270;
          break;
        default:
          break;
      }
      GST_INFO_OBJECT (esink, "update rotate_angle : %d", esink->rotate_angle);
      break;
    }

    case PROP_DISPLAY_GEOMETRY_METHOD:
    {
      Eina_Bool r;
      guint geometry = g_value_get_int (value);
      if (esink->display_geometry_method != geometry) {
        esink->display_geometry_method = geometry;
        GST_INFO_OBJECT (esink, "Overlay geometry method update, display_geometry_method(%d)", esink->display_geometry_method);
      }

      g_mutex_lock (&(esink->display_buffer_lock));

      if (esink->cur_index != -1 && esink->epipe) {
        GST_WARNING_OBJECT (esink, "apply property esink->cur_index = %d", esink->cur_index);
        esink->displaying_buffer[esink->cur_index].ref_count++;
        gst_buffer_ref (esink->displaying_buffer[esink->cur_index].buffer);
        esink->sent_buffer_cnt++;
        r = ecore_pipe_write (esink->epipe, &esink->cur_index, SIZE_FOR_NATIVE_INDEX);

        if (r == EINA_FALSE) {
          GST_WARNING_OBJECT (esink, "ecore_pipe_write fail");
          esink->displaying_buffer[esink->cur_index].ref_count--;
          gst_buffer_unref (esink->displaying_buffer[esink->cur_index].buffer);
          esink->sent_buffer_cnt--;
        }
      }
      g_mutex_unlock (&(esink->display_buffer_lock));
      break;
    }

    case PROP_ENABLE_FLUSH_BUFFER:
      esink->enable_flush_buffer = g_value_get_boolean (value);
      GST_INFO_OBJECT (esink, "flush buffer value : %d", esink->enable_flush_buffer);
      break;

    case PROP_FLIP:
      esink->flip = g_value_get_int (value);
      GST_INFO_OBJECT (esink, "update flip : %d", esink->flip);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&instance_lock);
}

static void
gst_evasimagesink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  FUNCTION_ENTER ();

  GstEvasImageSink *esink = GST_EVASIMAGESINK (object);

  switch (prop_id) {
    case PROP_EVAS_OBJECT:
      g_value_set_pointer (value, esink->eo);
      break;
    case PROP_EVAS_OBJECT_SHOW:
      g_value_set_boolean (value, esink->object_show);
      break;
    case PROP_ROTATE_ANGLE:
      g_value_set_int (value, esink->rotate_angle);
      break;
    case PROP_DISPLAY_GEOMETRY_METHOD:
      g_value_set_int (value, esink->display_geometry_method);
      break;
    case PROP_ENABLE_FLUSH_BUFFER:
      g_value_set_boolean (value, esink->enable_flush_buffer);
      break;
    case PROP_FLIP:
      g_value_set_int (value, esink->flip);
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
gst_evasimagesink_finalize (GObject * object)
{
  FUNCTION_ENTER ();

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_evasimagesink_init (GstEvasImageSink * esink)
{
  FUNCTION_ENTER ();
  gint i = 0;

  esink->eo = NULL;
  esink->epipe = NULL;
  esink->object_show = FALSE;
  esink->update_visibility = UPDATE_FALSE;
  esink->is_evas_object_size_set = FALSE;

  g_mutex_init (&(esink->display_buffer_lock));
  g_mutex_init (&(esink->flow_lock));

  for (i = 0; i < TBM_SURFACE_NUM; i++) {
    esink->displaying_buffer[i].n_surface = NULL;
    esink->displaying_buffer[i].buffer = NULL;
    esink->displaying_buffer[i].bo = NULL;
    esink->displaying_buffer[i].ref_count = 0;
  }

  esink->prev_buf = NULL;
  esink->prev_index = -1;
  esink->cur_index = -1;
  esink->enable_flush_buffer = TRUE;
  esink->flush_buffer = NULL;
  esink->need_flush = FALSE;
  esink->display_geometry_method = DISP_GEO_METHOD_LETTER_BOX;
  esink->flip = FLIP_NONE;
  esink->eo_size.x = esink->eo_size.y = 0;
  esink->eo_size.w = esink->eo_size.h = 0;
  esink->use_ratio = FALSE;
  esink->sent_buffer_cnt = 0;
  esink->w = esink->h = 0;
  esink->debuglog_cnt_showFrame = DEBUGLOG_DEFAULT_COUNT;
  esink->debuglog_cnt_ecoreCbPipe = DEBUGLOG_DEFAULT_COUNT;

  esink->drm_fd = -1;
  esink->bufmgr = NULL;

  esink->fps_n = 0;
  esink->fps_d = 0;

  esink->src_buf_idx = 0;
  esink->is_buffer_allocated = FALSE;

  if (instance_lock_count == 0)
    g_mutex_init(&instance_lock);

  g_mutex_lock (&instance_lock);
  instance_lock_count++;
  g_mutex_unlock (&instance_lock);

  g_object_weak_ref (G_OBJECT (esink), gst_evasimagesink_fini, NULL);
}

static void
gst_evasimagesink_class_init (GstEvasImageSinkClass * klass)
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

  gobject_class->set_property = gst_evasimagesink_set_property;
  gobject_class->get_property = gst_evasimagesink_get_property;

  /**
   * GstEvasImageSink:evas-object
   *
   * Evas image object for rendering
   */
  g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT,
    g_param_spec_pointer ("evas-object", "Destination Evas Object",
        "Destination evas image object",
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_EVAS_OBJECT_SHOW,
    g_param_spec_boolean ("visible", "Show Evas Object",
      "When disabled, evas object does not show", TRUE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_ROTATE_ANGLE,
    g_param_spec_int("rotate", "Rotate angle",
      "Rotate angle of display output", DEGREE_0, DEGREE_NUM,
      DEGREE_0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_DISPLAY_GEOMETRY_METHOD,
    g_param_spec_int("display-geometry-method", "Display geometry method",
      "Geometrical method for display", DISP_GEO_METHOD_LETTER_BOX,
      DISP_GEO_METHOD_NUM, DISP_GEO_METHOD_LETTER_BOX,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENABLE_FLUSH_BUFFER,
    g_param_spec_boolean("enable-flush-buffer", "Enable flush buffer mechanism",
      "Enable flush buffer mechanism when state change(PAUSED_TO_READY)",
      TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(gobject_class, PROP_FLIP,
    g_param_spec_int("flip", "Display flip",
      "Flip for display", FLIP_NONE, FLIP_NUM, FLIP_NONE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_evasimagesink_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_evasimagesink_change_state);
  gst_element_class_set_details_simple (gstelement_class, "EvasImageSink",
      "Sink/Video", "evas image object videosink based on Xv extension",
      "Sangchul Lee <sc11.lee@samsung.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_evasimagesink_sink_template_factory));
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_evasimagesink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_evasimagesink_setcaps);
  gstbasesink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_evasimagesink_get_times);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_evasimagesink_event);
  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_evasimagesink_show_frame);
}

/* Object typing & Creation */

static gboolean
plugin_init (GstPlugin * plugin)
{
  FUNCTION_ENTER ();

  if (!gst_element_register (plugin, "evasimagesink", GST_RANK_NONE,
          GST_TYPE_EVASIMAGESINK)) {
    return FALSE;
  }
  GST_DEBUG_CATEGORY_INIT (gst_debug_evasimagesink, "evasimagesink", 0,
      "evasimagesink element");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    evasimagesink, "Evas image object render plugin using Xv extension",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
