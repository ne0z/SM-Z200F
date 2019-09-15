/*
 * gstcamerasrc.h
 *
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Abhijit Dey <abhijit.dey@samsung.com>
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

#ifndef __GST_CAMERASRC_H__
#define __GST_CAMERASRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/video/colorbalance.h>
#include <gst/video/cameracontrol.h>
#include <gst/video/video-format.h>
#include <mm_types.h>
#include "camera_snapshot.h"
#include "camera_preview.h"
#include "camera_record.h"
#include "camera_main.h"
#include "HDR/camerahdr.h"

G_BEGIN_DECLS

#define GST_TYPE_CAMERASRC (gst_camerasrc_get_type())
#define GST_CAMERASRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAMERASRC,GstCameraSrc))
#define GST_CAMERASRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAMERASRC,GstCameraSrcClass))
#define GST_IS_CAMERASRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAMERASRC))
#define GST_IS_CAMERASRC_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAMERASRC))
#define ASPECT_RATIO_4_3 1.5  //4//4:3
#define ASPECT_RATIO_16_9 1.7  //16:9
#define ASPECT_RATIO_1_1 1  //1:1
enum {
    GST_CAMERASRC_FOCUS_MODE_AUTO,
    GST_CAMERASRC_FOCUS_MODE_MANUAL,
    GST_CAMERASRC_FOCUS_MODE_PAN,
    GST_CAMERASRC_FOCUS_MODE_TOUCH_AUTO,
    GST_CAMERASRC_FOCUS_MODE_CONTINUOUS,
    GST_CAMERASRC_FOCUS_MODE_MAX
};

enum {
    GST_CAMERASRC_FOCUS_RANGE_NORMAL,
    GST_CAMERASRC_FOCUS_RANGE_MACRO,
    GST_CAMERASRC_FOCUS_RANGE_FULL,
    GST_CAMERASRC_FOCUS_RANGE_MAX
};


typedef struct _GstCameraSrc      GstCameraSrc;
typedef struct _GstCameraSrcClass GstCameraSrcClass;
typedef struct _GstCameraBuffer GstCameraBuffer;

typedef struct _camerasrc_thread_handle_t {
    GThread *thread;            /**< thread for snapshot */
    GMutex thread_lock;         /**< mutex for snapshot thread */
    GCond thread_cond;          /**< cond for snapshot thread */
    gboolean thread_exit;       /**< thread exit flag */
}camerasrc_thread_handle_t;

struct _GstCameraBuffer {
    GstBuffer *buffer;
    GstCameraSrc *camerasrc;
    void* buffer_metadata;
    int buffer_type;
    int buffer_index;
};

struct _GstCameraSrc
{
    GstPushSrc  element;

    //camera property
    gboolean ready_state;
    int     width;
    int     height;
    int     fps;
    const gchar *format_str;
    int     pix_format;
    int     colorspace;
    int     rotate;
    gboolean    use_rotate_caps;

    //Element properties
    gint    high_speed_fps;
    gboolean    fps_auto;
    gint    camera_id;
    const gchar *cap_format_str;
    guint32     cap_fourcc;
    camerasrc_capture_quality     cap_quality;
    gint    prev_cap_width;
    gint    cap_width;
    gint    cap_height;
    gint    cap_interval;
    gint    cap_count;
    gint    cap_jpg_quality;
    camerasrc_hdr_mode      cap_hdr;
    gint    video_width;
    gint    video_height;
    gboolean    cap_provide_exif;
    gboolean    enable_zsl_mode;
    gboolean    enable_hybrid_mode;
    gboolean    recording_hint;
    gboolean    signal_still_capture;
    int     sensor_orientation;
    gint    low_light_detection;
    gint    low_light_condition;
    gint    face_detect;
    gboolean    face_detected;
    gboolean    vflip;
    gboolean    hflip;
#ifdef DUAL_CAMERA
    gboolean    dual_camera_mode;
#endif
    int     vt_mode;
    int     shot_mode;
    //Internal variable
    void*   camera_handle;
    gint    mode;
    gint    ion_fd;
    gboolean firsttime;
    gboolean firsttime_record;
    GQueue* command_list;
    GMutex commandq_mutex;
    GCond commandq_cond;
    GQueue* cmd_list;
    GMutex* cmd_lock;
    GCond* cmd_cond;
    gint num_live_buffers;
    gint qbuf_without_display;
    gint num_video_buffers;
    GCond *preview_buffer_cond;                   /**< condition for buffer control */
    GMutex *preview_buffer_lock;                  /**< lock for buffer control */
    GCond *video_buffer_cond;                     /**< condition for buffer control */
    GMutex *video_buffer_lock;                    /**< lock for buffer control */
    int focus_mode;                               /**< focus mode */
    int focus_range;                              /**< focus range */
    int is_focusing;                              /**< focus status */
    camera_preview_hw_intf* preview_stream;
    camera_snapshot_hw_intf* snapshot_stream;
    camera_record_hw_intf* record_stream;
    gboolean is_start_failed;

    // Colorbalance , CameraControl interface
    GList*  colors;
    GList*  camera_controls;

    unsigned char *snapshot_buffer;		/**< dualcamera, snapshot buffer*/
    int snapshot_buffer_size;			/**< dualcamera, snapshot buffersize*/
#ifdef VDIS
    gboolean enable_vdis_mode;
#endif
    camerasrc_batch_ctrl_t* batch_control_value_cache;
    int64_t batch_control_id_cache;
    gboolean setting_value_cached;
    gboolean camera_cpu_clock_lock;

    GCond *restart_camera_cond;                      /**< condition for restart camera */
    GMutex *restart_camera_lock;                     /**< lock for restart camera */

    GCond *capture_buffer_cond; 					 /**< condition for capture_buffer */
    GMutex *capture_buffer_lock;					 /**< lock for capture_buffer */

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    camerasrc_thread_handle_t *raw_capture_done_handle;
#endif
    camerasrc_thread_handle_t *snapshot_handle;      /**< snapshot handle */

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    camerasrc_thread_handle_t *preflash_thread_handle;      /**< preflash_thread_handle */
#endif
    gboolean received_capture_command;
    MMVideoBuffer *last_buffer_cache;
    gint frame_count;
    gint finalizeLogCnt;
};

struct _GstCameraSrcClass
{
    GstPushSrcClass parent_class;

    void    (*still_capture)    (GstElement *element, GstBuffer *main, GstBuffer *sub);
    void    (*video_stream_cb)    (GstElement *element, GstBuffer *main);
    void    (*nego_complete)    (GstElement *element);
    void    (*register_trouble)    (GstElement *element);
};

int gst_camerasrc_set_camera_control(GstCameraSrc* camerasrc,int64_t control_type,int value_1,int value_2,char* string_1);
void gst_camerasrc_set_capture_command( GstCameraSrc* camerasrc, GstCameraControlCaptureCommand cmd );
void gst_camerasrc_set_record_command( GstCameraSrc* camerasrc, GstCameraControlRecordCommand cmd );
void gst_camerasrc_post_message_int(GstCameraSrc *camerasrc, const char *msg_name, const char *field_name, int value);
unsigned long gst_get_current_time(void);
GType gst_camerasrc_get_type (void);

G_END_DECLS

#endif /* __GST_CAMERASRC_H__ */
