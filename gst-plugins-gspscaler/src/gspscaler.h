/*
 * GSPScaler
 *
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Abhinay Ganapavarapu <abhinay.g@samsung.com>
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


#ifndef __GST_GSP_SCALER_H_
#define __GST_GSP_SCALER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <tbm_bufmgr.h>
#include <drm/sprd_drm.h>
#include <drm/drm_fourcc.h>
#include <mm_types.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (gspscaler_debug);
#define GST_CAT_DEFAULT gspscaler_debug

#define GST_TYPE_GSPSCALER (gst_gspscaler_get_type())
#define GST_GSPSCALER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GSPSCALER,GstGSPScaler))
#define GST_GSPSCALER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GSPSCALER,GstGSPScalerClass))
#define GST_IS_GSPSCALER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GSPSCALER))
#define GST_IS_GSPSCALER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GSPSCALER))

typedef struct _GstGSPScaler GstGSPScaler;
typedef struct _GstGSPScalerClass GstGSPScalerClass;
typedef struct _GstGSPScalerBuffer GstGSPScalerBuffer;
typedef struct _GstGSPScalerBufferInfo GstGSPScalerBufferInfo;

#define GSPSCALER_DST_WIDTH_DEFAULT 0
#define GSPSCALER_DST_WIDTH_MIN 0
#define GSPSCALER_DST_WIDTH_MAX 32767
#define GSPSCALER_DST_HEIGHT_DEFAULT 0
#define GSPSCALER_DST_HEIGHT_MIN 0
#define GSPSCALER_DST_HEIGHT_MAX 32767
#define GSPSCALER_DST_BUFFER_MIN 2
#define GSPSCALER_DST_BUFFER_DEFAULT 3
#define GSPSCALER_SRC_BUFFER_MAX 6
#define GSPSCALER_DST_BUFFER_MAX 6

enum{
  GSPSCALER_DST_ROTATION_0,
  GSPSCALER_DST_ROTATION_90,
  GSPSCALER_DST_ROTATION_180,
  GSPSCALER_DST_ROTATION_270
};
#define GSPSCALER_DST_ROTATION_MIN GSPSCALER_DST_ROTATION_0
#define GSPSCALER_DST_ROTATION_MAX GSPSCALER_DST_ROTATION_270
#define GSPSCALER_DST_ROTATION_DEFAULT GSPSCALER_DST_ROTATION_0

enum{
  GSPSCALER_DST_FLIP_NONE,
  GSPSCALER_DST_FLIP_HORZ,
  GSPSCALER_DST_FLIP_VERT,
  GSPSCALER_DST_FLIP_BOTH
};

enum{
  GSPSCALER_DST_BUFFER_FREE,
  GSPSCALER_DST_BUFFER_INUSE
};

#define GSPSCALER_DST_FLIP_MIN GSPSCALER_DST_FLIP_NONE
#define GSPSCALER_DST_FLIP_MAX GSPSCALER_DST_FLIP_BOTH
#define GSPSCALER_DST_FLIP_DEFAULT GSPSCALER_DST_FLIP_NONE

#define MAX_SRC_BUF_NUM GSPSCALER_SRC_BUFFER_MAX
#define MAX_DST_BUF_NUM GSPSCALER_DST_BUFFER_MAX
#define BUFFER_AVAILABLE_MAX_WAIT_TIME 1000*1000 // 1sec

struct _GstGSPScalerBuffer {
  GstBuffer *buffer;
  GstGSPScaler *gspscaler;
  guint actual_size;
  int dst_buffer_index;
};

struct _GstGSPScalerBufferInfo {
  int ion_fd;
  void* usr_addr;
  uint64_t size;
  tbm_bo bo;
  int phy_addr;
  int phy_size;
  unsigned int gem_handle[3];
};

#define ALIGN(x, a)       (((x) + (a) - 1) & ~((a) - 1))


struct _GstGSPScaler {
  GstBaseTransform element;
  GstPad* src_pad;

  GMutex buf_idx_lock;
  GCond  buf_idx_cond;

  gboolean is_src_format_std;
  gboolean is_dst_format_std;

  const gchar *src_format_gst;
  const gchar *dst_format_gst;
  int src_format_drm;
  int dst_format_drm;

  gboolean is_same_caps;

  struct drm_sprd_ipp_config src_config;
  struct drm_sprd_ipp_config dst_config;

  struct drm_sprd_ipp_queue_buf src_qbuf;
  struct drm_sprd_ipp_queue_buf dst_qbuf;

  struct drm_sprd_ipp_cmd_ctrl ctrl;

  guint dst_buf_size;
  guint src_buf_idx;
  guint dst_buf_idx;

  gint src_width;
  gint src_height;
  gint src_x;
  gint src_y;

  gint dst_width;
  gint dst_height;
  gint dst_x;
  gint dst_y;

  gint rotation;
  gint flip;

  int32_t src_drmhandle;
  int32_t dest_drmhandle;

  int32_t drm_fd;
  unsigned int ion_fd;

  gboolean is_drm_inited;
  int op_buf_share_method;

  guint num_of_src_buffers;
  guint num_of_dst_buffers;

  GstGSPScalerBufferInfo src_buffer_info[MAX_DST_BUF_NUM];
  GstGSPScalerBufferInfo dst_buffer_info[MAX_DST_BUF_NUM];
  int dst_buffer_status[MAX_DST_BUF_NUM];
  tbm_bufmgr tbm;

  gboolean play_success;

  gboolean is_playing;
  gboolean is_property_set;

  int prop_id;

  MMVideoBuffer* video_out_buf;
};

struct _GstGSPScalerClass {
  GstBaseTransformClass parent_class;
};

GType gst_gspscaler_get_type(void);

G_END_DECLS

#endif // __GST_GSP_SCALER_H_
