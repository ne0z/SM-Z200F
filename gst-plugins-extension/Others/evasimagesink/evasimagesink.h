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

#ifndef __GST_EVASIMAGESINK_H__
#define __GST_EVASIMAGESINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>

#include <Evas.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <mm_types.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

G_BEGIN_DECLS
#define GST_TYPE_EVASIMAGESINK \
  (gst_evasimagesink_get_type())
#define GST_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_EVASIMAGESINK, GstEvasImageSink))
#define GST_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_EVASIMAGESINK, GstEvasImageSinkClass))
#define GST_IS_EVASIMAGESINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_EVASIMAGESINK))
#define GST_IS_EVASIMAGESINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_EVASIMAGESINK))

typedef struct _GstEvasImageSink GstEvasImageSink;
typedef struct _GstEvasImageSinkClass GstEvasImageSinkClass;

#define TBM_SURFACE_NUM 30
#define SOURCE_BUFFER_NUM 8
typedef struct _GstEvasImageTbmDisplayingBuffer GstEvasImageTbmDisplayingBuffer;
typedef struct _GstEvasImageTbmFlushBuffer GstEvasImageTbmFlushBuffer;
typedef enum {
  BUF_SHARE_METHOD_NONE = -1,
  BUF_SHARE_METHOD_PADDR = 0,
  BUF_SHARE_METHOD_FD,
  BUF_SHARE_METHOD_TIZEN_BUFFER,
  BUF_SHARE_METHOD_FLUSH_BUFFER
} buf_share_method_t;

enum {
  DEGREE_0 = 0,
  DEGREE_90,
  DEGREE_180,
  DEGREE_270,
  DEGREE_NUM,
};

enum {
  DISP_GEO_METHOD_LETTER_BOX = 0,
  DISP_GEO_METHOD_ORIGIN_SIZE,
  DISP_GEO_METHOD_FULL_SCREEN,
  DISP_GEO_METHOD_CROPPED_FULL_SCREEN,
  DISP_GEO_METHOD_ORIGIN_SIZE_OR_LETTER_BOX,
  DISP_GEO_METHOD_CUSTOM_DST_ROI,
  DISP_GEO_METHOD_NUM,
};

enum {
  FLIP_NONE = 0,
  FLIP_HORIZONTAL,
  FLIP_VERTICAL,
  FLIP_BOTH,
  FLIP_NUM
};

struct tbm_buffer_info {
  tbm_surface_h n_surface;
};

/* _GstEvasImageTbmDisplayingBuffer
 *
 * buffer   : manage ref count by got index through comparison
 * bo     : compare with buffer from codec for getting index
 * n_buffer   : compare with buffer from evas for getting index
 * ref_count  : decide whether it unref buffer or not in gst_evas_image_sink_fini/reset
 */
struct _GstEvasImageTbmDisplayingBuffer {
  GstBuffer *buffer;
  void *bo;
  tbm_surface_h n_surface;
  int ref_count;
};

struct _GstEvasImageTbmFlushBuffer {
  tbm_surface_h n_surface;
  tbm_bo flush_buffer_bo[TBM_SURFACE_NUM];
  int bo_num;
};

/**
 * GstEvasImageSink:
 * @display_name: the name of the Display we want to render to
 * @xcontext: our instance's #GstXContext
 * @xpixmap: the #GstXPixmap we are rendering to
 * @fps_n: the framerate fraction numerator
 * @fps_d: the framerate fraction denominator
 * @x_lock: used to protect X calls as we are not using the XLib in threaded
 * mode
 * @flow_lock: used to protect data flow routines from external calls such as
 * methods from the #GstXOverlay interface
 * @par: used to override calculated pixel aspect ratio from @xcontext
 * @synchronous: used to store if XSynchronous should be used or not (for
 * debugging purpose only)
 * @keep_aspect: used to remember if reverse negotiation scaling should respect
 * aspect ratio
 * @brightness: used to store the user settings for color balance brightness
 * @contrast: used to store the user settings for color balance contrast
 * @hue: used to store the user settings for color balance hue
 * @saturation: used to store the user settings for color balance saturation
 * @cb_changed: used to store if the color balance settings where changed
 * @video_width: the width of incoming video frames in pixels
 * @video_height: the height of incoming video frames in pixels
 *
 * The #GstEvasImageSink data structure.
 */
struct _GstEvasImageSink
{
  GstVideoSink element;

  Evas_Object *eo;
  Ecore_Pipe *epipe;
  Evas_Coord w;
  Evas_Coord h;
  gboolean object_show;
  gchar update_visibility;
  gboolean gl_zerocopy;

  GstBuffer *oldbuf;

  /* Framerate numerator and denominator */
  gint fps_n;
  gint fps_d;

  gboolean is_evas_object_size_set;
  guint present_data_addr;
  GMutex display_buffer_lock;
  GMutex flow_lock;
  GstEvasImageTbmDisplayingBuffer displaying_buffer[TBM_SURFACE_NUM];
  GstVideoRectangle eo_size;
  gboolean use_ratio;
  guint rotate_angle;
  guint display_geometry_method;
  guint flip;
  GstBuffer *prev_buf;
  gint prev_index;
  gint cur_index;
  gboolean need_flush;
  gboolean enable_flush_buffer;
  GstEvasImageTbmFlushBuffer *flush_buffer;
  gint sent_buffer_cnt;
  gint debuglog_cnt_showFrame;
  gint debuglog_cnt_ecoreCbPipe;

  /* drm fd for buffer manager*/
  gint drm_fd;
  void *bufmgr;

  tbm_format tbm_surface_format;
  struct tbm_buffer_info src_buffer_info[TBM_SURFACE_NUM];
  guint src_buf_idx;
  gboolean is_buffer_allocated;
};

struct _GstEvasImageSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_evasimagesink_get_type (void);

G_END_DECLS
#endif /* __GST_EVASIMAGESINK_H__ */
