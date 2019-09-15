/*
 * EcoreBufferSink
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
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

#ifndef __GST_ECOREBUFFERSINK_H__
#define __GST_ECOREBUFFERSINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Buffer.h>
#include <Ecore_Buffer_Queue.h>
#include <mm_types.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

G_BEGIN_DECLS
#define GST_TYPE_ECOREBUFFERSINK \
  (gst_ecorebuffersink_get_type())
#define GST_ECOREBUFFERSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ECOREBUFFERSINK, GstEcoreBufferSink))
#define GST_ECOREBUFFERSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ECOREBUFFERSINK, GstEcoreBufferSinkClass))
#define GST_IS_ECOREBUFFERSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ECOREBUFFERSINK))
#define GST_IS_ECOREBUFFERSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ECOREBUFFERSINK))

typedef struct _GstEcoreBufferSink GstEcoreBufferSink;
typedef struct _GstEcoreBufferSinkClass GstEcoreBufferSinkClass;

#define ECOREBUFFER_POOL_NUM 30
typedef struct _GstEcoreBufferTbmDisplayingBuffer GstEcoreBufferTbmDisplayingBuffer;

struct _GstEcoreBufferTbmDisplayingBuffer {
  GstBuffer *buffer;
  void *bo;
  tbm_surface_h n_surface;
  int ref_count;
  Ecore_Buffer *ecore_buffer;
};

struct _GstEcoreBufferSink
{
  GstVideoSink element;
  Ecore_Buffer_Provider *provider;
  Ecore_Pipe *epipe;
  int w;
  int h;

  gint fps_n;
  gint fps_d;

  GMutex display_buffer_lock;
  GMutex flow_lock;
  GMutex instance_lock;
  guint instance_lock_count;

  GstEcoreBufferTbmDisplayingBuffer displaying_buffer[ECOREBUFFER_POOL_NUM];

  gboolean visible;
  gboolean buffer_init;
  gboolean queue_init;
  gchar *ecorebuffer_queue_name;

  gint sent_buffer_cnt;
  gint debuglog_cnt_showFrame;
  gint debuglog_cnt_ecoreCbPipe;
  gint rotate_flag;

  tbm_format tbm_surface_format;
  gboolean is_consumer_connected;
};

struct _GstEcoreBufferSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_ecorebuffersink_get_type (void);

G_END_DECLS
#endif /* __GST_ECOREBUFFERSINK_H__ */
