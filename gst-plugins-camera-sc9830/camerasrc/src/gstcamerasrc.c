 /*
 * gstcamerasrc.c
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/gstutils.h>
#include <glib-object.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h> // sched_yield()
#include "gstcamerasrc.h"
#include "gstcamerasrccontrol.h"
#include "gstcamerasrccolorbalance.h"

#include <mm_util_jpeg.h>


GST_DEBUG_CATEGORY_STATIC (gst_camera_src_debug);
#define GST_CAT_DEFAULT gst_camera_src_debug
#if ENABLE_TIMING_DATA
static GTimer *capture_timer = NULL;
#endif

#define _PREVIEW_BUFFER_WAIT_TIMEOUT            1000000 /* usec */
#define _VIDEO_BUFFER_WAIT_TIMEOUT              100000  /* usec */
#define DEFAULT_JPEG_ENCODE_QUALITY             85
#define ENV_NAME_USE_HW_CODEC           "IMAGE_UTIL_USE_HW_CODEC"
#define MAX_FACE_NUM 10
#define DEBUG_CAMERALOG_DEFAULT_COUNT 8

#define SAFE_FREE_GMUTEX(gmutex) \
	if (gmutex) { \
		g_mutex_free(gmutex); \
		gmutex = NULL; \
	}

#define SAFE_FREE_GCOND(gcond) \
	if (gcond) { \
		g_cond_free(gcond); \
		gcond = NULL; \
	}

#define SAFE_FREE_GQUEUE(gqueue) \
	if (gqueue) { \
		g_queue_free(gqueue); \
		gqueue = NULL; \
	}

#define _DEFAULT_NUM_OF_CPU_CORES               4
#define _DEFAULT_HDR_METHOD                     SS_MODE_HDRTMO
#define _DEFAULT_HDR_FORMAT                     SS_FORMAT_NV16


enum {
    SIGNAL_STILL_CAPTURE,
    SIGNAL_VIDEO_STREAM_CB,
    SIGNAL_NEGO_COMPLETE,
    LAST_SIGNAL
};

enum {
    GST_CAMERASRC_CMD_NONE,
    GST_CAMERASRC_CMD_PREVIEW_RESTART,
    GST_CAMERASRC_CMD_RECORD_START,
    GST_CAMERASRC_CMD_RECORD_STOP,
    GST_CAMERASRC_CMD_CAPTURE_START,
    GST_CAMERASRC_CMD_CAPTURE_STOP
};

enum {
    PROP_0,
    PROP_CAMERA_HIGH_SPEED_FPS,
    PROP_CAMERA_AUTO_FPS,
    PROP_CAMERA_ID,
    PROP_CAMERA_CAPTURE_FOURCC,
    PROP_CAMERA_CAPTURE_QUALITY,
    PROP_CAMERA_CAPTURE_WIDTH,
    PROP_CAMERA_CAPTURE_HEIGHT,
    PROP_CAMERA_CAPTURE_INTERVAL,
    PROP_CAMERA_CAPTURE_COUNT,
    PROP_CAMERA_CAPTURE_JPG_QUALITY,
    PROP_CAMERA_CAPTURE_PROVIDE_EXIF,
    PROP_CAMERA_SHOT_MODE,
    PROP_CAMERA_CAPTURE_HDR,
    PROP_CAMERA_VIDEO_WIDTH,
    PROP_CAMERA_VIDEO_HEIGHT,
#ifdef VDIS
    PROP_CAMERA_ENABLE_VDIS,
#endif
    PROP_CAMERA_ENABLE_HYBRID_MODE,
    PROP_CAMERA_RECORDING_HINT,
    PROP_CAMERA_LOW_LIGHT_AUTO_DETECTION,
    PROP_VFLIP,
    PROP_HFLIP,
    PROP_SENSOR_ORIENTATION,
#ifdef DUAL_CAMERA
    PROP_DUAL_CAMERA_MODE,
#endif
    PROP_CAMERA_CPU_LOCK_MODE,
    PROP_VT_MODE,
    PROP_NUM,
};
static void gst_camerasrc_uri_handler_init (gpointer g_iface, gpointer iface_data);


static guint gst_camerasrc_signals[LAST_SIGNAL] = { 0 };

//Element template variables
static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw,"
        "format = (string) { I420 }, "
        "width = (int) [ 1, 4096 ], "
        "height = (int) [ 1, 4096 ]; "
        "video/x-raw,"
        "format = (string) { NV12 }, "
        "width = (int) [ 1, 4096 ], "
        "height = (int) [ 1, 4096 ]; "
        "video/x-raw,"
        "format = (string) { SN12 }, "
        "width = (int) [ 1, 4096 ], "
        "height = (int) [ 1, 4096 ]; "
        "video/x-raw,"
        "format = (string) { NV21 }, "
        "width = (int) [ 1, 4096 ], "
        "height = (int) [ 1, 4096 ]; ")
    );
#define GST_TYPE_CAMERASRC_QUALITY (gst_camerasrc_quality_get_type ())
#define GST_IS_CAMERASRC_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CAMERASRC_BUFFER))
#define GST_CAMERASRC_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CAMERASRC_BUFFER, GstCameraBuffer))

/******************************************************************************
 * Function Declarations
 *******************************************************************************/

/*********************************Util functions***********************************/
static void GenerateYUV420BlackFrame(unsigned char *buf, int buf_size, int width, int height);
#if _ENABLE_CAMERASRC_DEBUG
static int __util_write_file(char *filename, void *data, int size);
static int data_dump(char *filename, void *data, int size);
#endif
static gboolean gst_camerasrc_get_timeinfo(GstCameraSrc * camerasrc, GstBuffer* buffer);
static gboolean gst_camerasrc_is_zero_copy_format(const gchar *format_str);
static gboolean gst_camerasrc_process_cmd(GstCameraSrc *camerasrc, int cmd);
static int cache_settings_for_batch_update(GstCameraSrc* camerasrc,int64_t control_type,int value_1,int value_2,char* string_1);
static int settings_batch_update_from_cache(GstCameraSrc* camerasrc);
/*********************************QCAM functions***********************************/
static gboolean
capture_snapshot(GstCameraSrc *camerasrc);
static void
preview_frame_cb(camera_memory_t *bufs,void *user_data, int bufferIndex);
static void
record_frame_cb(camera_memory_t *bufs,void *user_data,int bufferIndex);

/**********************Snapshot Stream***********************/
static void
snapshot_done_signal(GstCameraSrc *camerasrc);

/******************************Suppliment functions*********************************/
static gboolean
gst_camerasrc_get_caps_info (GstCameraSrc * camerasrc, GstCaps * caps);
static void
gst_camerasrc_error_handler(GstCameraSrc *camerasrc, int ret);
static gboolean
gst_camerasrc_create(GstCameraSrc *camerasrc);
static gboolean
gst_camerasrc_destroy(GstCameraSrc *camerasrc);
static gboolean
gst_camerasrc_fill_ctrl_list(GstCameraSrc *camerasrc);
static gboolean
gst_camerasrc_empty_ctrl_list(GstCameraSrc *camerasrc);
static gboolean
gst_camerasrc_start(GstCameraSrc *camerasrc);
static gboolean
gst_camerasrc_stop(GstCameraSrc *camerasrc);
static GstFlowReturn
gst_camerasrc_read_frame(GstCameraSrc *camerasrc,GstBuffer **buffer);

/*********************************GST functions***********************************/

static void
gst_camerasrc_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void
gst_camerasrc_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void
gst_camerasrc_finalize (GObject * object);
static GstStateChangeReturn
gst_camerasrc_change_state (GstElement *element, GstStateChange transition);
static gboolean
gst_camerasrc_src_start (GstBaseSrc * src);
static gboolean
gst_camerasrc_src_stop (GstBaseSrc * src);
static GstCaps *
gst_camerasrc_get_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean
gst_camerasrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static GstFlowReturn
gst_camerasrc_src_create (GstPushSrc * src, GstBuffer ** buffer);

/*********************************BUFFER functions***********************************/
static void gst_camerasrc_buffer_class_init(gpointer g_class, gpointer class_data);
static GstCameraBuffer* gst_camerasrc_buffer_new(GstCameraSrc *camerasrc);
static void gst_camerasrc_buffer_finalize(GstCameraBuffer *buffer);
static void gst_camerasrc_wait_all_preview_buffer_finalized(GstCameraSrc *camerasrc);
static void gst_camerasrc_wait_all_video_buffer_finalized(GstCameraSrc *camerasrc);
static void gst_camerasrc_release_all_queued_preview_buffer(GstCameraSrc *camerasrc);


/******************************************************************************
 * Function Implementations
 *******************************************************************************/


//TODO: need to check how to implement color balance functions
GST_IMPLEMENT_CAMERASRC_COLOR_BALANCE_METHODS( GstCameraSrc, gst_camera_src );
GST_IMPLEMENT_CAMERASRC_CONTROL_METHODS( GstCameraSrc, gst_camera_src );

G_DEFINE_TYPE_WITH_CODE(GstCameraSrc, gst_camerasrc, GST_TYPE_PUSH_SRC,
                        G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gst_camerasrc_uri_handler_init)
                        G_IMPLEMENT_INTERFACE(GST_TYPE_CAMERA_CONTROL, gst_camera_src_control_interface_init)
                        G_IMPLEMENT_INTERFACE(GST_TYPE_COLOR_BALANCE, gst_camera_src_color_balance_interface_init));

/*********************************Util functions***********************************/
static  void
GenerateYUV420BlackFrame (unsigned char *buf, int buf_size, int width, int height)
{
    int i;
    int y_len = 0;
    int yuv_len = 0;

    y_len = width * height;
    yuv_len = (width * height * 3) >> 1;

    if (buf_size < yuv_len) {
        return;
    }

    if (width%4){
        for (i = 0; i < y_len; i++) {
            buf[i] = 0x10;
        }

        for (; i < yuv_len ; i++) {
            buf[i] = 0x80;
        }
    }else{
        //faster way
        int *ibuf = NULL;
        short *sbuf = NULL;
        ibuf = (int*)buf;

        for (i = 0; i < y_len/4; i++){
            ibuf[i] = 0x10101010;       //set YYYY
        }
        sbuf = (short*)(&buf[y_len]);

        for (i = 0 ; i < (yuv_len - y_len) / 2 ; i++){
            sbuf[i] = 0x8080;       //set UV
        }
    }

    return;
}

unsigned long gst_get_current_time(void)
{
    struct timespec lc_time;
    clock_gettime (CLOCK_MONOTONIC_RAW, &lc_time);
    return ((unsigned long)(lc_time.tv_sec * 1000L) + (unsigned long)(lc_time.tv_nsec / 1000000L));
}

static gboolean
gst_camerasrc_get_timeinfo(GstCameraSrc * camerasrc, GstBuffer* buffer)
{
    GstClock *clock = NULL;
    GstClockTime timestamp = GST_CLOCK_TIME_NONE;
    GstClockTime duration = GST_CLOCK_TIME_NONE;
    int fps_nu = 0, fps_de = 0;

    if (!camerasrc || !buffer){
        GST_WARNING_OBJECT (camerasrc, "[hadle:%p, buffer:%p]", camerasrc, buffer);
        return FALSE;
    }

    if ((clock = GST_ELEMENT_CLOCK (camerasrc))){
        timestamp = GST_ELEMENT (camerasrc)->base_time;
        gst_object_ref (clock);

        timestamp = gst_clock_get_time (clock) - timestamp;
        gst_object_unref (clock);

        if (camerasrc->fps_auto){
            duration = GST_CLOCK_TIME_NONE;
        }else{
            if (camerasrc->high_speed_fps == 0){
                if (camerasrc->fps == 0) {
                    fps_nu   = 0;
                    fps_de = 0;
                }else {
                    fps_nu   = 1;
                    fps_de = camerasrc->fps;
                }
            }else{
                fps_nu = 1;
                fps_de = camerasrc->fps;
            }

            if (fps_nu > 0 && fps_de > 0) {
                GstClockTime latency;

                latency = gst_util_uint64_scale_int (GST_SECOND, fps_nu, fps_de);
                duration = latency;
            }
        }
    }else{
        timestamp = GST_CLOCK_TIME_NONE;
    }

    GST_BUFFER_TIMESTAMP(buffer) = timestamp;
    GST_BUFFER_DURATION(buffer) = duration;

    /*
    GST_LOG_OBJECT(camerasrc, "[%"GST_TIME_FORMAT " dur %" GST_TIME_FORMAT "]",
                              GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (buffer)),
                              GST_TIME_ARGS(GST_BUFFER_DURATION (buffer)));
    */

    return TRUE;
}

static gboolean gst_camerasrc_is_zero_copy_format(const gchar *format_str){
    gboolean result=FALSE;
    if(strcmp(format_str, "SN12") == 0){
        result=TRUE;
    }
    else{
        result=FALSE;
    }
    return result;
}


/*********************************QCAM functions***********************************/
static gboolean
capture_snapshot(GstCameraSrc *camerasrc)
{
    GST_WARNING_OBJECT(camerasrc, "ENTERED");
    camerasrc->snapshot_stream->capture_num = camerasrc->cap_count;
    camerasrc->snapshot_stream->capture_interval = camerasrc->cap_interval;
    camerasrc->snapshot_stream->capture_fourcc = camerasrc->cap_fourcc;
#ifdef USE_NEXT_CAPTURE_TIME
    camerasrc->snapshot_stream->next_captured_time = 0;
#endif /* USE_NEXT_CAPTURE_TIME */
#ifdef CONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
    camerasrc->is_focusing = FALSE;
#else
    camerasrc->is_focusing = TRUE;
#endif
    gboolean do_capture = FALSE;

    GST_WARNING_OBJECT(camerasrc, "snapshot resolution [%dx%d], signal_still_capture %d",
                                  camerasrc->snapshot_stream->snapshot_width,
                                  camerasrc->snapshot_stream->snapshot_height,
                                  camerasrc->signal_still_capture);
#ifdef CAMERA_NONZSL_CAPTURE_FLASH_ON_AUTO
    if((camerasrc->snapshot_stream->flash_mode != 0) && !camerasrc->preview_stream->recording_hint
             && camerasrc->snapshot_stream->capture_num == 1)
#else
    if (0)
#endif
    {
    //Non ZSL
        if (VIDEO_IN_MODE_UNKNOWN != camerasrc->mode) {
             camerasrc->mode = VIDEO_IN_MODE_CAPTURE;

             /* release remained buffer in queue */
             gst_camerasrc_release_all_queued_preview_buffer(camerasrc);

             /* wait all preview buffer finalized for previiew restart */
             gst_camerasrc_wait_all_preview_buffer_finalized(camerasrc);
             do_capture = TRUE;
        }
    }
    else if(camerasrc->cap_hdr !=0)
    {
    //HDR
        if (VIDEO_IN_MODE_UNKNOWN != camerasrc->mode) {
             camerasrc->mode = VIDEO_IN_MODE_CAPTURE;

             /* release remained buffer in queue */
             gst_camerasrc_release_all_queued_preview_buffer(camerasrc);

             /* wait all preview buffer finalized for previiew restart */
             gst_camerasrc_wait_all_preview_buffer_finalized(camerasrc);
             do_capture = TRUE;
        }
    }
    else {
        do_capture = TRUE;
    }

    if(do_capture) {
     /* set snapshot resolution */
        if(camerasrc->cap_width != camerasrc->snapshot_stream->snapshot_width ||  camerasrc->cap_height != camerasrc->snapshot_stream->snapshot_height){
            set_snapshot_dimension(camerasrc->snapshot_stream, camerasrc->cap_width, camerasrc->cap_height);
        }
        if(camerasrc->snapshot_stream->capture_fourcc==GST_MAKE_FOURCC ('N', 'V', '1', '2')
            ||camerasrc->snapshot_stream->capture_fourcc==GST_MAKE_FOURCC ('S', 'N', '1', '2')){
            if (camerasrc->snapshot_stream->capture_num !=4) {  // in lls mode and HDR , do not set raw take set
                send_command(camerasrc->snapshot_stream->camera_handle->cam_device_handle,CAMERA_CMD_RAW_TAKE, 1, 0);
            } else {
                send_command(camerasrc->snapshot_stream->camera_handle->cam_device_handle,CAMERA_CMD_RAW_TAKE, 0, 0);
            }
        }
        else{
            send_command(camerasrc->snapshot_stream->camera_handle->cam_device_handle,CAMERA_CMD_RAW_TAKE, 0, 0);
        }
        if(!start_snapshot(camerasrc->snapshot_stream)) {
            GST_ERROR_OBJECT( camerasrc, "start_snapshot is failed.");
            gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_INTERNAL);
            return FALSE;
        }
    }

    GST_DEBUG_OBJECT (camerasrc, "LEAVED");

    return TRUE;
}

static gpointer _gst_camerasrc_snapshot_thread_func(gpointer data)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)data;
    camerasrc_thread_handle_t *snapshot_handle = NULL;

    if (camerasrc == NULL || camerasrc->snapshot_handle == NULL) {
        GST_ERROR("handle is NULL");
        return NULL;
    }

    GST_WARNING_OBJECT(camerasrc, "start snapshot thread");

    snapshot_handle = camerasrc->snapshot_handle;

    g_mutex_lock(&snapshot_handle->thread_lock);
    while (!snapshot_handle->thread_exit) {
        GST_WARNING_OBJECT(camerasrc, "wait for snapshot signal");
        g_cond_wait(&snapshot_handle->thread_cond, &snapshot_handle->thread_lock);
        GST_WARNING_OBJECT(camerasrc, "snapshot signal received");

        if (snapshot_handle->thread_exit) {
            break;
        }

        /* snapshot_done_signal*/
        snapshot_done_signal(camerasrc);

    }
    g_mutex_unlock(&snapshot_handle->thread_lock);

    GST_WARNING_OBJECT(camerasrc, "snapshot thread exit");

    return NULL;
}
static long _gst_camerasrc_hdr_progressing_cb(long progress, long status, void *user_data)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)user_data;

    if (camerasrc == NULL) {
        GST_WARNING("handle is NULL");
        return HDR_PROCESS_NO_ERROR;
    }

    GST_INFO_OBJECT(camerasrc,"HDR progressing %d percent completed(state:%d)...", progress, status);

    gst_camerasrc_post_message_int(camerasrc, "camerasrc-HDR", "progress", progress);

    return HDR_PROCESS_NO_ERROR;
}


static gboolean gst_camerasrc_run_hdr_processing(GstCameraSrc *camerasrc, HDRsrc_buffer_t *input_buffer, HDRInputPictureFormat format, unsigned char **result_data)
{
    int ret = HDR_PROCESS_NO_ERROR;
    if (camerasrc == NULL || result_data == NULL) {
        GST_ERROR_OBJECT(camerasrc,"handle[%p] or result_data[%p] is NULL",
        camerasrc, result_data);
        return FALSE;
    }
    GST_INFO_OBJECT(camerasrc,"cap_height = %d,cap_width =%d format=%d",camerasrc->cap_height,camerasrc->cap_width,format);
    ret= run_hdr_processing(input_buffer,
                                            camerasrc->cap_width,
                                            camerasrc->cap_height,
                                            format,
                                            result_data,
                                            (void*)_gst_camerasrc_hdr_progressing_cb,
                                            (void*)camerasrc);
    if(HDR_PROCESS_NO_ERROR!=ret){
        GST_ERROR_OBJECT(camerasrc,"HDR process failed with error %d", ret);
        return FALSE;
    }
    GST_INFO_OBJECT(camerasrc,"HDR process success");
    return TRUE;
}

#if _ENABLE_CAMERASRC_DEBUG
static int
data_dump (char *filename, void *data, int size){
    FILE *fp = NULL;
    fp = fopen (filename, "a+");
    if (!fp)
        return FALSE;
    fwrite (data, 1, size, fp);
    fclose (fp);
    return TRUE;
}
#endif

static void snapshot_done_signal(GstCameraSrc *camerasrc)
{
    camera_snapshot_hw_intf *snapshot_stream = camerasrc->snapshot_stream;
    int hdr_jpeg_buffer_size, hdr_ref_img_jpeg_buffer_size;
    unsigned char *hdr_jpeg_buffer = NULL;
    unsigned char *hdr_ref_img_jpeg_buffer = NULL;
    unsigned char *hdr_raw_buffer = NULL;
    GstCaps *hdr_jpeg_buffer_caps = NULL;
    GstSample *hdr_jpeg_buffer_sample = NULL;
    camera_memory_t *screennail_data = NULL;
    unsigned char *hdr_out_data = NULL;

#ifdef USE_NEXT_CAPTURE_TIME
    unsigned long current_time = 0;
#endif /* USE_NEXT_CAPTURE_TIME */

    GST_WARNING_OBJECT (camerasrc, "ENTERED");

    g_mutex_lock(camerasrc->capture_buffer_lock);

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    g_mutex_lock(camerasrc->restart_camera_lock);
    GST_WARNING_OBJECT(camerasrc, "restart camera signal");
    g_cond_signal(camerasrc->restart_camera_cond);
    g_mutex_unlock(camerasrc->restart_camera_lock);
#else
#ifdef CAMERA_NONZSL_CAPTURE_FLASH_ON_AUTO
    if(camerasrc->snapshot_stream->flash_mode != 0)
#else
    if(0)
#endif
    {
        g_mutex_lock(camerasrc->restart_camera_lock);
        GST_WARNING_OBJECT(camerasrc, "restart camera signal");
        g_cond_signal(camerasrc->restart_camera_cond);
        g_mutex_unlock(camerasrc->restart_camera_lock);
    }
   else if(camerasrc->cap_hdr != 0)
   {
        g_mutex_lock(camerasrc->restart_camera_lock);
        GST_WARNING_OBJECT(camerasrc, "restart camera signal");
        g_cond_signal(camerasrc->restart_camera_cond);
        g_mutex_unlock(camerasrc->restart_camera_lock);
   }
#endif

    screennail_data=get_screen_nail_frame(camerasrc->preview_stream);
    GstBuffer *main_buf_cap_signal = NULL;
    GstCaps *buf_caps_main = NULL;
    GstSample *buf_sample_main = NULL;
    GstBuffer *hdr_ref_img_buf_cap_signal = NULL;
    GstBuffer *ssnail_buf_cap_signal = NULL;
    GstCaps *ssnail_buf_caps = NULL;
    GstSample *ssnail_buf_sample = NULL;


    if(!snapshot_stream->m_hdr_mode) {
        main_buf_cap_signal = gst_buffer_new_wrapped_full(0, snapshot_stream->capture_data_buffer, snapshot_stream->capture_data_size, 0, snapshot_stream->capture_data_size, NULL, NULL);
        if(screennail_data){
            if(screennail_data->data){
                ssnail_buf_cap_signal = gst_buffer_new_wrapped_full(0, screennail_data->data, screennail_data->size, 0, screennail_data->size, NULL, NULL);
                 if (ssnail_buf_cap_signal == NULL) {
                    GST_WARNING_OBJECT(camerasrc,"No Screen Nail data");
                 }
            }
        }
    }

    if(snapshot_stream->m_hdr_mode) {
#if 0 /* In HDR mode, the signal for capture sound is sent when 1st raw data cb was came. you can see this on snapshot_data_cb()*/
        /* send message for noti to complete to get frames */
        gst_camerasrc_post_message_int(camerasrc, "camerasrc-Capture", "capture-done", TRUE);
#endif
        gboolean ret = CAMERASRC_SUCCESS;
        unsigned int thumb_width = 0;
        unsigned int thumb_height = 0;
        unsigned int thumb_length = 0;
        float ratio =0;

        ret = gst_camerasrc_run_hdr_processing(camerasrc, snapshot_stream->m_bracket_buffer, HDR_FORMAT_NV12,&hdr_out_data);
        if (ret == FALSE) {
                GST_ERROR_OBJECT( camerasrc, "HDR process failed");
                gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_INTERNAL);
                g_mutex_unlock(camerasrc->capture_buffer_lock);
                return;
            } else { // success
            GST_WARNING_OBJECT( camerasrc, "HDR process success");
#if _ENABLE_CAMERASRC_DEBUG
              data_dump("/opt/usr/media/Others/snapshot_hdr_result.raw",hdr_out_data,YUV420_SIZE(snapshot_stream->snapshot_width,snapshot_stream->snapshot_height));
#endif
            hdr_jpeg_buffer_size = (snapshot_stream->snapshot_width*snapshot_stream->snapshot_height*3)/2;

            GST_WARNING_OBJECT( camerasrc, "HDR image encoding started - hw ");
            ret =  jpeg_encode_hw(
                            hdr_out_data,
                            snapshot_stream->snapshot_width,
                            snapshot_stream->snapshot_height,
                            3,									// QUALITY_MIDDLE_HIGH
                            &hdr_jpeg_buffer,
                            &hdr_jpeg_buffer_size
                            );

            GST_WARNING_OBJECT( camerasrc, "HDR image encoding completed - hw :err code:%d",ret);
            if (ret != 0){ // switching to software encoding
                GST_WARNING_OBJECT( camerasrc, "HDR image encoding started - software ");
                ret = mm_util_jpeg_encode_to_memory(
                                  &hdr_jpeg_buffer,
                                  &hdr_jpeg_buffer_size,
                                  hdr_out_data,
                                  snapshot_stream->snapshot_width,
                                  snapshot_stream->snapshot_height,
                                  MM_UTIL_JPEG_FMT_NV12,
                                  DEFAULT_JPEG_ENCODE_QUALITY);
                GST_WARNING_OBJECT( camerasrc, "HDR image encoding completed - software ");
            }

            if (NULL != ret) {
                GST_WARNING_OBJECT( camerasrc, "HDR JPEG encode failed");
            } else {
                GST_WARNING_OBJECT( camerasrc, "HDR JPEG encode success");
#if _ENABLE_CAMERASRC_DEBUG
                data_dump("/opt/usr/media/Others/hdr_result.jpg",hdr_jpeg_buffer,hdr_jpeg_buffer_size);
#endif

                if (camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL) {
                    GST_WARNING_OBJECT( camerasrc, "HDR original image encoding started - hw ");
                    ret =  jpeg_encode_hw(
                            snapshot_stream->m_bracket_buffer[1].planes[0].start,
                            snapshot_stream->snapshot_width,
                            snapshot_stream->snapshot_height,
                            3,									// QUALITY_MIDDLE_HIGH
                            &hdr_ref_img_jpeg_buffer,
                            &hdr_ref_img_jpeg_buffer_size
                            );
                    GST_WARNING_OBJECT( camerasrc, "HDR original image encoding completed - hw :err code:%d",ret);
                    if (ret != 0) { // switching to software encoding
                        ret = mm_util_jpeg_encode_to_memory_hw(
                            &hdr_ref_img_jpeg_buffer,
                            &hdr_ref_img_jpeg_buffer_size,
                            snapshot_stream->m_bracket_buffer[1].planes[0].start,
                            camerasrc->cap_width,
                            camerasrc->cap_height,
                            MM_UTIL_JPEG_FMT_NV12,
                            DEFAULT_JPEG_ENCODE_QUALITY);
                        GST_WARNING_OBJECT( camerasrc, "HDR original image encoding completed - software ");
                    }
                    if (NULL != ret) {
                        GST_WARNING_OBJECT( camerasrc, "HDR original JPEG encode failed");
                    }
                    else {
                        GST_WARNING_OBJECT( camerasrc, "HDR original JPEG encode success");
                        hdr_ref_img_buf_cap_signal = gst_buffer_new_wrapped_full(0,
                            hdr_ref_img_jpeg_buffer,
                            hdr_ref_img_jpeg_buffer_size,
                            0,
                            hdr_ref_img_jpeg_buffer_size,
                            NULL,
                            NULL);
                        GST_WARNING_OBJECT( camerasrc, "hdr_ref_img_buf_cap_signal create success");
                    }
                }
            }

            main_buf_cap_signal = gst_buffer_new_wrapped_full(0,
                              hdr_jpeg_buffer,
                              hdr_jpeg_buffer_size,
                              0,
                              hdr_jpeg_buffer_size,
                              NULL,
                              NULL);
            ratio=(float)snapshot_stream->snapshot_width/snapshot_stream->snapshot_height;
            GST_WARNING(" ratio = %ld",ratio);

            if(ratio == ASPECT_RATIO_1_1) {
                thumb_width = 240;
                thumb_height = 240;
            } else if(ratio > ASPECT_RATIO_1_1 && ratio < ASPECT_RATIO_4_3) {
                thumb_width = 512;
                thumb_height = 384;
            } else if(ratio >= ASPECT_RATIO_4_3 && ratio < ASPECT_RATIO_16_9) {
                thumb_width = 320;
                thumb_height = 192;
            } else if(ratio >= ASPECT_RATIO_16_9) {
                thumb_width = 512;
                thumb_height = 288;
            }
            GST_WARNING(" screen nail size (%d x %d)",thumb_width,thumb_height);

            thumb_length = thumb_width * thumb_height*3/2;
            hdr_raw_buffer = (unsigned char *)malloc(thumb_length);

            ret = mm_util_resize_image(hdr_out_data, snapshot_stream->snapshot_width, snapshot_stream->snapshot_height, 3,
                              hdr_raw_buffer, &thumb_width, &thumb_height);
            if (NULL != ret) {
                GST_WARNING_OBJECT( camerasrc, "HDR raw resize failed");
            } else {
                GST_WARNING_OBJECT( camerasrc, "HDR raw resize success");
            }
            ssnail_buf_cap_signal = gst_buffer_new_wrapped_full(0, hdr_raw_buffer, thumb_length, 0, thumb_length, NULL, NULL);
            if (ssnail_buf_cap_signal != NULL) {
                ssnail_buf_caps = gst_caps_new_simple("video/x-raw",
                                                                     "format", G_TYPE_STRING, "NV12",
                                                                     "width", G_TYPE_INT, thumb_width,
                                                                     "height", G_TYPE_INT, thumb_height,
                                                                     NULL);
                ssnail_buf_sample = gst_sample_new(ssnail_buf_cap_signal, ssnail_buf_caps, NULL, NULL);
            }
            camerasrc->cap_provide_exif = FALSE;
    }
  }

    if (snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('J', 'P', 'E', 'G')) {
        if(NULL != main_buf_cap_signal) {
            buf_caps_main = gst_caps_new_simple("image/jpeg",
                                          "width", G_TYPE_INT, snapshot_stream->snapshot_width,
                                          "height", G_TYPE_INT,snapshot_stream->snapshot_height,
                                          NULL);
            buf_sample_main = gst_sample_new(main_buf_cap_signal, buf_caps_main, NULL, NULL);
        }
        if (camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL) {
            hdr_jpeg_buffer_caps = gst_caps_new_simple("image/jpeg",
                                          "width", G_TYPE_INT, snapshot_stream->snapshot_width,
                                          "height", G_TYPE_INT,snapshot_stream->snapshot_height,
                                          NULL);
            hdr_jpeg_buffer_sample = gst_sample_new(hdr_ref_img_buf_cap_signal, hdr_jpeg_buffer_caps, NULL, NULL);
        }
    }
     else if (snapshot_stream->capture_fourcc==GST_MAKE_FOURCC ('N', 'V', '1', '2') ||
               snapshot_stream->capture_fourcc==GST_MAKE_FOURCC ('S', 'N', '1', '2')) {
        if(NULL != main_buf_cap_signal) {
            buf_caps_main = gst_caps_new_simple("video/x-raw",
                                                                   "format", G_TYPE_STRING, "NV12",
                                                                   "width", G_TYPE_INT, snapshot_stream->snapshot_width,
                                                                   "height", G_TYPE_INT, snapshot_stream->snapshot_height,NULL);
            buf_sample_main = gst_sample_new(main_buf_cap_signal, buf_caps_main, NULL, NULL);
        }
    }
    if (!snapshot_stream->m_hdr_mode &&(ssnail_buf_cap_signal != NULL)) {
        ssnail_buf_caps = gst_caps_new_simple("video/x-raw",
                                                                     "format", G_TYPE_STRING, "NV12",
                                                                     "width", G_TYPE_INT, camerasrc->preview_stream->preview_width,
                                                                     "height", G_TYPE_INT, camerasrc->preview_stream->preview_height,
                                                                     NULL);
        ssnail_buf_sample = gst_sample_new(ssnail_buf_cap_signal, ssnail_buf_caps, NULL, NULL);
    }
#ifdef USE_NEXT_CAPTURE_TIME
        /* check capture interval */
        if (snapshot_stream->capture_num > 1) {
            current_time = gst_get_current_time();
            if (snapshot_stream->next_captured_time != 0) {
                if (snapshot_stream->next_captured_time > current_time) {
                    GST_WARNING_OBJECT(camerasrc, "sleep time %u ms", snapshot_stream->next_captured_time - current_time);
                    usleep((snapshot_stream->next_captured_time - current_time)*1000);
                } else {
                    GST_WARNING_OBJECT(camerasrc, "no need to sleep");
                }
                snapshot_stream->next_captured_time += snapshot_stream->capture_interval;
            } else {
                snapshot_stream->next_captured_time = current_time + snapshot_stream->capture_interval;
            }
        }
#endif /* USE_NEXT_CAPTURE_TIME */

    if(camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL) {
            g_signal_emit( G_OBJECT (camerasrc),
                            gst_camerasrc_signals[SIGNAL_STILL_CAPTURE],
                            0,
                            hdr_jpeg_buffer_sample,
                            NULL,//TODO:Is there a way to get thumbnail buffer separately with mm_camera_interface2
                            (snapshot_stream->m_hdr_mode? NULL: ssnail_buf_sample));//TODO:Save last preview before capture and add it here??
    }

    g_signal_emit( G_OBJECT (camerasrc),
                    gst_camerasrc_signals[SIGNAL_STILL_CAPTURE],
                    0,
                    buf_sample_main,
                    NULL,//TODO:Is there a way to get thumbnail buffer separately with mm_camera_interface2
                    ssnail_buf_sample);//TODO:Save last preview before capture and add it here??

    camerasrc->cap_provide_exif = TRUE;

    if(snapshot_stream->m_hdr_mode && snapshot_stream->m_bracket_buffer_count == HDR_BRACKET_FRAME_NUM){
        int i=0;
        for(i=0;i<snapshot_stream->m_bracket_buffer_count;i++){
            GST_WARNING_OBJECT(camerasrc, "HDR: freeing bracket buffer");
            free(snapshot_stream->m_bracket_buffer[i].planes[0].start);
        }

        if (NULL != hdr_jpeg_buffer) {
            GST_WARNING_OBJECT(camerasrc, "HDR: freeing jpeg  bufferr");
            free(hdr_jpeg_buffer);
            hdr_jpeg_buffer = NULL;
        }

        if (NULL != hdr_raw_buffer) {
            GST_WARNING_OBJECT(camerasrc, "HDR: freeing raw bufferr");
            free(hdr_raw_buffer);
            hdr_raw_buffer = NULL;
        }

        if ((camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL)
            && (NULL != hdr_ref_img_jpeg_buffer)) {
            GST_WARNING_OBJECT(camerasrc, "HDR ORIGINAL: freeing jpeg buffer");
            free(hdr_ref_img_jpeg_buffer);
            hdr_ref_img_jpeg_buffer = NULL;
        }
        snapshot_stream->m_bracket_buffer_count = 0;

        if (hdr_out_data != NULL) {
            GST_WARNING_OBJECT(camerasrc, "HDR: freeing hdr out buffer");
            free(hdr_out_data);
            hdr_out_data = NULL;
        }
    }

        if(main_buf_cap_signal != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free main_buf_cap_signal");
                gst_buffer_unref(main_buf_cap_signal);
        }

        if(ssnail_buf_cap_signal != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free ssnail_buf_cap_signal");
                gst_buffer_unref(ssnail_buf_cap_signal);
        }

        if(hdr_ref_img_buf_cap_signal != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free hdr_ref_img_buf_cap_signal");
                gst_buffer_unref(hdr_ref_img_buf_cap_signal);
        }

        if(buf_caps_main != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free buf_caps_main");
                gst_caps_unref(buf_caps_main);
        }

        if(ssnail_buf_caps != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free ssnail_buf_caps");
                gst_caps_unref(ssnail_buf_caps);
        }

        if(hdr_jpeg_buffer_caps != NULL) {
                GST_WARNING_OBJECT(camerasrc,"free hdr_jpeg_buffer_caps");
                gst_caps_unref(hdr_jpeg_buffer_caps);
        }

    free_cached_screen_nail_frame(screennail_data);

    if(snapshot_stream->snapshot_data){
        snapshot_stream->snapshot_data->release(snapshot_stream->snapshot_data);
        snapshot_stream->capture_data_buffer = NULL;
        snapshot_stream->capture_data_size = 0;
        snapshot_stream->snapshot_data = NULL;
        GST_WARNING_OBJECT(camerasrc,"free snapshot data");
    }
    g_mutex_unlock(camerasrc->capture_buffer_lock);

    GST_WARNING_OBJECT (camerasrc, "LEAVED");
    return;
}

static void
record_frame_cb(camera_memory_t *bufs,void *user_data,int bufferIndex){
    GstCameraSrc *camerasrc=(GstCameraSrc*)user_data;
    camera_memory_t *frame = bufs;
#ifdef ENABLE_ZERO_COPY
    MMVideoBuffer *img_buffer =NULL;
#endif
    GstCameraBuffer *buffer = NULL;
    GstCaps *buffer_caps = NULL;
    GstSample *buffer_sample = NULL;
    GST_DEBUG_OBJECT(camerasrc, "Entered record_frame_cb");

    int size = 0;
    int video_width = 0;
    int video_height = 0;

    g_mutex_lock(camerasrc->video_buffer_lock);

    if (camerasrc->mode == VIDEO_IN_MODE_UNKNOWN) {
        GST_WARNING_OBJECT(camerasrc, "UNKNOWN video mode - release buffer");
        done_record_frame(camerasrc->record_stream, frame);
        g_mutex_unlock(camerasrc->video_buffer_lock);
        return;
    } else if (camerasrc->mode != VIDEO_IN_MODE_VIDEO) {
        GST_WARNING_OBJECT(camerasrc, "STOP video mode - just return");
        g_mutex_unlock(camerasrc->video_buffer_lock);
        return;
    }

    buffer = gst_camerasrc_buffer_new(camerasrc);
    video_width = camerasrc->video_width;
    video_height = camerasrc->video_height;
    size = frame->size;
#ifdef ENABLE_ZERO_COPY
    if (gst_camerasrc_is_zero_copy_format(camerasrc->format_str)) {
        GST_DEBUG_OBJECT(camerasrc, "zero copy is enabled");
        img_buffer = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));
        memset(img_buffer, 0x0, sizeof(MMVideoBuffer));

        img_buffer->type = MM_VIDEO_BUFFER_TYPE_TBM_BO;
        img_buffer->handle_num = 1;
        img_buffer->handle.bo[0] = frame->handle;
        img_buffer->handle.paddr[0] = frame->physical_addr;
        img_buffer->handle.paddr[1] = frame->physical_addr + (video_height*video_width);
        img_buffer->stride_height[0] = video_height;
        img_buffer->stride_width[0] = video_width;
        img_buffer->stride_height[1] = video_height >> 1;
        img_buffer->stride_width[1] = video_width;
        img_buffer->handle_size[0] = frame->size;
        img_buffer->plane_num = 2;
        img_buffer->height[0] = video_height;
        img_buffer->width[0] = video_width;
        img_buffer->height[1] = video_height >> 1;
        img_buffer->width[1] = video_width;
        img_buffer->data[0] = frame->data;
        img_buffer->data[1] = img_buffer->data[0] + (img_buffer->stride_height[0] * img_buffer->stride_width[0]);
        img_buffer->size[0] = img_buffer->height[0] * img_buffer->width[0];
        img_buffer->size[1] = img_buffer->height[1] * img_buffer->width[1];
        GstMapInfo map;
        gst_buffer_map(buffer->buffer,&map,GST_MAP_WRITE);
        map.data = frame->data;
        map.size = size;
        gst_buffer_unmap(buffer->buffer,&map);
        buffer->buffer_metadata = (void *)frame;
        buffer->buffer_type = RECORD_BUFFER;
        buffer->buffer_index = bufferIndex;
        GST_DEBUG_OBJECT(camerasrc, "record buffer %p, img_buffer %p, bo %p",
                                    buffer, img_buffer, img_buffer->handle.bo[0]);
        GstMemory *meta = NULL;
        meta = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY, img_buffer, sizeof(*img_buffer), 0, sizeof(*img_buffer), img_buffer, free);
        gst_buffer_append_memory(buffer->buffer, meta);
    } else
#endif
    {
        GST_DEBUG_OBJECT(camerasrc, "Using camera buffer Non Zero-Copy format");
        GstMapInfo map;
        gst_buffer_map(buffer->buffer,&map,GST_MAP_WRITE);
        map.data = frame->data;
        map.size = size;
        gst_buffer_unmap(buffer->buffer,&map);

        buffer->buffer_type = RECORD_BUFFER;
    }

    /* increase video buffer count */
    GST_DEBUG_OBJECT(camerasrc,"get video buffer - live %d -> %d",
                               camerasrc->num_video_buffers, camerasrc->num_video_buffers + 1);
    camerasrc->num_video_buffers++;

    g_mutex_unlock(camerasrc->video_buffer_lock);

    /* get time info */
    gst_camerasrc_get_timeinfo(camerasrc, (GstBuffer*)buffer);

    if (camerasrc->firsttime_record) {
        int fps_de = 0;
        int fps_nu = 0;
        camerasrc->firsttime_record = FALSE;

        if (camerasrc->fps <= 0) {
            fps_nu = 0;
            fps_de = 1;
        } else {
            fps_nu = camerasrc->fps;
            fps_de = 1;
        }
       buffer_caps = gst_caps_new_simple("video/x-raw",
                                                      "format", G_TYPE_STRING, camerasrc->format_str,
                                                      "width", G_TYPE_INT, video_width,
                                                      "height", G_TYPE_INT, video_height,
                                                      "framerate", GST_TYPE_FRACTION, fps_nu, fps_de,
                                                      NULL);
       buffer_sample = gst_sample_new(buffer, buffer_caps, NULL, NULL);

    }

    g_signal_emit(G_OBJECT (camerasrc),
                  gst_camerasrc_signals[SIGNAL_VIDEO_STREAM_CB],
                  0,
                  buffer_sample
                  );

    GST_DEBUG_OBJECT(camerasrc, "return video-stream-cb");

#ifdef ENABLE_ZERO_COPY
    if(!gst_camerasrc_is_zero_copy_format(camerasrc->format_str))
#endif
    {
        done_record_frame(camerasrc->record_stream,(void*)frame);
    }

    GST_DEBUG_OBJECT (camerasrc, "LEAVED");

    return;
}

static void
preview_frame_cb(camera_memory_t *bufs,void *user_data, int bufferIndex)
{
    GstCameraSrc *camerasrc=(GstCameraSrc*)user_data;
    camera_memory_t *frame = bufs;
#ifdef ENABLE_ZERO_COPY
    MMVideoBuffer *img_buffer =NULL;
#endif
    GstCameraBuffer *buffer = NULL;

    int size = ((camerasrc->preview_stream->preview_width * camerasrc->preview_stream->preview_height * 3) >> 1);
    GST_DEBUG_OBJECT(camerasrc, "Entered preview_frame_cb");

    get_preview_stream_mutex (camerasrc->preview_stream);

    if (camerasrc->mode == VIDEO_IN_MODE_UNKNOWN || camerasrc->mode == VIDEO_IN_MODE_CAPTURE) {
        GST_WARNING_OBJECT(camerasrc, "Current camerasrc mode(%d) mode - release buffer", camerasrc->mode);

        release_preview_stream_mutex (camerasrc->preview_stream);
        done_preview_frame(camerasrc->preview_stream, bufferIndex);
#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
        free(bufs);
#endif
#endif
        return;
    } else if (camerasrc->mode == VIDEO_IN_MODE_STOP) {
        GST_WARNING_OBJECT(camerasrc, "STOP preview mode - just return");
        release_preview_stream_mutex (camerasrc->preview_stream);
#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
        free(bufs);
#endif
#endif
        return;
    }
       else if(camerasrc->mode == VIDEO_IN_MODE_VIDEO) {
           record_frame_cb(bufs,user_data,bufferIndex);
    }

    g_mutex_lock(camerasrc->preview_buffer_lock);
    if(camerasrc->num_live_buffers > 5) {
        GST_WARNING_OBJECT(camerasrc, "Display has already %dea preview buffers. So, we re-qbuf this buffer.",camerasrc->num_live_buffers);
        camerasrc->qbuf_without_display++;
        if(camerasrc->qbuf_without_display > 50){
            GST_ERROR_OBJECT(camerasrc, "50 times continuous qbuf without display return error %d",camerasrc->qbuf_without_display);
            release_preview_stream_mutex(camerasrc->preview_stream);
            done_preview_frame(camerasrc->preview_stream, bufferIndex);
#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
            free(bufs);
#endif
#endif
            g_mutex_unlock(camerasrc->preview_buffer_lock);
            return;
        }
        release_preview_stream_mutex(camerasrc->preview_stream);
        done_preview_frame(camerasrc->preview_stream, bufferIndex);
#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
        free(bufs);
#endif
#endif
        g_mutex_unlock(camerasrc->preview_buffer_lock);
        return;
    }
    camerasrc->qbuf_without_display = 0;
    g_mutex_unlock(camerasrc->preview_buffer_lock);

    buffer = gst_camerasrc_buffer_new(camerasrc);

#ifdef ENABLE_ZERO_COPY
    if (gst_camerasrc_is_zero_copy_format(camerasrc->format_str)) {
         GST_DEBUG_OBJECT(camerasrc, "zero copy is enabled");
        img_buffer = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));
        if (img_buffer == NULL)
        {
            GST_ERROR_OBJECT(camerasrc, "Memory allocation for MMVideoBuffer failed");

            release_preview_stream_mutex(camerasrc->preview_stream);
            done_preview_frame(camerasrc->preview_stream, bufferIndex);
            buffer->buffer_type = -1;
            gst_camerasrc_buffer_finalize(buffer);

#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
                free(bufs);
#endif
#endif
            return;
        }
        memset(img_buffer, 0x0, sizeof(MMVideoBuffer));

        img_buffer->type = MM_VIDEO_BUFFER_TYPE_TBM_BO;
        img_buffer->handle_num = 1;
        img_buffer->handle.bo[0] = frame->handle;
        img_buffer->handle.paddr[0] = frame->physical_addr;
        img_buffer->handle.paddr[1] = frame->physical_addr + (camerasrc->preview_stream->preview_height * camerasrc->preview_stream->preview_width);
        img_buffer->stride_height[0] = camerasrc->preview_stream->preview_height;
        img_buffer->stride_width[0] = camerasrc->preview_stream->preview_width;
        img_buffer->stride_height[1] = (camerasrc->preview_stream->preview_height) >> 1;
        img_buffer->stride_width[1] = camerasrc->preview_stream->preview_width;
        img_buffer->handle_size[0] = frame->size;
        img_buffer->plane_num = 2;
        img_buffer->height[0] = camerasrc->preview_stream->preview_height;
        img_buffer->width[0] = camerasrc->preview_stream->preview_width;
        img_buffer->height[1] = camerasrc->preview_stream->preview_height >> 1;
        img_buffer->width[1] = camerasrc->preview_stream->preview_width;
        img_buffer->data[0] = frame->data;
        img_buffer->data[1] = img_buffer->data[0] + (camerasrc->preview_stream->preview_height * camerasrc->preview_stream->preview_width);
        img_buffer->size[0] = img_buffer->height[0] * img_buffer->width[0];
        img_buffer->size[1] = img_buffer->height[1] * img_buffer->width[1];
        buffer->buffer_metadata = (void*)frame;
        buffer->buffer_type = PREVIEW_BUFFER;
        buffer->buffer_index = bufferIndex;

        GST_DEBUG_OBJECT(camerasrc, "Zero-Copy preview - bo [%p] size [%dx%d] viraddr[%x]",
                                    frame->handle, img_buffer->width[0], img_buffer->height[0],(unsigned int) img_buffer->data[0]);
    } else
#endif
    {
        GST_DEBUG_OBJECT(camerasrc, "Using camera buffer Non Zero-Copy format");
        buffer->buffer_metadata = (void*)frame;
        buffer->buffer_type = PREVIEW_BUFFER;
        buffer->buffer_index = bufferIndex;
    }

    /* increase preview buffer count */
    g_mutex_lock(camerasrc->preview_buffer_lock);
    GST_DEBUG_OBJECT(camerasrc,"get preview buffer - num_live_buffers %d -> %d",
                               camerasrc->num_live_buffers, camerasrc->num_live_buffers + 1);
    camerasrc->num_live_buffers++;
    g_mutex_unlock(camerasrc->preview_buffer_lock);

    /* get time info */
    gst_camerasrc_get_timeinfo(camerasrc, (GstBuffer*)buffer->buffer);

    if (camerasrc->firsttime) {
        if(camerasrc->face_detect){
            GST_WARNING("Face detection need to be re-setting(%d)", camerasrc->face_detect);
            send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_START_FACE_DETECTION, camerasrc->face_detect, 0);
        }
        camerasrc->firsttime = FALSE;
        GST_WARNING("First Preview frame is comming.");
    }
    gst_buffer_append_memory(buffer->buffer, gst_memory_new_wrapped(0,
                                                            frame->data,
                                                            size,
                                                            0,
                                                            size,
                                                            buffer,
                                                            (GDestroyNotify)gst_camerasrc_buffer_finalize));

    if (img_buffer) {
        gst_buffer_append_memory(buffer->buffer,
                    gst_memory_new_wrapped(0,
                                            img_buffer,
                                            sizeof(*img_buffer),
                                            0,
                                            sizeof(*img_buffer),
                                            img_buffer,
                                            free)
                                            );
    }

    buffer = (GstCameraBuffer*)buffer->buffer;
    GST_DEBUG_OBJECT(camerasrc, "gst_buffer_append_memory done %p",buffer);
    cache_preview_frame(camerasrc->preview_stream,(GstBuffer*)buffer);

    release_preview_stream_mutex(camerasrc->preview_stream);
#ifdef __MINT_CONF_SPRD__
#ifndef CONFIG_SS_PREVIEW_HEAP
    free(bufs);
#endif
#endif
    GST_DEBUG_OBJECT(camerasrc,"leave");
    return;
}

/******************************Suppliment functions*********************************/

/* VOID:OBJECT,OBJECT (generated by 'glib-genmarshal') */
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer
void
gst_camerasrc_VOID__OBJECT_OBJECT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     arg_2,
                                                    gpointer     arg_3,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 4);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_object (param_values + 1),
            g_marshal_value_peek_object (param_values + 2),
            g_marshal_value_peek_object (param_values + 3),
            data2);
}

void
gst_camerasrc_VOID__OBJECT_VIDEO_STREAM (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_VIDEO_STREAM) (gpointer     data1,
                                                    gpointer     arg,
                                                    gpointer     data2);
  register GMarshalFunc_VOID__OBJECT_VIDEO_STREAM callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__OBJECT_VIDEO_STREAM) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_object (param_values + 1),
            data2);
}

static GType
gst_camerasrc_quality_get_type (void)
{
    static GType camerasrc_quality_type = 0;
    static const GEnumValue quality_types[] = {
        {GST_CAMERASRC_QUALITY_LOW, "Low quality", "low"},
        {GST_CAMERASRC_QUALITY_HIGH, "High quality", "high"},
        {0, NULL, NULL}
    };

    if (!camerasrc_quality_type) {
        camerasrc_quality_type = g_enum_register_static ("GstCameraSrcQuality", quality_types);
    }
    return camerasrc_quality_type;
}

static gboolean
gst_camerasrc_get_caps_info (GstCameraSrc * camerasrc, GstCaps * caps)
{
    GstStructure *structure;
    const GValue *framerate;
    const gchar *format_gst = NULL;
    gint fps_n = 0, fps_d = 0;
    gint w = 0, h = 0, rot = 0;
    const gchar *mimetype;

    GST_WARNING_OBJECT(camerasrc, "ENTERED");

    GST_WARNING_OBJECT(camerasrc, "Collect data for given caps.(caps:%x)", (unsigned int)caps);

    structure = gst_caps_get_structure (caps, 0);

    if (!gst_structure_get_int (structure, "width", &w)){
        GST_ERROR_OBJECT(camerasrc, "Failed to get width info in caps");
        goto _caps_info_failed;
    }else{
        GST_WARNING_OBJECT(camerasrc, "width caps proved [%d]",w);
    }

    if (!gst_structure_get_int (structure, "height", &h)){
        GST_ERROR_OBJECT(camerasrc, "Failed to get height info in caps");
        goto _caps_info_failed;
    }else{
        GST_WARNING_OBJECT(camerasrc, "width caps proved [%d]",h);
    }

    if (!gst_structure_get_int (structure, "rotate", &rot)){
        GST_WARNING_OBJECT(camerasrc, "Failed to get rotate info in caps. set default 0.");
        camerasrc->use_rotate_caps = FALSE;
    } else {
        GST_WARNING_OBJECT(camerasrc, "Succeed to get rotate[%d] info in caps", rot);
        camerasrc->use_rotate_caps = TRUE;
    }

    camerasrc->rotate = rot;
    /*TEMP CODE
    if(w<640) w=640;
    if(h<480) h=480;*/
    camerasrc->preview_stream->preview_width = camerasrc->width = w;
    camerasrc->preview_stream->preview_height = camerasrc->height = h;

    framerate = gst_structure_get_value (structure, "framerate");
    if (!framerate){
        GST_ERROR_OBJECT(camerasrc, "Failed to get framerate info in caps");
        goto _caps_info_failed;
    }

    fps_n = gst_value_get_fraction_numerator (framerate);
    fps_d = gst_value_get_fraction_denominator (framerate);
    camerasrc->fps = (int)((float)fps_n / (float)fps_d);
    camerasrc->preview_stream->fps = camerasrc->fps;
    camerasrc->preview_stream->fps_auto = camerasrc->fps_auto;
    GST_INFO_OBJECT(camerasrc, "auto (%d), fps (%d) = fps_n (%d) / fps_d (%d)",
        camerasrc->fps_auto, camerasrc->fps, fps_n, fps_d);

    mimetype = gst_structure_get_name (structure);
    if (!mimetype){
        GST_ERROR_OBJECT(camerasrc, "Failed to get mimetype info in caps");
        goto _caps_info_failed;
    }
    if (!(format_gst = gst_structure_get_string (structure, "format"))) {
        GST_DEBUG_OBJECT (camerasrc, "Couldnt get format");
    }
    camerasrc->format_str = format_gst;
    if ( strcmp(format_gst, "SN12") == 0 &&
          strcmp(format_gst, "NV12") == 0) {
        camerasrc->preview_stream->preview_format = CAMERASRC_PREVIEW_FORMAT_YUV420SP;
        camerasrc->colorspace = 0;
        GST_INFO_OBJECT (camerasrc, "received standard source format...");
    }
    camerasrc->snapshot_stream->snapshot_format= CAMERASRC_SNAPSHOT_FORMAT_JPEG;
    camerasrc->preview_stream->format_str =camerasrc->format_str;

    if (camerasrc->use_rotate_caps) {
        GST_WARNING_OBJECT(camerasrc, "Caps : resolution (%dx%d) rotate (%d) at %d/%d fps, "
        "pix_format %d, colorspace %d", w, h, rot, fps_n, fps_d, camerasrc->preview_stream->preview_format, camerasrc->colorspace);
    }else{
        GST_WARNING_OBJECT(camerasrc, "Caps : resolution (%dx%d) at %d/%d fps, "
        "pix_format %d, colorspace %d", w, h, fps_n, fps_d, camerasrc->preview_stream->preview_format, camerasrc->colorspace);
    }

    GST_WARNING_OBJECT(camerasrc, "LEAVED");

    return TRUE;

_caps_info_failed:
    GST_ERROR_OBJECT (camerasrc, "Failed to get caps info.");

    return FALSE;
}

static void
gst_camerasrc_error_handler(GstCameraSrc *camerasrc, int ret)
{
    switch (ret) {
    case CAMERASRC_SUCCESS:
    break;
    case CAMERASRC_ERR_IO_CONTROL:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, FAILED, ("IO control error"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_UNAVAILABLE_DEVICE:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, NOT_FOUND, ("Device was not found"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, TOO_LAZY, ("Timeout"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_DEVICE_NOT_SUPPORT:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, SETTINGS, ("Not supported"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_ALLOCATION:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, NO_SPACE_LEFT, ("memory allocation failed"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_BUSY:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, BUSY, ("Device busy"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_PRIVILEGE:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, OPEN_WRITE, ("Permission denied"), GST_ERROR_SYSTEM);
    break;
    case CAMERASRC_ERR_DEVICE_OPEN:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, OPEN_READ_WRITE, ("Open failed"), GST_ERROR_SYSTEM);
    break;

    default:
        GST_ELEMENT_ERROR (camerasrc, RESOURCE, SEEK, (("General video device error[ret=%x]"), ret), GST_ERROR_SYSTEM);
    break;
    }

    return;
}

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
static gpointer _gst_camerasrc_preflash_thread_func(gpointer data)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)data;
    camerasrc_thread_handle_t *preflash_thread_handle = NULL;

    if (camerasrc == NULL || camerasrc->preflash_thread_handle == NULL) {
        GST_ERROR("handle is NULL");
        return NULL;
    }

    GST_WARNING_OBJECT(camerasrc, "start nofocus_preflash_ thread");

    preflash_thread_handle = camerasrc->preflash_thread_handle;

    g_mutex_lock(&preflash_thread_handle->thread_lock);
    while (!preflash_thread_handle->thread_exit) {
        GST_WARNING_OBJECT(camerasrc, "wait for preflash start signal");
        g_cond_wait(&preflash_thread_handle->thread_cond, &preflash_thread_handle->thread_lock);
        GST_WARNING_OBJECT(camerasrc, "preflash start signal received");

        if (preflash_thread_handle->thread_exit) {
            break;
        }

        if(camerasrc->snapshot_stream->flash_mode &&camerasrc->camera_id == CAMERASRC_DEV_ID_PRIMARY){
                GST_INFO_OBJECT(camerasrc, "flash mode is %d, so need to perform preflash",camerasrc->snapshot_stream->flash_mode);
                send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_NOFOCUS_PREFLASH, 0, 0);
        }

         g_mutex_lock(camerasrc->cmd_lock);

        camerasrc->received_capture_command = TRUE;

         //no wait preview frame
         signal_flush_frame(camerasrc->preview_stream);
         g_mutex_unlock(camerasrc->cmd_lock);
    }
    g_mutex_unlock(&preflash_thread_handle->thread_lock);

    GST_WARNING_OBJECT(camerasrc, "nofocus_preflash thread exit");

    return NULL;
}
#endif

#ifdef RAW_CB_FOR_SHUTTER_SOUND
static gpointer _gst_camerasrc_raw_capture_done_thread_func(gpointer data)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)data;
    camerasrc_thread_handle_t *raw_capture_done_handle = NULL;

    if (camerasrc == NULL || camerasrc->raw_capture_done_handle == NULL) {
        GST_ERROR("handle is NULL");
        return NULL;
    }

    GST_WARNING_OBJECT(camerasrc, "start raw_capture_notify");

    raw_capture_done_handle = camerasrc->raw_capture_done_handle;

    g_mutex_lock(&raw_capture_done_handle->thread_lock);
    while (!raw_capture_done_handle->thread_exit) {
        GST_WARNING_OBJECT(camerasrc, "wait for raw_capture_done_ signal");
        g_cond_wait(&raw_capture_done_handle->thread_cond, &raw_capture_done_handle->thread_lock);
        GST_WARNING_OBJECT(camerasrc, "raw_capture_done_ signal received");

        if (raw_capture_done_handle->thread_exit) {
            break;
        }

        /* send message for noti to complete to get frames */
        gst_camerasrc_post_message_int(camerasrc, "camerasrc-Capture", "capture-done", TRUE);

    }
    g_mutex_unlock(&raw_capture_done_handle->thread_lock);

    GST_WARNING_OBJECT(camerasrc, "raw_capture_notify thread exit");

    return NULL;
}
#endif

static gboolean
gst_camerasrc_create(GstCameraSrc *camerasrc)
{
    int ret = TRUE;
    camerasrc_thread_handle_t *snapshot_handle = NULL;
#ifdef RAW_CB_FOR_SHUTTER_SOUND
    camerasrc_thread_handle_t *raw_capture_done_handle = NULL;
#endif
#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    camerasrc_thread_handle_t *preflash_thread_handle=NULL;
#endif
    GST_WARNING_OBJECT(camerasrc, "start");

    if (camerasrc->camera_handle == NULL) {
      GST_INFO("create_camera_handle");
      ret = create_camera_handle((camerasrc_handle_t**)&camerasrc->camera_handle);

      if (ret != CAMERASRC_SUCCESS) {
          goto _ERROR;
      }
    }

    GST_WARNING_OBJECT(camerasrc, "create snapshot thread");

    /* init snapshot thread */
    if (camerasrc->snapshot_handle != NULL) {
        free(camerasrc->snapshot_handle);
        camerasrc->snapshot_handle = NULL;
    }

    snapshot_handle = (camerasrc_thread_handle_t *)malloc(sizeof(camerasrc_thread_handle_t));
    if (snapshot_handle == NULL) {
        GST_ERROR_OBJECT(camerasrc, "failed to alloc snapshot handle");
        goto _ERROR;
    }

    camerasrc->snapshot_handle = snapshot_handle;
    g_mutex_init(&snapshot_handle->thread_lock);
    g_cond_init(&snapshot_handle->thread_cond);
    snapshot_handle->thread_exit = FALSE;
    snapshot_handle->thread = g_thread_try_new("camerasrc_snapshot",
                                          (GThreadFunc)_gst_camerasrc_snapshot_thread_func,
                                          (gpointer)camerasrc,
                                          NULL);
    if (snapshot_handle->thread == NULL) {
        GST_ERROR_OBJECT(camerasrc, "g_thread_try_new failed");
        goto _ERROR;
    }

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    if (camerasrc->raw_capture_done_handle != NULL) {
        free(camerasrc->raw_capture_done_handle);
        camerasrc->raw_capture_done_handle = NULL;
    }

    raw_capture_done_handle = (camerasrc_thread_handle_t *)malloc(sizeof(camerasrc_thread_handle_t));
    if (raw_capture_done_handle == NULL) {
        GST_ERROR_OBJECT(camerasrc, "failed to alloc raw_capture_done_handle");
        goto _ERROR;
    }

    camerasrc->raw_capture_done_handle = raw_capture_done_handle;
    g_mutex_init(&raw_capture_done_handle->thread_lock);
    g_cond_init(&raw_capture_done_handle->thread_cond);
    raw_capture_done_handle->thread_exit = FALSE;
    raw_capture_done_handle->thread = g_thread_try_new("camerasrc_rawdatacb",
                                          (GThreadFunc)_gst_camerasrc_raw_capture_done_thread_func,
                                          (gpointer)camerasrc,
                                          NULL);
    if (raw_capture_done_handle->thread == NULL) {
        GST_ERROR_OBJECT(camerasrc, "g_thread_try_new failed");
        goto _ERROR;
    }
#endif

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    if (camerasrc->preflash_thread_handle != NULL) {
        free(camerasrc->preflash_thread_handle);
        camerasrc->preflash_thread_handle = NULL;
    }

    preflash_thread_handle = (camerasrc_thread_handle_t *)malloc(sizeof(camerasrc_thread_handle_t));
    if ( preflash_thread_handle== NULL) {
        GST_ERROR_OBJECT(camerasrc, "failed to alloc preflash thread handle");
        goto _ERROR;
    }

    camerasrc->preflash_thread_handle = preflash_thread_handle;
    g_mutex_init(&preflash_thread_handle->thread_lock);
    g_cond_init(&preflash_thread_handle->thread_cond);
    preflash_thread_handle->thread_exit = FALSE;
    preflash_thread_handle->thread = g_thread_try_new("camerasrc_preflash",
                                          (GThreadFunc)_gst_camerasrc_preflash_thread_func,
                                          (gpointer)camerasrc,
                                          NULL);
    if (preflash_thread_handle->thread == NULL) {
        GST_ERROR_OBJECT(camerasrc, "g_thread_try_new failed");
        goto _ERROR;
    }
#endif

    camerasrc->preview_stream->camera_handle = camerasrc->camera_handle;
    camerasrc->snapshot_stream->camera_handle = camerasrc->camera_handle;
    camerasrc->record_stream->camera_handle = camerasrc->camera_handle;

    /* set focus mode PAN if front camera */
    if (camerasrc->camera_id == CAMERASRC_DEV_ID_SECONDARY) {
        camerasrc->focus_mode = GST_CAMERASRC_FOCUS_MODE_PAN;
    }

    GST_INFO("open_camera_device");
    ret = open_camera_device((camerasrc_handle_t *)camerasrc->camera_handle, camerasrc->camera_id, &(camerasrc->sensor_orientation));
    if (ret != CAMERASRC_SUCCESS) {
        GST_ERROR_OBJECT(camerasrc, "open_camera_device() failed. errcode = %d", ret);
        goto _ERROR;
    }

    GST_WARNING_OBJECT (camerasrc, "open_camera_device success - sensor orientation %d", camerasrc->sensor_orientation);

    ((camerasrc_handle_t*)(camerasrc->camera_handle))->preview_stream = (void*)camerasrc->preview_stream;
    ((camerasrc_handle_t*)(camerasrc->camera_handle))->snapshot_stream = (void*)camerasrc->snapshot_stream;
    ((camerasrc_handle_t*)(camerasrc->camera_handle))->record_stream = (void*)camerasrc->record_stream;

    GST_INFO("gst_camerasrc_fill_ctrl_list");
    if (!gst_camerasrc_fill_ctrl_list(camerasrc)) {
        GST_WARNING_OBJECT(camerasrc,"Can't fill v4l2 control list.");
    }

    GST_WARNING_OBJECT(camerasrc, "done");

    return TRUE;

_ERROR:
    if (camerasrc->snapshot_handle) {
        if (camerasrc->snapshot_handle->thread) {
            /* send exit signal to snapshot thread */
            g_mutex_lock(&camerasrc->snapshot_handle->thread_lock);
            GST_WARNING_OBJECT(camerasrc, "send signal to exit snapshot thread");
            camerasrc->snapshot_handle->thread_exit = TRUE;
            g_cond_signal(&camerasrc->snapshot_handle->thread_cond);
            g_mutex_unlock(&camerasrc->snapshot_handle->thread_lock);

            /* wait for exit */
            g_thread_join(camerasrc->snapshot_handle->thread);
            camerasrc->snapshot_handle->thread = NULL;
        }

        /* release mutex and cond */
        g_mutex_clear(&camerasrc->snapshot_handle->thread_lock);
        g_cond_clear(&camerasrc->snapshot_handle->thread_cond);

        free(camerasrc->snapshot_handle);
        camerasrc->snapshot_handle = NULL;
    }

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    if (camerasrc->raw_capture_done_handle) {
        if (camerasrc->raw_capture_done_handle->thread) {
            /* send exit signal to raw_capture_done_handle thread */
            g_mutex_lock(&camerasrc->raw_capture_done_handle->thread_lock);
            GST_WARNING_OBJECT(camerasrc, "send signal to exit snapshot thread");
            camerasrc->raw_capture_done_handle->thread_exit = TRUE;
            g_cond_signal(&camerasrc->raw_capture_done_handle->thread_cond);
            g_mutex_unlock(&camerasrc->raw_capture_done_handle->thread_lock);

            /* wait for exit */
            g_thread_join(camerasrc->raw_capture_done_handle->thread);
            camerasrc->raw_capture_done_handle->thread = NULL;
        }

        /* release mutex and cond */
        g_mutex_clear(&camerasrc->raw_capture_done_handle->thread_lock);
        g_cond_clear(&camerasrc->raw_capture_done_handle->thread_cond);

        free(camerasrc->raw_capture_done_handle);
        camerasrc->raw_capture_done_handle = NULL;
    }
#endif

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    if (camerasrc->preflash_thread_handle) {
        if (camerasrc->preflash_thread_handle->thread) {
            /* send exit signal to snapshot thread */
            g_mutex_lock(&camerasrc->preflash_thread_handle->thread_lock);
            GST_WARNING_OBJECT(camerasrc, "send signal to exit snapshot thread");
            camerasrc->preflash_thread_handle->thread_exit = TRUE;
            g_cond_signal(&camerasrc->preflash_thread_handle->thread_cond);
            g_mutex_unlock(&camerasrc->preflash_thread_handle->thread_lock);

            /* wait for exit */
            g_thread_join(camerasrc->preflash_thread_handle->thread);
            camerasrc->preflash_thread_handle->thread = NULL;
        }

        /* release mutex and cond */
        g_mutex_clear(&camerasrc->preflash_thread_handle->thread_lock);
        g_cond_clear(&camerasrc->preflash_thread_handle->thread_cond);

        free(camerasrc->preflash_thread_handle);
        camerasrc->preflash_thread_handle = NULL;
    }
#endif

    if (camerasrc->camera_handle) {
        free(camerasrc->camera_handle);
        camerasrc->camera_handle = NULL;
    }

    gst_camerasrc_error_handler(camerasrc, ret);

    return FALSE;
}


static gboolean
gst_camerasrc_destroy(GstCameraSrc *camerasrc)
{
    GST_WARNING_OBJECT(camerasrc, "ENTERED - mode %d", camerasrc->mode);

    /*Empty control list */
    gst_camerasrc_empty_ctrl_list(camerasrc);

    /* remove snapshot thread */
    g_mutex_lock(&camerasrc->snapshot_handle->thread_lock);
    GST_WARNING_OBJECT(camerasrc, "send signal to exit snapshot thread");
    camerasrc->snapshot_handle->thread_exit = TRUE;
    g_cond_signal(&camerasrc->snapshot_handle->thread_cond);
    g_mutex_unlock(&camerasrc->snapshot_handle->thread_lock);

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    /* remove raw capture thread */
    g_mutex_lock(&camerasrc->raw_capture_done_handle->thread_lock);
    GST_WARNING_OBJECT(camerasrc, "send signal to exit raw cb  thread");
    camerasrc->raw_capture_done_handle->thread_exit = TRUE;
    g_cond_signal(&camerasrc->raw_capture_done_handle->thread_cond);
    g_mutex_unlock(&camerasrc->raw_capture_done_handle->thread_lock);
#endif

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    g_mutex_lock(&camerasrc->preflash_thread_handle->thread_lock);
    GST_WARNING_OBJECT(camerasrc, "send signal to exit preflash thread");
    camerasrc->preflash_thread_handle->thread_exit = TRUE;
    g_cond_signal(&camerasrc->preflash_thread_handle->thread_cond);
    g_mutex_unlock(&camerasrc->preflash_thread_handle->thread_lock);
#endif

    if (VIDEO_IN_MODE_PREVIEW == camerasrc->mode) {
        stop_preview(camerasrc->preview_stream,camerasrc->enable_zsl_mode);
        camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;

        GST_WARNING_OBJECT(camerasrc, "done stop_preview");
    }
    close_camera_device(camerasrc->camera_handle);
    GST_WARNING_OBJECT(camerasrc, "close_camera_device done");
    camerasrc->ready_state = FALSE;

    /* join snapshot thread */
    g_thread_join(camerasrc->snapshot_handle->thread);
    camerasrc->snapshot_handle->thread = NULL;
    GST_WARNING_OBJECT(camerasrc, "snapshot thread joined");
    g_mutex_clear(&camerasrc->snapshot_handle->thread_lock);
    g_cond_clear(&camerasrc->snapshot_handle->thread_cond);
    free(camerasrc->snapshot_handle);
    camerasrc->snapshot_handle = NULL;

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    /* join raw raw_capture_done_handle thread */
    g_thread_join(camerasrc->raw_capture_done_handle->thread);
    camerasrc->raw_capture_done_handle->thread = NULL;
    GST_WARNING_OBJECT(camerasrc, "raw cb  thread joined");
    g_mutex_clear(&camerasrc->raw_capture_done_handle->thread_lock);
    g_cond_clear(&camerasrc->raw_capture_done_handle->thread_cond);
    free(camerasrc->raw_capture_done_handle);
    camerasrc->raw_capture_done_handle = NULL;
#endif

#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    /* join raw capture thread */
    g_thread_join(camerasrc->preflash_thread_handle->thread);
    camerasrc->preflash_thread_handle->thread = NULL;
    GST_WARNING_OBJECT(camerasrc, "preflash thread joined");
    g_mutex_clear(&camerasrc->preflash_thread_handle->thread_lock);
    g_cond_clear(&camerasrc->preflash_thread_handle->thread_cond);
    free(camerasrc->preflash_thread_handle);
    camerasrc->preflash_thread_handle = NULL;
#endif
    GST_WARNING_OBJECT(camerasrc, "LEAVED");

    return TRUE;
}

static gboolean gst_camerasrc_fill_ctrl_list(GstCameraSrc *camerasrc)
{
	int n = 0;
	camerasrc_ctrl_info_t ctrl_info;
	g_return_val_if_fail(camerasrc, FALSE);

	GST_DEBUG_OBJECT(camerasrc, "ENTERED");
	for (n = CAMERASRC_COLOR_CTRL_BRIGHTNESS; n < CAMERASRC_COLOR_CTRL_NUM; n++) {
		GstCameraSrcColorBalanceChannel *camerasrc_color_channel = NULL;
		GstColorBalanceChannel *color_channel = NULL;
		gint channel_type;

		memset(&ctrl_info, 0x0, sizeof(camerasrc_ctrl_info_t));

		switch (n) {
			case CAMERASRC_COLOR_CTRL_BRIGHTNESS:
			case CAMERASRC_COLOR_CTRL_CONTRAST:
			case CAMERASRC_COLOR_CTRL_WHITE_BALANCE:
			case CAMERASRC_COLOR_CTRL_COLOR_TONE:
			case CAMERASRC_COLOR_CTRL_SATURATION:
			case CAMERASRC_COLOR_CTRL_SHARPNESS:
				channel_type = INTERFACE_COLOR_BALANCE;
				break;
			default:
				channel_type = INTERFACE_NONE;
				continue;
		}

		if (channel_type == INTERFACE_COLOR_BALANCE) {

			camerasrc_color_channel = g_object_new(GST_TYPE_CAMERASRC_COLOR_BALANCE_CHANNEL, NULL);
			color_channel = GST_COLOR_BALANCE_CHANNEL(camerasrc_color_channel);

			color_channel->label = g_strdup((const gchar *)camerasrc_ctrl_label[n]);
			if(color_channel->label  == NULL)
				continue;
			camerasrc_color_channel->id = n;
			color_channel->min_value = ctrl_info.min;
			color_channel->max_value = ctrl_info.max;

			camerasrc->colors = g_list_append(camerasrc->colors, (gpointer)color_channel);
			GST_INFO_OBJECT(camerasrc, "Adding Color Balance Channel %s (%x)",
			                           color_channel->label, camerasrc_color_channel->id);
		}
	}

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");
	return TRUE;
}

static gboolean gst_camerasrc_empty_ctrl_list(GstCameraSrc *camerasrc)
{
	g_return_val_if_fail(camerasrc, FALSE);

	GST_DEBUG_OBJECT (camerasrc, "ENTERED");

	g_list_foreach(camerasrc->colors, (GFunc)g_object_unref, NULL);
	g_list_free(camerasrc->colors);
	camerasrc->colors = NULL;

	g_list_foreach(camerasrc->camera_controls, (GFunc)g_object_unref, NULL);
	g_list_free(camerasrc->camera_controls);
	camerasrc->camera_controls = NULL;

	GST_DEBUG_OBJECT(camerasrc, "LEAVED");

	return TRUE;
}
void gst_camerasrc_post_message_int(GstCameraSrc *camerasrc, const char *msg_name, const char *field_name, int value)
{
	GstMessage *m = NULL;
	GstStructure *s = NULL;

	if (!camerasrc || !msg_name || !field_name) {
		GST_ERROR("pointer is NULL %p, %p, %p", camerasrc, msg_name, field_name);
		return;
	}

	GST_INFO("post message [%s] %s %d", msg_name, field_name, value);

	s = gst_structure_new(msg_name, field_name, G_TYPE_INT, value, NULL);
	if (s == NULL) {
		GST_ERROR("gst_structure_new failed");
		//gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_ALLOCATION);
		return;
	}

	m = gst_message_new_element(GST_OBJECT(camerasrc), s);
	if (m == NULL) {
		GST_ERROR("gst_message_new_element failed");
		//gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_ALLOCATION);
		return;
	}

	gst_element_post_message(GST_ELEMENT(camerasrc), m);

	return;
}

static int gst_camerasrc_af_cb(void *usr_param,int state)
{
	GstCameraSrc *camerasrc = (GstCameraSrc *)usr_param;

	if (camerasrc->is_focusing) {
		gst_camerasrc_post_message_int(camerasrc, "camerasrc-AF", "focus-state", state);
	} else {
		GST_WARNING("not focusing status. skip focus message %d", state);
	}

	return CAMERASRC_SUCCESS;
}

static int camerasrc_get_face_detect_info(GstCameraSrc *camerasrc, camera_frame_metadata_t *metadata, GstCameraControlFaceDetectInfo **fd_info)
{
    int num_of_faces = 0;
    int i = 0;
    int rect_x = 0;
    int rect_y = 0;
    GstCameraControlFaceDetectInfo *fd_info_tmp = NULL;

    if(camerasrc == NULL || !fd_info){
        GST_ERROR("pointer[%p] or fd_info[%p] is NULL",
                    camerasrc, fd_info);
        return CAMERASRC_ERR_INVALID_PARAMETER;
    }

    if(metadata){
        num_of_faces = metadata->number_of_faces;
    }

    if(num_of_faces < 1 && !camerasrc->face_detected){
        *fd_info = NULL;
        return CAMERASRC_SUCCESS;
    }

    if(num_of_faces < 1){
        camerasrc->face_detected = FALSE;
    } else {
        camerasrc->face_detected = TRUE;
    }

    if(num_of_faces > MAX_FACE_NUM){
       GST_WARNING("too many faces are detected %d, set max %d", num_of_faces, MAX_FACE_NUM);
        num_of_faces = MAX_FACE_NUM;
    }

    fd_info_tmp = (GstCameraControlFaceDetectInfo *)malloc(sizeof(GstCameraControlFaceDetectInfo));
    if(!fd_info_tmp){
        GST_ERROR("fd_info alloc failed");
        *fd_info = NULL;
        return CAMERASRC_ERR_ALLOCATION;
    }

    memset(fd_info_tmp, 0x0, sizeof(GstCameraControlFaceDetectInfo));
    fd_info_tmp->num_of_faces = num_of_faces;

    if(metadata){
        for(i =0 ; i < num_of_faces ; i++) {
            fd_info_tmp->face_info[i].id = i;
            fd_info_tmp->face_info[i].score = metadata->faces[i].score;
            rect_x = metadata->faces[i].rect[0];
            rect_y = metadata->faces[i].rect[1];
            fd_info_tmp->face_info[i].rect.x = rect_x;
            fd_info_tmp->face_info[i].rect.y = rect_y;
            fd_info_tmp->face_info[i].rect.width = metadata->faces[i].rect[2];
            fd_info_tmp->face_info[i].rect.height = metadata->faces[i].rect[3];
        }
    }
    *fd_info = fd_info_tmp;
    return CAMERASRC_SUCCESS;
}

static void
gst_camerasrc_post_message_pointer(GstCameraSrc *camerasrc,
        const char *msg_name, const char *field_name, gpointer value)
{
        GstMessage *message = NULL;
        GstStructure *structure = NULL;
        structure = gst_structure_new(msg_name, field_name, G_TYPE_POINTER, value, NULL);
        message = gst_message_new_element(GST_OBJECT(camerasrc), structure);

        if(!gst_element_post_message(GST_ELEMENT(camerasrc), message)) {
            GST_ERROR("gst_element_post_message failed.");
            return;
        }
        GST_INFO("post face detect info successfully");
		return;
}

static void gst_camerasrc_post_face_detection_info (GstCameraSrc *camerasrc, camera_frame_metadata_t *metadata)
{
    GstCameraControlFaceDetectInfo *fd_info = NULL;
    if(camerasrc == NULL || metadata == NULL){
        GST_WARNING("pointer[%p] or metadata[%p] is NULL",
                    camerasrc, metadata);
        return ;
    }

    camerasrc_get_face_detect_info(camerasrc, metadata, &fd_info);

    /* post message if face is detected */
    if (fd_info) {
        gst_camerasrc_post_message_pointer(camerasrc, "camerasrc-FD", "face-info", (gpointer)fd_info);
    }
    return;
}

static int
gst_camerasrc_preview_metadata_cb(void *usr_param,camera_frame_metadata_t *metadata)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)usr_param;

    //GST_WARNING_OBJECT(camerasrc, "metadata->number_of_faces = %d",metadata->number_of_faces);
    if(CAMERASRC_FACE_DETECT_ON==camerasrc->face_detect){
        gst_camerasrc_post_face_detection_info (camerasrc, metadata);
    }
    /*Handle face detect result here as well*/
    return CAMERASRC_SUCCESS;
}

#ifdef RAW_CB_FOR_SHUTTER_SOUND
static int gst_camerasrc_raw_capture_done_cb(void *usr_param,int state)
{
    GstCameraSrc *camerasrc = (GstCameraSrc *)usr_param;
#ifndef CONFIG_CAMERA_ZSL_CAPTURE
    if (camerasrc->mode == VIDEO_IN_MODE_CAPTURE)
#endif
    {
        g_mutex_lock(&camerasrc->raw_capture_done_handle->thread_lock);
        GST_WARNING_OBJECT(camerasrc, "send raw_capture_done  signal");
        g_cond_signal(&camerasrc->raw_capture_done_handle->thread_cond);
        g_mutex_unlock(&camerasrc->raw_capture_done_handle->thread_lock);
    }
    return CAMERASRC_SUCCESS;
}
#endif

static gboolean
gst_camerasrc_start(GstCameraSrc *camerasrc)
{
    int set_flip = CAMERASRC_FLIP_NONE;
    gboolean result = FALSE;

    GST_WARNING_OBJECT(camerasrc, "ENTERED");

    if (camerasrc->is_start_failed) {
        GST_ERROR_OBJECT(camerasrc, "already error is occurred");
        return FALSE;
    }

    /*set focus callback function callback*/
#ifdef CONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
    camerasrc->preview_stream->af_cb = NULL;
#else
    camerasrc->preview_stream->af_cb = gst_camerasrc_af_cb;
#endif

    /*set preview metadata(lowlight/face detect) callback function callback*/
    camerasrc->preview_stream->preview_metadata_cb = gst_camerasrc_preview_metadata_cb;

#ifdef RAW_CB_FOR_SHUTTER_SOUND
    /*set rawcapture callback function callback*/
    camerasrc->snapshot_stream->raw_capture_cb = (raw_snapshot_done_cb_t)gst_camerasrc_raw_capture_done_cb;
#endif

    /* set Focus mode */
    gst_camerasrc_control_set_focus(camerasrc, camerasrc->focus_mode, camerasrc->focus_range);

    /* set snapshot resolution */
    set_snapshot_dimension(camerasrc->snapshot_stream, camerasrc->cap_width, camerasrc->cap_height);

    /* init number of live buffers */
    camerasrc->num_live_buffers = 0;
    /* init qbuf without display count */
    camerasrc->qbuf_without_display = 0;
    /* init condition of finalization for print log */
    camerasrc->finalizeLogCnt = DEBUG_CAMERALOG_DEFAULT_COUNT;

    /* init restart flag */
    camerasrc->snapshot_stream->restart_flag = FALSE;

    /* auto lls detect */
    GST_WARNING_OBJECT(camerasrc, "low_light_detection (%d)", camerasrc->low_light_detection);
    set_low_light_auto_detection((camerasrc_handle_t *)camerasrc->camera_handle, camerasrc->low_light_detection);

    /* night shot mode */
    GST_WARNING("camerasrc->shot_mode =%d",camerasrc->shot_mode);
    if(camerasrc->shot_mode == 1) {
       camerasrc->preview_stream->low_light_mode = 1;
    } else {
       camerasrc->preview_stream->low_light_mode = 0;
    }
    set_low_light_mode((camerasrc_handle_t *)camerasrc->camera_handle, camerasrc->preview_stream->low_light_mode);

#ifdef DUAL_CAMERA
    /*set dual camera mode*/
    GST_INFO("set_dual_camera_mode");
    set_dual_camera_mode(camerasrc, camerasrc->dual_camera_mode);
#endif

    /*set vt mode*/
    gst_camerasrc_set_camera_control(camerasrc,
                    CAMERASRC_PARAM_CONTROL_VT_MODE,
                    camerasrc->vt_mode,0,NULL);

    /* set flip */
    if (camerasrc->vflip && camerasrc->hflip) {
        set_flip = CAMERASRC_FLIP_BOTH;
    } else if (camerasrc->hflip) {
        set_flip = CAMERASRC_FLIP_HORIZONTAL;
    } else if (camerasrc->vflip) {
        set_flip = CAMERASRC_FLIP_VERTICAL;
    }

    GST_WARNING_OBJECT(camerasrc, "hflip %d, vflip %d, set_flip %d",
                                  camerasrc->hflip, camerasrc->vflip, set_flip);

   // set_preview_flip(camerasrc->preview_stream, set_flip);
   send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_SET_FRONT_MIRROR, set_flip, 0);

    if (camerasrc->enable_hybrid_mode) {
        GST_WARNING_OBJECT(camerasrc, "start RECORDING STREAM");
        GST_INFO("prepare_record");
        prepare_record(camerasrc->record_stream);
        camerasrc->preview_stream->recording_hint = TRUE;
    } else {
        GST_WARNING_OBJECT(camerasrc, "start PREVIEW STREAM");
        GST_INFO("unprepare_record");
        unprepare_record(camerasrc->record_stream);
        camerasrc->preview_stream->recording_hint = FALSE;
    }

    /* init is_focusing */
#ifdef CONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
    camerasrc->is_focusing = FALSE;
#else
    camerasrc->is_focusing = TRUE;
#endif

    // HDR SET if preset mode and postset mode is different
    GST_WARNING_OBJECT(camerasrc,"gst_camerasrc_start pre hdr mode = %d , hdr mode = %d" ,camerasrc->snapshot_stream->m_pre_hdr_mode,camerasrc->snapshot_stream->m_hdr_mode );
    if (camerasrc->snapshot_stream->m_hdr_mode != camerasrc->snapshot_stream->m_pre_hdr_mode){
        send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_HDR_MODE, camerasrc->snapshot_stream->m_hdr_mode, 0);
        camerasrc->snapshot_stream->m_pre_hdr_mode = camerasrc->snapshot_stream->m_hdr_mode;
        GST_WARNING_OBJECT(camerasrc,"HDR mode set as %d",camerasrc->snapshot_stream->m_hdr_mode);
    }

    send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_SET_CAMERA_ACCESS_MODE, camerasrc->camera_cpu_clock_lock, 0);

    // Set Jpeg Quality
    gst_camerasrc_set_camera_control(camerasrc,
                    CAMERASRC_PARAM_CONTROL_JPEG_QUALITY,
                    camerasrc->cap_jpg_quality,0,NULL);

    gst_camerasrc_set_camera_control(camerasrc,
                         CAMERASRC_PARAM_CONTROL_SNAPSHOT_FORMAT,
                         camerasrc->snapshot_stream->snapshot_format,
                         0,
                         NULL);

    send_command(camerasrc->preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_SET_SAMSUNG_CAMERA, 0, 0);

    settings_batch_update_from_cache(camerasrc);

    GST_WARNING("start_preview");
    camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
    result = start_preview(camerasrc->preview_stream,camerasrc->enable_zsl_mode);
    if (!result) {
        GST_ERROR_OBJECT( camerasrc, "start_preview fails");
        camerasrc->mode =VIDEO_IN_MODE_UNKNOWN;
        camerasrc->is_start_failed = TRUE;
        gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_INTERNAL);
    } else {
        camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
        camerasrc->firsttime = TRUE;
    }

    GST_WARNING_OBJECT(camerasrc, "LEAVED - result %d", result);

    return result;
}


static gboolean
gst_camerasrc_stop(GstCameraSrc *camerasrc)
{
    GST_WARNING_OBJECT (camerasrc, "ENTERED - mode %d", camerasrc->mode);

    if (VIDEO_IN_MODE_UNKNOWN != camerasrc->mode) {
        camerasrc->mode = VIDEO_IN_MODE_STOP;

        /* release remained buffer in queue */
        GST_INFO("STOP_PREVIEW:gst_camerasrc_release_all_queued_preview_buffer");
        gst_camerasrc_release_all_queued_preview_buffer(camerasrc);

        /* wait all preview buffer finalized for previiew restart */
        GST_INFO("STOP_PREVIEW:gst_camerasrc_wait_all_preview_buffer_finalized");
        gst_camerasrc_wait_all_preview_buffer_finalized(camerasrc);

        /* stop preview and release buffer */
        GST_INFO("STOP_PREVIEW:stop_preview");
        stop_preview(camerasrc->preview_stream,camerasrc->enable_zsl_mode);

        camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;
    }

    GST_WARNING_OBJECT (camerasrc, "LEAVED");

    return TRUE;
}

static gboolean gst_camerasrc_process_cmd(GstCameraSrc *camerasrc, int cmd)
{
    if (!camerasrc){
        GST_ERROR_OBJECT(camerasrc, "NULL handle");
        return FALSE;
    }

    switch (cmd) {
    case GST_CAMERASRC_CMD_PREVIEW_RESTART:
        GST_WARNING_OBJECT(camerasrc, "GST_CAMERASRC_CMD_PREVIEW_RESTART was called");
        /* stop preview */
        gst_camerasrc_stop(camerasrc);

        if((camerasrc->snapshot_stream->longshot_mode == FALSE)
            &&(camerasrc->snapshot_stream->capture_num > 1)
            &&(camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_OFF)
            ){
            /*WARNING:Cannot set interval after ZSL stream has started.Will add it in later.*/
            int skip_frame = camerasrc->snapshot_stream->capture_interval/50;
            if (skip_frame < 1) {
                 skip_frame = 1;
            }
            GST_WARNING_OBJECT(camerasrc, "burst capture num=%d and skip_frame=%d",
                                                camerasrc->snapshot_stream->capture_num,skip_frame);
            gst_camerasrc_set_camera_control(camerasrc,
                                    CAMERASRC_PARAM_CONTROL_BURST_CONFIG,
                                    camerasrc->snapshot_stream->capture_num,
                                    skip_frame,
                                    NULL);
        }

        /* start preview */
        gst_camerasrc_start(camerasrc);

        /* completed signal */
        g_cond_signal(camerasrc->cmd_cond);
        g_cond_signal(&camerasrc->snapshot_stream->cmd_cond);
        GST_WARNING_OBJECT(camerasrc, "camerasrc->snapshot_stream->cmd_cond was signaled");
        break;
    case GST_CAMERASRC_CMD_RECORD_START:
        if (!camerasrc->enable_hybrid_mode) {
            /* set mode as UNKNOWN to stop preview */
            camerasrc->mode = VIDEO_IN_MODE_STOP;

            /* release buffer in queue */
            GST_INFO("RECORDING_START:gst_camerasrc_release_all_queued_preview_buffer");
            gst_camerasrc_release_all_queued_preview_buffer(camerasrc);

            /* wait all preview buffer finalized for previiew restart */
            GST_INFO("RECORDING_START:gst_camerasrc_wait_all_preview_buffer_finalized");
            gst_camerasrc_wait_all_preview_buffer_finalized(camerasrc);
        }

        GST_WARNING_OBJECT(camerasrc, "start video stream - resolution [%dx%d]",
                                      camerasrc->record_stream->record_width,
                                      camerasrc->record_stream->record_height);

        camerasrc->firsttime_record = TRUE;
        camerasrc->num_video_buffers = 0;


        /* set mode as VIDEO */
        camerasrc->mode = VIDEO_IN_MODE_VIDEO;

        GST_INFO("		camerasrc:start_record");
        if(!start_record(camerasrc->record_stream, !camerasrc->enable_hybrid_mode))
        {
                GST_ERROR_OBJECT(camerasrc, "start_record is failed.");
                gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_INTERNAL);
        }


        /* completed signal */
        g_cond_signal(camerasrc->cmd_cond);
        break;
    case GST_CAMERASRC_CMD_RECORD_STOP:
        GST_WARNING_OBJECT(camerasrc, "stop record in read_frame");
        if (!camerasrc->enable_hybrid_mode) {
            /* set mode as UNKNOWN to stop preview */
            camerasrc->mode = VIDEO_IN_MODE_STOP;

            /* release buffer in queue */
            GST_INFO("RECORDING_STOP:gst_camerasrc_release_all_queued_preview_buffer");
            gst_camerasrc_release_all_queued_preview_buffer(camerasrc);

            /* wait all preview buffer finalized for previiew restart */
            GST_INFO("RECORDING_STOP:gst_camerasrc_wait_all_preview_buffer_finalized");
            gst_camerasrc_wait_all_preview_buffer_finalized(camerasrc);

        } else {
            /* set mode as PREVIEW */
            camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
        }

        /* wait for video buffer finalized */
        GST_INFO("RECORDING_STOP:gst_camerasrc_wait_all_video_buffer_finalized");
        gst_camerasrc_wait_all_video_buffer_finalized(camerasrc);


        GST_INFO("RECORDING_STOP:stop_record");
        if(!stop_record(camerasrc->record_stream, !camerasrc->enable_hybrid_mode))
        {
                GST_ERROR_OBJECT(camerasrc, "stop_record is failed.");
                gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_INTERNAL);
        }

        GST_WARNING_OBJECT(camerasrc, "stop record done in read_frame");

        /* completed signal */
        g_cond_signal(camerasrc->cmd_cond);

        /* set mode as PREVIEW */
        camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
        break;
    case GST_CAMERASRC_CMD_CAPTURE_START:
        GST_INFO("capture_snapshot start");
        capture_snapshot (camerasrc);
        break;
    case GST_CAMERASRC_CMD_CAPTURE_STOP:
        /*Not handled now*/
        break;
    default:
        break;
    }

    return TRUE;
}

static int
cache_settings_for_batch_update(GstCameraSrc* camerasrc,int64_t control_type,int value_1,int value_2,char* string_1){
    if(!camerasrc->batch_control_value_cache){
        camerasrc->batch_control_value_cache=(camerasrc_batch_ctrl_t*)malloc(sizeof(camerasrc_batch_ctrl_t));
        memset(camerasrc->batch_control_value_cache, 0, sizeof (camerasrc_batch_ctrl_t));
        camerasrc->batch_control_id_cache=CAMERASRC_PARAM_CONTROL_START;
    }
    camerasrc->batch_control_id_cache=camerasrc->batch_control_id_cache|control_type;
    camerasrc->setting_value_cached=TRUE;
    switch(control_type)
    {
        case CAMERASRC_PARAM_CONTROL_PREVIEW_SIZE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PREVIEW_SIZE %dx%d", value_1, value_2);
            camerasrc->batch_control_value_cache->preview_width=value_1;
            camerasrc->batch_control_value_cache->preview_height=value_2;
            break;
        case CAMERASRC_PARAM_CONTROL_PREVIEW_FORMAT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PREVIEW_FORMAT %d", value_1);
            camerasrc->batch_control_value_cache->preview_format=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_VIDEO_SIZE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_VIDEO_SIZE %dx%d", value_1, value_2);
            camerasrc->batch_control_value_cache->video_width=value_1;
            camerasrc->batch_control_value_cache->video_height=value_2;
            break;
        case CAMERASRC_PARAM_CONTROL_VIDEO_FORMAT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_VIDEO_FORMAT %d", value_1);
            camerasrc->batch_control_value_cache->video_format=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_RECORDING_HINT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_RECORDING_HINT %d %d", value_1, value_2);
            camerasrc->batch_control_value_cache->recording_hint=value_1;
            camerasrc->batch_control_value_cache->hfr_value=value_2;
            break;
        case CAMERASRC_PARAM_CONTROL_SNAPSHOT_SIZE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SNAPSHOT_SIZE %dx%d", value_1, value_2);
            camerasrc->batch_control_value_cache->snapshot_width=value_1;
            camerasrc->batch_control_value_cache->snapshot_height=value_2;
            break;
        case CAMERASRC_PARAM_CONTROL_SNAPSHOT_FORMAT:
            camerasrc->batch_control_value_cache->snapshot_format=value_1;
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SNAPSHOT_FORMAT %d", value_1);
            break;
        case CAMERASRC_PARAM_CONTROL_FPS_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_FPS_MODE %d", value_1);
            camerasrc->batch_control_value_cache->fps_mode=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_FPS:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_FPS %d", value_1);
            camerasrc->batch_control_value_cache->fps = value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_BURST_CONFIG:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_BURST_CONFIG %d %d", value_1, value_2);
            camerasrc->batch_control_value_cache->burst_number=value_1;
            camerasrc->batch_control_value_cache->burst_interval=value_2;
            break;
        case CAMERASRC_PARAM_CONTROL_FACE_DETECT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_FACE_DETECT %d", value_1);
            camerasrc->batch_control_value_cache->face_detect=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_FLASH_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_FLASH_MODE %d", value_1);
            camerasrc->batch_control_value_cache->flash_mode=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_BRIGHTNESS:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_BRIGHTNESS %d", value_1);
            camerasrc->batch_control_value_cache->brightness_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_CONTRAST:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_CONTRAST %d", value_1);
            camerasrc->batch_control_value_cache->contrast_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_COLOR_TONE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_COLOR_TONE %d", value_1);
            camerasrc->batch_control_value_cache->color_tone_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PROGRAM_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PROGRAM_MODE %d", value_1);
            camerasrc->batch_control_value_cache->program_mode=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PARTCOLOR_SRC:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PARTCOLOR_SRC %d", value_1);
            camerasrc->batch_control_value_cache->partcolor_src=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PARTCOLOR_DST:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PARTCOLOR_DST %d", value_1);
            camerasrc->batch_control_value_cache->partcolor_dest=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PARTCOLOR_MODE:
            camerasrc->batch_control_value_cache->partcolor_mode=value_1;
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PARTCOLOR_MODE %d", value_1);
            break;
        case CAMERASRC_PARAM_CONTROL_WIDE_DYNAMIC_RANGE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_WIDE_DYNAMIC_RANGE %d", value_1);
            camerasrc->batch_control_value_cache->wdr_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_SATURATION:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SATURATION %d", value_1);
            camerasrc->batch_control_value_cache->saturation_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_SHARPNESS:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SHARPNESS %d", value_1);
            camerasrc->batch_control_value_cache->sharpness_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PHOTOMETRY:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PHOTOMETRY %d", value_1);
            camerasrc->batch_control_value_cache->photometry_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_METERING:
            camerasrc->batch_control_value_cache->metering_value=value_1;
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_METERING %d", value_1);
            break;
        case CAMERASRC_PARAM_CONTROL_WB:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_WB %d", value_1);
            camerasrc->batch_control_value_cache->wb_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_ISO:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_ISO %d", value_1);
            camerasrc->batch_control_value_cache->iso_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_ANTI_SHAKE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_ANTI_SHAKE %d", value_1);
            camerasrc->batch_control_value_cache->antishake_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_LOW_LIGHT_DETECTION:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_LOW_LIGHT_DETECTION %d", value_1);
            camerasrc->batch_control_value_cache->lld_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_BURST_SHOT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_BURST_SHOT %d", value_1);
            camerasrc->batch_control_value_cache->burst_shot_config=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_ZOOM:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_ZOOM %d", value_1);
            camerasrc->batch_control_value_cache->zoom_value=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_AF:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_AF %d", value_1);
            camerasrc->batch_control_value_cache->af_config=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_AF_AREAS:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_AF_AREAS %s", string_1);
            if(camerasrc->batch_control_value_cache->af_area){
                free(camerasrc->batch_control_value_cache->af_area);
            }
            camerasrc->batch_control_value_cache->af_area=(char*)malloc(strlen(string_1) + 1);
            memcpy(camerasrc->batch_control_value_cache->af_area,string_1,strlen(string_1));
            break;
        case CAMERASRC_PARAM_CONTROL_SCENE_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SCENE_MODE %d", value_1);
            camerasrc->batch_control_value_cache->scene_mode=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_HDR:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_HDR %d", value_1);
            camerasrc->batch_control_value_cache->hdr_ae_bracket=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_PREVIEW_FLIP:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_PREVIEW_FLIP %d", value_1);
            camerasrc->batch_control_value_cache->preview_flip=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_VIDEO_FLIP:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_VIDEO_FLIP %d", value_1);
            camerasrc->batch_control_value_cache->video_flip=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_SNAPSHOT_FLIP:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SNAPSHOT_FLIP %d", value_1);
            camerasrc->batch_control_value_cache->snapshot_flip=value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_SHOOTING_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_SHOOTING_MODE %d", value_1);
            camerasrc->batch_control_value_cache->shoting_mode=value_1;
            break;
#ifdef DUAL_CAMERA
        case CAMERASRC_PARAM_CONTROL_DUAL_CAMERA_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_DUAL_CAMERA_MODE %d", value_1);
            camerasrc->batch_control_value_cache->dual_cam_mode=value_1;
            break;
#endif
#ifdef VDIS
        case CAMERASRC_PARAM_CONTROL_VDIS:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_VDIS %d", value_1);
            camerasrc->batch_control_value_cache->vdis_mode=value_1;
            break;
#endif
        case CAMERASRC_PARAM_CONTROL_JPEG_QUALITY:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_JPEG_QUALITY %d", value_1);
            camerasrc->batch_control_value_cache->jpeg_quality=value_1;
            break;
#ifdef DUAL_CAMERA
        case CAMERASRC_PARAM_CONTROL_DUAL_RECORDING_HINT:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_DUAL_RECORDING_HINT %d", value_1);
            camerasrc->batch_control_value_cache->dual_recording_hint=value_1;
            break;
#endif
        case CAMERASRC_PARAM_CONTROL_AE_LOCK:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_AE_LOCK %d", value_1);
            camerasrc->batch_control_value_cache->ae_lock = value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_AWB_LOCK:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_AWB_LOCK %d", value_1);
            camerasrc->batch_control_value_cache->awb_lock = value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_VT_MODE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_VT_MODE %d", value_1);
            camerasrc->batch_control_value_cache->vt_mode = value_1;
            break;
        case CAMERASRC_PARAM_CONTROL_FPS_RANGE:
            GST_INFO_OBJECT(camerasrc, "CAMERASRC_PARAM_CONTROL_FPS_RANGE %d %d", value_1,value_2);
            camerasrc->batch_control_value_cache->fps_min=value_1;
            camerasrc->batch_control_value_cache->fps_max=value_2;
            break;
        default:
            GST_ERROR_OBJECT(camerasrc, "Invalid setting : 0x%x",(unsigned int) control_type);
            break;
    }
    return CAMERASRC_SUCCESS;
}

static int
settings_batch_update_from_cache(GstCameraSrc* camerasrc){
    GST_WARNING_OBJECT(camerasrc, "ENTER - value cached %d", camerasrc->setting_value_cached);
    if(camerasrc->setting_value_cached){
        set_camera_launch_setting((camerasrc_handle_t *)camerasrc->camera_handle,
                                                        camerasrc->batch_control_id_cache,
                                                        camerasrc->batch_control_value_cache);
        camerasrc->setting_value_cached=FALSE;
        camerasrc->batch_control_id_cache=CAMERASRC_PARAM_CONTROL_START;
    }
    GST_WARNING_OBJECT(camerasrc, "LEAVE");
    return CAMERASRC_SUCCESS;
}

static GstFlowReturn
gst_camerasrc_read_frame(GstCameraSrc *camerasrc,GstBuffer **buffer)
{
    GstFlowReturn ret=GST_FLOW_OK;
    GstBuffer *black_frame_buffer = NULL;
    int black_buf_len = 0;
    int cmd = GST_CAMERASRC_CMD_NONE;
    GstMemory *img_buf_memory = NULL;
    GstMapInfo mapInfo;
    GstBuffer *temp_buffer;
    /* check command */
    g_mutex_lock(camerasrc->cmd_lock);
    if (!g_queue_is_empty(camerasrc->cmd_list)) {
        /* process cmd */
        cmd = (int)g_queue_pop_head(camerasrc->cmd_list);
        gst_camerasrc_process_cmd(camerasrc, cmd);
    }
    g_mutex_unlock(camerasrc->cmd_lock);

    switch (camerasrc->mode) {
        case VIDEO_IN_MODE_PREVIEW:
        case VIDEO_IN_MODE_VIDEO:
            GST_DEBUG_OBJECT(camerasrc, "Getting mutex in thread %p",g_thread_self ());
            get_preview_stream_mutex (camerasrc->preview_stream);
            GST_DEBUG_OBJECT(camerasrc, "Got mutex in thread %p",g_thread_self ());

            wait_for_frame (camerasrc->preview_stream);
            if (frame_available(camerasrc->preview_stream)){
                *buffer = get_preview_frame (camerasrc->preview_stream);
                camerasrc->frame_count = 0;
#ifdef ENABLE_ZERO_COPY
                if(gst_camerasrc_is_zero_copy_format(camerasrc->format_str)){
                    MMVideoBuffer *img_buffer = NULL;
                    if (gst_buffer_n_memory(*buffer) > 1){
                        img_buf_memory = gst_buffer_peek_memory(*buffer,1);
                        gst_memory_map(img_buf_memory,&mapInfo,GST_MAP_WRITE);
                        img_buffer = (MMVideoBuffer *)mapInfo.data;
                    }

                    if (img_buffer) {
                        GST_DEBUG_OBJECT(camerasrc, "Passing Frame[bo %p] size [%dx%d]",
                                                    img_buffer->handle.bo[0], img_buffer->width[0], img_buffer->height[0]);

                        if (camerasrc->snapshot_stream->restart_flag) {
                            GST_WARNING_OBJECT(camerasrc, "make FLUSH_BUFFER");

                            img_buffer->flush_request = TRUE;
                            camerasrc->snapshot_stream->restart_flag = FALSE;
                            g_queue_push_tail(camerasrc->cmd_list, (gpointer) GST_CAMERASRC_CMD_PREVIEW_RESTART);
                            gst_memory_unmap(img_buf_memory,&mapInfo);
                        } else if(camerasrc->received_capture_command){
                            if(camerasrc->last_buffer_cache){
                                    GST_WARNING_OBJECT(camerasrc, "make dummy FLUSH_BUFFER");
                                    camerasrc->last_buffer_cache->flush_request = TRUE;
                                    temp_buffer = *buffer ;
                                    *buffer = gst_buffer_new();
                                    gst_buffer_unref(temp_buffer);
                                    GstMapInfo map;
                                    gst_buffer_map(*buffer,&map,GST_MAP_WRITE);
                                    map.data = camerasrc->last_buffer_cache->data[0];
                                    map.size = ((camerasrc->width * camerasrc->height * 3) >> 1);
                                    gst_buffer_unmap(*buffer,&map);

                                    gst_buffer_append_memory(*buffer, gst_memory_new_wrapped(0,
                                                                        camerasrc->last_buffer_cache->data[0],
                                                                        (camerasrc->width * camerasrc->height * 3) >> 1,
                                                                        0,
                                                                        (camerasrc->width * camerasrc->height * 3) >> 1,NULL,NULL));

                                    gst_buffer_append_memory(*buffer, gst_memory_new_wrapped(0,
                                                                        camerasrc->last_buffer_cache, sizeof(*camerasrc->last_buffer_cache), 0,
                                                                        sizeof(*camerasrc->last_buffer_cache), camerasrc->last_buffer_cache,free));


                                    camerasrc->received_capture_command = FALSE;
                                    /*gst_buffer_unref will free GST_BUFFER_MALLOCDATA so allocate new*/
                                    camerasrc->last_buffer_cache = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));
                                    gst_memory_unmap(img_buf_memory,&mapInfo);


                                    g_mutex_lock(camerasrc->cmd_lock);
                                    g_queue_push_tail(camerasrc->cmd_list,(gpointer)GST_CAMERASRC_CMD_CAPTURE_START);
                                    g_mutex_unlock(camerasrc->cmd_lock);
                            }
                       }else {
                            gst_memory_unmap(img_buf_memory,&mapInfo);
                       }
                       if(camerasrc->last_buffer_cache && img_buffer){
                            memcpy(camerasrc->last_buffer_cache,img_buffer,sizeof(MMVideoBuffer));
                       }
                    }
                }
#endif
            } else if (camerasrc->received_capture_command) {
                if(camerasrc->last_buffer_cache){

                    GST_WARNING_OBJECT(camerasrc, "make dummy FLUSH_BUFFER");
                    camerasrc->last_buffer_cache->flush_request = TRUE;
                    *buffer = gst_buffer_new();
                    GstMapInfo map;
                    gst_buffer_map(*buffer,&map,GST_MAP_WRITE);
                    map.data = camerasrc->last_buffer_cache->data[0];
                    map.size = ((camerasrc->width * camerasrc->height * 3) >> 1);
                    gst_buffer_unmap(*buffer,&map);

                    gst_buffer_append_memory(*buffer, gst_memory_new_wrapped(0,
                                                camerasrc->last_buffer_cache->data[0],
                                                (camerasrc->width * camerasrc->height * 3) >> 1,
                                                0,
                                                (camerasrc->width * camerasrc->height * 3) >> 1,NULL,NULL));

                    gst_buffer_append_memory(*buffer, gst_memory_new_wrapped(0,
                                                camerasrc->last_buffer_cache, sizeof(*camerasrc->last_buffer_cache), 0,
                                                sizeof(*camerasrc->last_buffer_cache),camerasrc->last_buffer_cache,free));


                    camerasrc->received_capture_command = FALSE;
                    /*gst_buffer_unref will free GST_BUFFER_MALLOCDATA so allocate new*/
                    camerasrc->last_buffer_cache = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));

                    g_mutex_lock(camerasrc->cmd_lock);
                    g_queue_push_tail(camerasrc->cmd_list,(gpointer)GST_CAMERASRC_CMD_CAPTURE_START);
                    g_mutex_unlock(camerasrc->cmd_lock);
                }
            } else{
                GST_WARNING_OBJECT(camerasrc, "Failed to retrive preview frame. frame_count(%d), num live buffers (%d)",
                                              camerasrc->frame_count, camerasrc->num_live_buffers);
                camerasrc->frame_count++;
            }

            release_preview_stream_mutex(camerasrc->preview_stream);
            GST_DEBUG_OBJECT(camerasrc, "mutex released in thread %p",g_thread_self ());
            if (_MAX_TRIAL_WAIT_FRAME == camerasrc->frame_count) {
                GST_ERROR_OBJECT(camerasrc, "Could not get preview frame more than 3 sec. - num live buffers %d",
                                            camerasrc->num_live_buffers);
                if (camerasrc->firsttime) {
                    gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_IO_CONTROL);
                } else {
                    gst_camerasrc_error_handler(camerasrc, CAMERASRC_ERR_UNAVAILABLE_DEVICE);
                }
                ret = GST_FLOW_ERROR;
                camerasrc->frame_count = 0;
            }
        break;
        case VIDEO_IN_MODE_CAPTURE:
        {
            int capture_command = GST_CAMERA_CONTROL_CAPTURE_COMMAND_NONE;
            g_mutex_lock(camerasrc->restart_camera_lock);
            GST_WARNING_OBJECT(camerasrc, "wait for signal");
            g_cond_wait(camerasrc->restart_camera_cond, camerasrc->restart_camera_lock);
            GST_WARNING_OBJECT(camerasrc, "signal received");
            g_mutex_unlock(camerasrc->restart_camera_lock);
            if(camerasrc->cap_hdr == 0) {
                if(!start_preview(camerasrc->preview_stream,camerasrc->enable_zsl_mode)){
                    GST_WARNING_OBJECT( camerasrc, "start_preview failed");
                    ret = GST_FLOW_ERROR;
                }
            }
            camerasrc->frame_count = 0;
            camerasrc->firsttime = TRUE;
            camerasrc->finalizeLogCnt = DEBUG_CAMERALOG_DEFAULT_COUNT; /* When restart preview after capture, reset log count. */
            camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
        }
        break;
        case VIDEO_IN_MODE_UNKNOWN:
            if(!gst_camerasrc_is_zero_copy_format(camerasrc->format_str)){
                black_buf_len = camerasrc->preview_stream->preview_width * camerasrc->preview_stream->preview_height * 3 /2;
                black_frame_buffer = gst_buffer_new_and_alloc(black_buf_len);
                GstMapInfo map;
                gst_buffer_map(black_frame_buffer,&map,GST_MAP_WRITE);
                GenerateYUV420BlackFrame(map.data,
                                        black_buf_len,
                                        camerasrc->preview_stream->preview_width ,
                                        camerasrc->preview_stream->preview_height);
                gst_buffer_unmap(black_frame_buffer,&map);

                *buffer = black_frame_buffer;
            }
        break;
        default:
            ret = GST_FLOW_ERROR;
            GST_ERROR_OBJECT (camerasrc, "can't reach statement.[camerasrc->mode=%d]", camerasrc->mode);
        break;
    }

    if (!buffer || !(*buffer) || !GST_IS_BUFFER(*buffer)) {
        /* To avoid seg fault, make dummy buffer. */
        GST_WARNING_OBJECT(camerasrc, "Make a dummy buffer");
        if(gst_camerasrc_is_zero_copy_format(camerasrc->format_str)){

            *buffer = gst_buffer_new();
            GstMapInfo map;
            gst_buffer_map(*buffer,&map,GST_MAP_WRITE);
            map.data = NULL;
            map.size = 0;
            gst_buffer_unmap(*buffer,&map);
        }else{
            /*VT call uses non-zero copy format(NV21) data from camera.
            Not all gst-plugins used in VT pipeline handle NULL GST buffer well*/
            black_buf_len = camerasrc->preview_stream->preview_width * camerasrc->preview_stream->preview_height * 3 /2;
            *buffer = gst_buffer_new_and_alloc(black_buf_len);
            GstMapInfo map;
            gst_buffer_map(*buffer,&map,GST_MAP_WRITE);
            GenerateYUV420BlackFrame(map.data,
                                                black_buf_len,
                                                camerasrc->preview_stream->preview_width ,
                                                camerasrc->preview_stream->preview_height);

            map.size = black_buf_len;
            gst_buffer_unmap(*buffer,&map);
        }
    }
    return ret;
}
/*********************************GST functions***********************************/

static void
gst_camerasrc_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
    GstCameraSrc *camerasrc;
    GstState current_state = GST_STATE_NULL;
    g_return_if_fail (GST_IS_CAMERASRC (object));
    camerasrc = GST_CAMERASRC (object);

    switch (prop_id) {
        case PROP_CAMERA_HIGH_SPEED_FPS:
            camerasrc->high_speed_fps = g_value_get_int (value);
            if (camerasrc->record_stream) {
                camerasrc->record_stream->hfr_mode = camerasrc->high_speed_fps;
            }
        break;
        case PROP_CAMERA_AUTO_FPS:
            camerasrc->fps_auto = g_value_get_boolean (value);
        break;
        case PROP_CAMERA_ID:
            camerasrc->camera_id  = g_value_get_int(value);
        break;
        case PROP_CAMERA_CAPTURE_FOURCC:
            camerasrc->cap_fourcc = g_value_get_uint (value);
        break;
        case PROP_CAMERA_CAPTURE_QUALITY:
            camerasrc->cap_quality = g_value_get_enum (value);
        break;
        case PROP_CAMERA_CAPTURE_WIDTH:
            camerasrc->prev_cap_width = camerasrc->cap_width;
            camerasrc->cap_width = g_value_get_int (value);
        break;
        case PROP_CAMERA_CAPTURE_HEIGHT:
            if(camerasrc->cap_height == g_value_get_int(value) && camerasrc->cap_width == camerasrc->prev_cap_width)
            {
                GST_WARNING_OBJECT(camerasrc, "height&width same as previous , need not restart preview,");
                break;
            }
            camerasrc->cap_height = g_value_get_int (value);
#ifdef CONFIG_CAMERA_ZSL_CAPTURE
            gst_element_get_state(GST_ELEMENT(camerasrc), &current_state, NULL, 0);
            GST_INFO("current_state %d", current_state);
            if (current_state == GST_STATE_PLAYING) {
                g_mutex_lock(camerasrc->cmd_lock);
                GST_WARNING_OBJECT(camerasrc, "push PREVIEW_RESTART cmd by Capture Height");
                camerasrc->snapshot_stream->restart_flag = TRUE;
                g_cond_wait(camerasrc->cmd_cond, camerasrc->cmd_lock);
                GST_WARNING_OBJECT(camerasrc, "cmd_cond was recieved and restart complete from Capture Height");
                g_mutex_unlock(camerasrc->cmd_lock);
            }
#endif
        break;
        case PROP_CAMERA_CAPTURE_INTERVAL:
            camerasrc->cap_interval = g_value_get_int (value);
        break;
        case PROP_CAMERA_CAPTURE_COUNT:
            camerasrc->cap_count = g_value_get_int (value);
        break;
        case PROP_CAMERA_CAPTURE_JPG_QUALITY:
            camerasrc->cap_jpg_quality = g_value_get_int (value);
            gst_element_get_state(GST_ELEMENT(camerasrc), &current_state, NULL, 0);
            GST_INFO("current_state %d", current_state);
            if (current_state == GST_STATE_PLAYING && camerasrc->snapshot_stream
                    && camerasrc->snapshot_stream->camera_handle) {
                gst_camerasrc_set_camera_control(camerasrc,
                        CAMERASRC_PARAM_CONTROL_JPEG_QUALITY,
                        camerasrc->cap_jpg_quality,0,NULL);
            }
        break;
        case PROP_CAMERA_CAPTURE_PROVIDE_EXIF:
            GST_WARNING_OBJECT(camerasrc, "can not set provide-exif");
        break;
        case PROP_CAMERA_CAPTURE_HDR:
            GST_INFO("hdr property set");
            camerasrc->cap_hdr = g_value_get_int(value);
            if (camerasrc->cap_hdr == CAMERASRC_HDR_CAPTURE_OFF) {
               GST_WARNING("hdr property set: cap_hdr=0 ");
                camerasrc->snapshot_stream->m_hdr_mode = 0;//CAM_EXP_BRACKETING_OFF;
                camerasrc->snapshot_stream->m_bracket_buffer_count = 0;
                gst_camerasrc_set_camera_control(camerasrc,CAMERASRC_PARAM_CONTROL_SHOOTING_MODE,CAMERA_SHOT_MODE_NORMAL,0,NULL);
                gst_camerasrc_set_camera_control(camerasrc,CAMERASRC_PARAM_CONTROL_FLASH_MODE,camerasrc->snapshot_stream->flash_mode,0,NULL);
            } else {
                GST_WARNING("hdr property set: cap_hdr=1 ");
                camerasrc->snapshot_stream->m_hdr_mode = 1;//CAM_EXP_BRACKETING_ON;
                gst_camerasrc_set_camera_control(camerasrc,CAMERASRC_PARAM_CONTROL_SHOOTING_MODE,CAMERA_SHOT_MODE_HDR,0,NULL);
                gst_camerasrc_set_camera_control(camerasrc,CAMERASRC_PARAM_CONTROL_FLASH_MODE,0,0,NULL);
            }
            // different preset with postset : set restart preview

            if (camerasrc->snapshot_stream->m_pre_hdr_mode != camerasrc->snapshot_stream->m_hdr_mode){
                GST_INFO("m_pre_hdr_mode != m_hdr_mode ");
                gst_element_get_state(GST_ELEMENT(camerasrc), &current_state, NULL, 0);
                GST_INFO("current_state %d", current_state);
                if (current_state == GST_STATE_PLAYING) {
                   GST_INFO("state is playing");
                   g_mutex_lock(camerasrc->cmd_lock);
                   GST_DEBUG_OBJECT(camerasrc, "push GST_CAMERASRC_CMD_PROCESS_HDR cmd");
                   g_queue_push_tail(camerasrc->cmd_list, (gpointer)GST_CAMERASRC_CMD_PREVIEW_RESTART);
                   g_cond_wait(camerasrc->cmd_cond, camerasrc->cmd_lock);
                   g_mutex_unlock(camerasrc->cmd_lock);
                }
            }

        break;
        case PROP_CAMERA_VIDEO_WIDTH:
            camerasrc->video_width = g_value_get_int (value);
            GST_WARNING_OBJECT(camerasrc, "video width %d", camerasrc->video_width);
        break;
        case PROP_CAMERA_VIDEO_HEIGHT:
            camerasrc->video_height = g_value_get_int (value);
            GST_WARNING_OBJECT(camerasrc, "video height %d", camerasrc->video_height);
            if (camerasrc->record_stream) {
                camerasrc->record_stream->record_width = camerasrc->video_width;
                camerasrc->record_stream->record_height = camerasrc->video_height;
            }
        break;
#ifdef VDIS
        case PROP_CAMERA_ENABLE_VDIS:
        {
            gboolean current_vdis_mode = FALSE;
            current_vdis_mode = camerasrc->enable_vdis_mode;

            /* set new one */
            camerasrc->enable_vdis_mode = g_value_get_boolean(value);
            if(camerasrc->record_stream)
                camerasrc->record_stream->enable_vdis_mode = camerasrc->enable_vdis_mode;
            GST_INFO("Set VDIS mode current %d, new %d",
                    current_vdis_mode, camerasrc->enable_vdis_mode);
        }
        break;
#endif
        case PROP_CAMERA_ENABLE_HYBRID_MODE:
            camerasrc->enable_hybrid_mode = g_value_get_boolean(value);
        break;
        case PROP_CAMERA_RECORDING_HINT:
            camerasrc->recording_hint = g_value_get_boolean(value);
            GST_WARNING_OBJECT(camerasrc, "setting recording-hint  %d", camerasrc->recording_hint);
        break;
        case PROP_CAMERA_LOW_LIGHT_AUTO_DETECTION:
            camerasrc->low_light_detection = g_value_get_int (value);
            GST_WARNING_OBJECT(camerasrc, "low_light_detection (%d)", camerasrc->low_light_detection);
            set_low_light_auto_detection((camerasrc_handle_t *)camerasrc->camera_handle, camerasrc->low_light_detection);
        break;
        case PROP_VFLIP:
            camerasrc->vflip = g_value_get_boolean ( value );
        break;
        case PROP_HFLIP:
            camerasrc->hflip = g_value_get_boolean ( value );
        break;
        case PROP_SENSOR_ORIENTATION:
            GST_WARNING_OBJECT(camerasrc, "can not set sensor orientation");
        break;
#ifdef DUAL_CAMERA
        case PROP_DUAL_CAMERA_MODE:
            camerasrc->dual_camera_mode = g_value_get_boolean ( value );
            GST_INFO_OBJECT(camerasrc, "dual camera mode (%d)", camerasrc->dual_camera_mode);
        break;
#endif
        case PROP_CAMERA_CPU_LOCK_MODE:
            camerasrc->camera_cpu_clock_lock = g_value_get_boolean ( value );
            GST_INFO_OBJECT(camerasrc, "clock lock (%d)", camerasrc->camera_cpu_clock_lock);
            break;
        case PROP_VT_MODE:
            if(camerasrc->vt_mode != g_value_get_int (value))
            {
                camerasrc->vt_mode = g_value_get_int (value);
                GST_WARNING_OBJECT(camerasrc, "vt_mode (%d)", camerasrc->vt_mode);
                //need to restrat preview for applying vt mode
                gst_element_get_state(GST_ELEMENT(camerasrc), &current_state, NULL, 0);
                GST_INFO("current_state %d", current_state);
                if (current_state == GST_STATE_PLAYING) {
                    g_mutex_lock(camerasrc->cmd_lock);
                    GST_WARNING_OBJECT(camerasrc, "PREVIEW_RESTART with restart_flag by vt_mode");
                    camerasrc->snapshot_stream->restart_flag = GST_CAMERASRC_CMD_PREVIEW_RESTART;
                    g_cond_wait(camerasrc->cmd_cond, camerasrc->cmd_lock);
                    GST_WARNING_OBJECT(camerasrc, "cmd_cond was recieved and restart complete from vt_mode");
                    g_mutex_unlock(camerasrc->cmd_lock);
                }
            }
        break;
        case PROP_CAMERA_SHOT_MODE:
            camerasrc->shot_mode = g_value_get_int (value);
            GST_WARNING_OBJECT(camerasrc, "shot_mode (%d)", camerasrc->shot_mode);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gst_camerasrc_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
    GstCameraSrc *camerasrc;

    g_return_if_fail (GST_IS_CAMERASRC (object));
    camerasrc = GST_CAMERASRC (object);

    switch (prop_id) {
        case PROP_CAMERA_HIGH_SPEED_FPS:
            g_value_set_int (value, camerasrc->high_speed_fps);
        break;
        case PROP_CAMERA_AUTO_FPS:
            g_value_set_boolean (value, camerasrc->fps_auto);
        break;
        case PROP_CAMERA_ID:
            g_value_set_int(value, camerasrc->camera_id);
        break;
        case PROP_CAMERA_CAPTURE_FOURCC:
            g_value_set_uint (value, camerasrc->cap_fourcc);
        break;
        case PROP_CAMERA_CAPTURE_QUALITY:
            g_value_set_enum (value, camerasrc->cap_quality);
        break;
        case PROP_CAMERA_CAPTURE_WIDTH:
            g_value_set_int (value, camerasrc->cap_width);
        break;
        case PROP_CAMERA_CAPTURE_HEIGHT:
            g_value_set_int (value, camerasrc->cap_height);
        break;
        case PROP_CAMERA_CAPTURE_INTERVAL:
            g_value_set_int (value, camerasrc->cap_interval);
        break;
        case PROP_CAMERA_CAPTURE_COUNT:
            g_value_set_int (value, camerasrc->cap_count);
        break;
        case PROP_CAMERA_CAPTURE_JPG_QUALITY:
            g_value_set_int (value, camerasrc->cap_jpg_quality);
        break;
        case PROP_CAMERA_CAPTURE_PROVIDE_EXIF:
            g_value_set_boolean (value, camerasrc->cap_provide_exif);
        break;
        case PROP_CAMERA_CAPTURE_HDR:
            g_value_set_int(value, camerasrc->cap_hdr);
        break;
        case PROP_CAMERA_VIDEO_WIDTH:
            g_value_set_int (value, camerasrc->video_width);
        break;
        case PROP_CAMERA_VIDEO_HEIGHT:
            g_value_set_int (value, camerasrc->video_height);
        break;
#ifdef VDIS
        case PROP_CAMERA_ENABLE_VDIS:
            g_value_set_boolean(value, camerasrc->enable_vdis_mode);
        break;
#endif
        case PROP_CAMERA_ENABLE_HYBRID_MODE:
            g_value_set_boolean(value, camerasrc->enable_hybrid_mode);
        break;
        case PROP_CAMERA_RECORDING_HINT:
            g_value_set_boolean(value, camerasrc->recording_hint);
        break;
        case PROP_CAMERA_LOW_LIGHT_AUTO_DETECTION:
            g_value_set_int(value, camerasrc->low_light_detection);
        break;
        case PROP_VFLIP:
            g_value_set_boolean(value, camerasrc->vflip);
        break;
        case PROP_HFLIP:
            g_value_set_boolean(value, camerasrc->hflip);
        break;
        case PROP_SENSOR_ORIENTATION:
            g_value_set_int(value, camerasrc->sensor_orientation);
        break;
#ifdef DUAL_CAMERA
        case PROP_DUAL_CAMERA_MODE:
            g_value_set_boolean(value, camerasrc->dual_camera_mode);
        break;
#endif
        case PROP_CAMERA_CPU_LOCK_MODE:
            g_value_set_boolean(value, camerasrc->camera_cpu_clock_lock);
        break;
        case PROP_VT_MODE:
            g_value_set_int(value, camerasrc->vt_mode);
        break;
        case PROP_CAMERA_SHOT_MODE:
            g_value_set_int(value, camerasrc->shot_mode);
        break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gst_camerasrc_finalize (GObject * object)
{
    GstCameraSrc*camerasrc = GST_CAMERASRC (object);
    GST_INFO_OBJECT(camerasrc, "ENTERED");
    free_preview(camerasrc->preview_stream);
    free_snapshot(camerasrc->snapshot_stream);
    free_record(camerasrc->record_stream);

    if( camerasrc->command_list != NULL ){
        g_queue_free( camerasrc->command_list );
        camerasrc->command_list = NULL;
    }
    g_mutex_clear (&camerasrc->commandq_mutex);
    g_cond_clear (&camerasrc->commandq_cond);

    SAFE_FREE_GCOND(camerasrc->preview_buffer_cond);
    SAFE_FREE_GMUTEX(camerasrc->preview_buffer_lock);
    SAFE_FREE_GCOND(camerasrc->video_buffer_cond);
    SAFE_FREE_GMUTEX(camerasrc->video_buffer_lock);
    SAFE_FREE_GQUEUE(camerasrc->cmd_list);
    SAFE_FREE_GCOND(camerasrc->cmd_cond);
    SAFE_FREE_GMUTEX(camerasrc->cmd_lock);
    SAFE_FREE_GCOND(camerasrc->restart_camera_cond);
    SAFE_FREE_GMUTEX(camerasrc->restart_camera_lock);
    SAFE_FREE_GCOND(camerasrc->capture_buffer_cond);
    SAFE_FREE_GMUTEX(camerasrc->capture_buffer_lock);

    if(camerasrc->batch_control_value_cache){
        if(camerasrc->batch_control_value_cache->af_area){
            free(camerasrc->batch_control_value_cache->af_area);
            camerasrc->batch_control_value_cache->af_area=NULL;
        }
        free(camerasrc->batch_control_value_cache);
        camerasrc->batch_control_value_cache=NULL;
    }

    if(camerasrc->last_buffer_cache){
        free(camerasrc->last_buffer_cache);
        camerasrc->last_buffer_cache = NULL;
    }

    if(camerasrc->camera_handle){
        free(camerasrc->camera_handle);
        camerasrc->camera_handle = NULL;
    }

    GST_INFO_OBJECT(camerasrc, "LEAVED");
    if (G_OBJECT_CLASS (gst_camerasrc_parent_class)->finalize)
        G_OBJECT_CLASS(gst_camerasrc_parent_class)->finalize(object);
}

static GstStateChangeReturn
gst_camerasrc_change_state (GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstCameraSrc *camerasrc;
    camerasrc = GST_CAMERASRC (element);

    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: NULL -> READY");
            if(camerasrc->ready_state == FALSE) {
                GST_INFO("    gst_camerasrc_create");
                if (!gst_camerasrc_create(camerasrc)){
                    camerasrc->ready_state = FALSE;
                    goto statechange_failed;
                }
                camerasrc->ready_state = TRUE;
            }
            else {
                GST_WARNING_OBJECT(camerasrc, "gst_camerasrc_change_state: ready_state is true, Already gst_camerasrc_create is done");
            }
            break;
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: READY -> PAUSED");
            ret = GST_STATE_CHANGE_NO_PREROLL;
        break;
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: PAUSED -> PLAYING");
            break;
        default:
            break;
    }
	//TODO: need to check why parent_class is not visible.

    ret = GST_ELEMENT_CLASS(gst_camerasrc_parent_class)->change_state (element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE){
        return ret;
    }

    switch (transition){
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: PLAYING -> PAUSED");
            ret = GST_STATE_CHANGE_NO_PREROLL;
            break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: PAUSED -> READY");
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
            GST_WARNING_OBJECT(camerasrc, "GST CAMERA SRC: READY -> NULL");
            GST_INFO("    gst_camerasrc_destroy");
            TRM_notify_recording_status(0);
            if (!gst_camerasrc_destroy(camerasrc)){
                goto statechange_failed;
            }
            break;
        default:
            break;
    }

    return ret;

statechange_failed:
    /* subclass must post a meaningfull error message */
    GST_ERROR_OBJECT (camerasrc, "state change failed");
    return GST_STATE_CHANGE_FAILURE;
}

int
gst_camerasrc_set_camera_control(GstCameraSrc* camerasrc,int64_t control_type,int value_1,int value_2,char* string_1){
    GstState current_state = GST_STATE_NULL;
    int ret=CAMERASRC_SUCCESS;
    if(!camerasrc){
        GST_ERROR ("null handle encountered...");
        return CAMERASRC_ERROR;
    }
    gst_element_get_state(GST_ELEMENT(camerasrc), &current_state, NULL, 0);
    if (camerasrc->mode != VIDEO_IN_MODE_UNKNOWN && camerasrc->mode != VIDEO_IN_MODE_STOP) {
        GST_INFO_OBJECT(camerasrc, "direct setting");
        ret = set_camera_control(camerasrc->camera_handle,control_type,value_1,value_2,string_1);
    }else{
        GST_INFO_OBJECT(camerasrc, "cache setting");
        ret = cache_settings_for_batch_update(camerasrc,control_type,value_1,value_2,string_1);
    }
    return ret;
}

void
gst_camerasrc_set_capture_command(GstCameraSrc *camerasrc, GstCameraControlCaptureCommand cmd){
    GST_INFO_OBJECT(camerasrc, "ENTERED");

    if( cmd == GST_CAMERA_CONTROL_CAPTURE_COMMAND_START ){
         TIMER_START(capture_timer);
#ifdef CONFIG_CAMERA_ZSL_CAPTURE
#ifdef CAMERA_NONZSL_CAPTURE_FLASH_ON_AUTO
        if((camerasrc->snapshot_stream->flash_mode != 0) && !camerasrc->preview_stream->recording_hint
        && camerasrc->snapshot_stream->capture_num == 1) {
            g_mutex_lock(camerasrc->cmd_lock);
            camerasrc->received_capture_command = TRUE;
            //no wait preview frame
            signal_flush_frame(camerasrc->preview_stream);
            g_mutex_unlock(camerasrc->cmd_lock);
        } else
#endif
        {
            capture_snapshot (camerasrc);
        }
#else
        /*To do preflash, send signal to preflash thread*/
        g_mutex_lock(&camerasrc->preflash_thread_handle->thread_lock);
        GST_WARNING_OBJECT(camerasrc, "send preflash_signal  signal");
        g_cond_signal(&camerasrc->preflash_thread_handle->thread_cond);
        g_mutex_unlock(&camerasrc->preflash_thread_handle->thread_lock);
#endif
    }else if (cmd == GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP){
#ifdef CONFIG_CAMERA_ZSL_CAPTURE
        int wait_cnt = 0;
        while(camerasrc->mode != VIDEO_IN_MODE_PREVIEW || camerasrc->firsttime) {
            if(_MAX_PREVIEW_START_TIMEOUT < wait_cnt) {
                GST_ERROR_OBJECT(camerasrc, "unable to start preview, waited for 500ms, timeout happened");
                break;
            } else {
                GST_DEBUG_OBJECT(camerasrc, "waiting for preview start");
                usleep(10*1000);
                wait_cnt++;
            }
        }
        GST_WARNING_OBJECT(camerasrc, "Preview started after %d ms",wait_cnt*10);
        stop_snapshot(camerasrc->snapshot_stream);
#else
        //TODO:Do nothing as of now
#endif
    }

    GST_LOG_OBJECT (camerasrc, "LEAVED");
    return;
}

void
gst_camerasrc_set_record_command(GstCameraSrc *camerasrc, GstCameraControlRecordCommand cmd)
{
    GST_WARNING_OBJECT(camerasrc, "ENTERED - %d", cmd);

    if (cmd == GST_CAMERA_CONTROL_RECORD_COMMAND_START) {
        /* set video stream size */
        camerasrc->record_stream->record_width = camerasrc->video_width;
        camerasrc->record_stream->record_height = camerasrc->video_height;
        camerasrc->record_stream->hfr_mode = camerasrc->high_speed_fps;
        camerasrc->mode = VIDEO_IN_MODE_VIDEO;
    } else if (cmd == GST_CAMERA_CONTROL_RECORD_COMMAND_STOP) {
         camerasrc->mode = VIDEO_IN_MODE_PREVIEW;
    } else {
        GST_ERROR_OBJECT(camerasrc, "unknown record cmd - %d", cmd);
    }

    GST_WARNING_OBJECT(camerasrc, "LEAVED");

    return;
}

static gboolean
gst_camerasrc_src_start (GstBaseSrc * src)
{
    GstCameraSrc *camerasrc = GST_CAMERASRC (src);
    int ret = TRUE;

    GST_DEBUG_OBJECT (camerasrc, "ENTERED");

    //TODo:gst_camerasrc_set_caps' will call gst_camerasrc_start(). So skip to call it.
    //ret = gst_camerasrc_start (camerasrc);

    GST_DEBUG_OBJECT (camerasrc, "LEAVED");

    return ret;
}


static gboolean
gst_camerasrc_src_stop (GstBaseSrc * src)
{
    int ret = 0;
    GstCameraSrc *camerasrc = GST_CAMERASRC (src);

    GST_WARNING_OBJECT (camerasrc, "ENTERED");

    GST_INFO("    gst_camerasrc_stop");
    ret = gst_camerasrc_stop(camerasrc);

    GST_WARNING_OBJECT (camerasrc, "LEAVED - ret %d", ret);

    return TRUE;
}

static GstCaps *
gst_camerasrc_get_caps (GstBaseSrc * src, GstCaps *filter)
{
    GstCameraSrc *camerasrc = GST_CAMERASRC (src);

    GstCaps *ret=NULL;

    GST_DEBUG_OBJECT (camerasrc, "ENTERED");
    if (camerasrc->mode == VIDEO_IN_MODE_UNKNOWN) {
        GST_INFO_OBJECT (camerasrc, "Unknown mode. Just return template caps.");
		ret = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(camerasrc));
		return filter ? gst_caps_intersect(ret, filter) : gst_caps_copy(ret);
    }else{
        /*FIXME: Using "VIDIOC_ENUM_FMT".*/
        ret = gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD(camerasrc)));
    }
    GST_DEBUG ("Probed caps: %" GST_PTR_FORMAT, ret);
    GST_DEBUG_OBJECT (camerasrc, "LEAVED");
    return ret;
}

static gboolean
gst_camerasrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
    gboolean ret = TRUE;
    GstCameraSrc *camerasrc;
    camerasrc = GST_CAMERASRC (src);

    GST_DEBUG_OBJECT (camerasrc, "ENTERED");

    /*if ((camerasrc->mode == VIDEO_IN_MODE_PREVIEW)||
    (camerasrc->mode == VIDEO_IN_MODE_VIDEO)){
        GST_INFO_OBJECT (camerasrc, "Proceed set_caps.");
        GST_INFO("            gst_camerasrc_stop",
            if (!gst_camerasrc_stop(camerasrc))
            {
            GST_INFO_OBJECT (camerasrc, "Cam sensor stop failed.");
            }
        )
    }else if (camerasrc->mode == VIDEO_IN_MODE_CAPTURE){
        GST_ERROR_OBJECT (camerasrc, "A mode of avsystem camera is capture. Not to proceed set_caps.");
        GST_DEBUG_OBJECT (camerasrc, "LEAVED");
        return FALSE;
    }else{
        //VIDEO_IN_MODE_UNKNOWN
        GST_INFO_OBJECT (camerasrc, "A mode of avsystem camera is unknown. Proceed set_caps.");
    }*/

    if (!gst_camerasrc_get_caps_info (camerasrc, caps)) {
        GST_INFO_OBJECT (camerasrc, "can't get capture information from caps %x",(unsigned int) caps);
        GST_DEBUG_OBJECT (camerasrc, "LEAVED");
        return FALSE;
    }
    if(camerasrc->ready_state == FALSE) {
        GST_INFO("    gst_camerasrc_create");
        if (!gst_camerasrc_create(camerasrc)){
            camerasrc->ready_state = FALSE;
            GST_DEBUG_OBJECT (camerasrc, "LEAVED");
            return FALSE;
        }
        camerasrc->ready_state = TRUE;
    }
    else {
        GST_WARNING_OBJECT(camerasrc, "gst_camerasrc_set_caps: ready_state is true, Already gst_camerasrc_create is done");
    }
    if (camerasrc->mode != VIDEO_IN_MODE_PREVIEW) {
        GST_INFO("            gst_camerasrc_start");
        ret = gst_camerasrc_start(camerasrc);
        if (!ret) {
            GST_ERROR_OBJECT (camerasrc,  "Cam sensor start failed.");
        }
    }
	ret = gst_pad_push_event (GST_BASE_SRC_PAD (src), gst_event_new_caps (caps));

    GST_WARNING_OBJECT(camerasrc, "LEAVED - ret %d", ret);

    return ret;
}

static GstFlowReturn
gst_camerasrc_src_create (GstPushSrc * src, GstBuffer ** buffer) {
    GstCameraSrc *camerasrc = GST_CAMERASRC (src);
    GstFlowReturn ret=GST_FLOW_OK;
    GST_DEBUG_OBJECT (camerasrc, "ENTERED");
    ret=gst_camerasrc_read_frame(camerasrc,buffer);
    GST_DEBUG_OBJECT (camerasrc, "LEAVED");
    return ret;
}

/*********************************BUFFER functions***********************************/
G_DEFINE_BOXED_TYPE(GstCameraBuffer, gst_camerasrc_buffer, NULL, gst_camerasrc_buffer_finalize);

static void
gst_camerasrc_buffer_class_init(gpointer g_class, gpointer class_data)
{
}

static GstCameraBuffer*
gst_camerasrc_buffer_new(GstCameraSrc *camerasrc)
{
    GstCameraBuffer *ret = NULL;
    ret = (GstCameraBuffer *)malloc(sizeof(*ret));
    ret->buffer = gst_buffer_new();
    ret->camerasrc = gst_object_ref(GST_OBJECT(camerasrc));
    GST_LOG_OBJECT(camerasrc, "creating buffer : %p", ret);
    return ret;
}

static void
gst_camerasrc_buffer_finalize(GstCameraBuffer *buffer)
{
    GstCameraSrc *camerasrc = buffer->camerasrc;
    if(camerasrc->finalizeLogCnt) {
        GST_WARNING_OBJECT(camerasrc,"ENTER - buffer (%p) buffer type (%d), buffer->gstbuffer (%p), buffer->buffer_index(%d), num-live-buffer(%d)",
                        buffer, buffer->buffer_type, buffer->buffer, buffer->buffer_index, camerasrc->num_live_buffers);
        camerasrc->finalizeLogCnt--;
    } else {
        GST_DEBUG_OBJECT(camerasrc,"ENTER - buffer (%p) buffer type (%d) buffer->gstbuffer (%p) , buffer->buffer_index(%d)",
                        buffer, buffer->buffer_type, buffer->buffer, buffer->buffer_index);
    }
    /* Buffer Q again */
    if (PREVIEW_BUFFER == buffer->buffer_type) {
        /* release preview buffer */
        done_preview_frame(camerasrc->preview_stream, buffer->buffer_index);

        /* decrease live buffer count and send signal */
        g_mutex_lock(camerasrc->preview_buffer_lock);
        GST_DEBUG_OBJECT(camerasrc, "release preview buffer - num_live_buffers %d -> %d",
                                    camerasrc->num_live_buffers, camerasrc->num_live_buffers - 1);
        camerasrc->num_live_buffers--;

        if (camerasrc->num_live_buffers < 0)
            GST_WARNING("Preview frame buffers count is negative. (Live=%d)", camerasrc->num_live_buffers);

        g_cond_signal(camerasrc->preview_buffer_cond);
        g_mutex_unlock(camerasrc->preview_buffer_lock);
    } else if (RECORD_BUFFER == buffer->buffer_type) {
        /* release video buffer */
        done_record_frame(camerasrc->record_stream, buffer->buffer_metadata);

        /* decrease live buffer count and send signal */
        g_mutex_lock(camerasrc->video_buffer_lock);
        GST_DEBUG_OBJECT(camerasrc, "release video buffer - live %d -> %d",
                                    camerasrc->num_video_buffers, camerasrc->num_video_buffers - 1);
        camerasrc->num_video_buffers--;

        if (camerasrc->num_video_buffers < 0)
            GST_WARNING("Recorder frame buffers count is negative. (Live=%d)", camerasrc->num_video_buffers);

        g_cond_signal(camerasrc->video_buffer_cond);
        g_mutex_unlock(camerasrc->video_buffer_lock);
    }

    GST_DEBUG_OBJECT(camerasrc,"done buffer[type %d] release", buffer->buffer_type);
	//TODO: need to change mini object related changes.
	gst_object_unref(camerasrc);
	free(buffer);

    GST_DEBUG_OBJECT(camerasrc, "LEAVE");
    return;
}


static void gst_camerasrc_wait_all_preview_buffer_finalized(GstCameraSrc *camerasrc)
{
    if (!camerasrc ||
        !camerasrc->preview_buffer_lock ||
        !camerasrc->preview_buffer_cond) {
        GST_ERROR("some handle is NULL");
        return;
    }

    /* check preview buffer count */
    g_mutex_lock(camerasrc->preview_buffer_lock);

    GST_WARNING_OBJECT(camerasrc, "wait preview buffer - live %d",
                                  camerasrc->num_live_buffers);

    while (camerasrc->num_live_buffers > 0) {
        GTimeVal abstimeout;
        g_get_current_time(&abstimeout);
        g_time_val_add(&abstimeout, _PREVIEW_BUFFER_WAIT_TIMEOUT);
        if (!g_cond_timed_wait(camerasrc->preview_buffer_cond, camerasrc->preview_buffer_lock, &abstimeout)) {
            GST_ERROR_OBJECT(camerasrc, "Preview Buffer wait timeout[%d usec].(Live=%d) Skip waiting...",
                                        _PREVIEW_BUFFER_WAIT_TIMEOUT, camerasrc->num_live_buffers);
            break;
        } else {
            GST_WARNING_OBJECT(camerasrc, "Signal received.");
        }
    }

    GST_WARNING("Waiting free Preview buffer finished. (Live=%d)",
                camerasrc->num_live_buffers);

    g_mutex_unlock(camerasrc->preview_buffer_lock);

    return;
}


static void gst_camerasrc_wait_all_video_buffer_finalized(GstCameraSrc *camerasrc)
{
    if (!camerasrc ||
        !camerasrc->video_buffer_lock ||
        !camerasrc->video_buffer_cond) {
        GST_ERROR("some handle is NULL");
        return;
    }

    GST_WARNING_OBJECT(camerasrc, "wait video buffer - live %d",
                                  camerasrc->num_video_buffers);

    /* check video buffer count */
    g_mutex_lock(camerasrc->video_buffer_lock);

    while (camerasrc->num_video_buffers > 0) {
        GTimeVal abstimeout;
        g_get_current_time(&abstimeout);
        g_time_val_add(&abstimeout, _VIDEO_BUFFER_WAIT_TIMEOUT);
        if (!g_cond_timed_wait(camerasrc->video_buffer_cond, camerasrc->video_buffer_lock, &abstimeout)) {
            GST_ERROR_OBJECT(camerasrc, "Video Buffer wait timeout[%d usec].(Live=%d) Skip waiting...",
                                        _VIDEO_BUFFER_WAIT_TIMEOUT, camerasrc->num_video_buffers);
            break;
        } else {
            GST_WARNING_OBJECT(camerasrc, "Signal received.");
        }
    }

    GST_WARNING_OBJECT(camerasrc, "Waiting free Video buffer finished. (Live=%d)",
                                  camerasrc->num_video_buffers);

    g_mutex_unlock(camerasrc->video_buffer_lock);

    return;
}


static void gst_camerasrc_release_all_queued_preview_buffer(GstCameraSrc *camerasrc)
{
    GstBuffer *buffer = NULL;

    if (!camerasrc ||
        !camerasrc->preview_stream ||
        !camerasrc->video_buffer_cond) {
        GST_ERROR("some handle is NULL");
        return;
    }

    get_preview_stream_mutex(camerasrc->preview_stream);

    GST_WARNING_OBJECT(camerasrc, "release remained buffer in queue");

    /* release remained buffer in queue */
    while (TRUE) {
        if (frame_available(camerasrc->preview_stream)) {
            buffer = get_preview_frame (camerasrc->preview_stream);
            GST_WARNING_OBJECT(camerasrc, "release buffer %p", buffer);
            gst_buffer_unref(buffer);
            buffer = NULL;
        } else {
            GST_WARNING_OBJECT(camerasrc, "no available buffer in queue");
            break;
        }
    }

    GST_WARNING_OBJECT(camerasrc, "release remained buffer done");

    release_preview_stream_mutex(camerasrc->preview_stream);

    return;
}


static void
gst_camerasrc_base_init (gpointer klass)
{

    GST_DEBUG("ENTERED");

    GST_DEBUG("LEAVED");
}


static void
gst_camerasrc_class_init (GstCameraSrcClass * klass)
{
  //  GST_DEBUG_CATEGORY_INIT(camerasrc_debug, "camerasrc", 0, "camerasrc element");
    GST_DEBUG_CATEGORY_INIT (gst_camera_src_debug, "camerasrc", 0, "camerasrc");

    GObjectClass *gobject_class;
    GstElementClass *element_class;
    GstBaseSrcClass *basesrc_class;
    GstPushSrcClass *pushsrc_class;

    GST_DEBUG("ENTERED");

    gobject_class = G_OBJECT_CLASS (klass);
    element_class = GST_ELEMENT_CLASS (klass);
    basesrc_class = GST_BASE_SRC_CLASS (klass);
    pushsrc_class = GST_PUSH_SRC_CLASS (klass);

    gobject_class->set_property = gst_camerasrc_set_property;
    gobject_class->get_property = gst_camerasrc_get_property;

    gobject_class->finalize = gst_camerasrc_finalize;

    element_class->change_state = gst_camerasrc_change_state;

    basesrc_class->start = gst_camerasrc_src_start;
    basesrc_class->stop = gst_camerasrc_src_stop;

    basesrc_class->get_caps = gst_camerasrc_get_caps;
    basesrc_class->set_caps = gst_camerasrc_set_caps;

    pushsrc_class->create = gst_camerasrc_src_create;

    g_object_class_install_property (gobject_class, PROP_CAMERA_HIGH_SPEED_FPS,
                                                g_param_spec_int ("high-speed-fps", "Fps for high speed recording",
                                                "If this value is 0, the element doesn't activate high speed recording.",
                                                0, G_MAXINT, _DEFAULT_HIGH_SPEED_FPS,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_AUTO_FPS,
                                                g_param_spec_boolean ("fps-auto", "FPS Auto",
                                                "Field for auto fps setting",
                                                _DEFAULT_FPS_AUTO,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_ID,
                                                g_param_spec_int ("camera-id", "index number of camera to activate",
                                                "index number of camera to activate",
                                                _FD_MIN, _FD_MAX, 0,
                                                G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_FOURCC,
                                                g_param_spec_uint ("capture-fourcc", "Capture format",
                                                "Fourcc value for capture format",
                                                0, G_MAXUINT, 0,
                                                G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_QUALITY,
                                                g_param_spec_enum ("capture-quality", "Capture quality",
                                                "Quality of capture image (JPEG: 'high', RAW: 'high' or 'low')",
                                                GST_TYPE_CAMERASRC_QUALITY,
                                                _DEFAULT_CAP_QUALITY,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_WIDTH,
                                                g_param_spec_int ("capture-width", "Capture width",
                                                "Width for camera size to capture",
                                                0, G_MAXINT, _DEFAULT_CAP_WIDTH,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_HEIGHT,
                                                g_param_spec_int ("capture-height", "Capture height",
                                                "Height for camera size to capture",
                                                0, G_MAXINT, _DEFAULT_CAP_HEIGHT,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_INTERVAL,
                                                g_param_spec_int ("capture-interval", "Capture interval",
                                                "Interval time to capture (millisecond)",
                                                0, G_MAXINT, _DEFAULT_CAP_INTERVAL,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_COUNT,
                                                g_param_spec_int ("capture-count", "Capture count",
                                                "Capture conut for multishot",
                                                1, G_MAXINT, _DEFAULT_CAP_COUNT,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_JPG_QUALITY,
                                                g_param_spec_int ("capture-jpg-quality", "JPEG Capture compress ratio",
                                                "Quality of capture image compress ratio",
                                                1, 100, _DEFAULT_CAP_JPG_QUALITY,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_CAPTURE_PROVIDE_EXIF,
                                                g_param_spec_boolean ("provide-exif", "Whether EXIF is provided",
                                                "Does capture provide EXIF?",
                                                _DEFAULT_CAP_PROVIDE_EXIF,
                                                G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_CAMERA_SHOT_MODE,
                                                g_param_spec_int ("shotmode", "shot mode",
                                                "shot mode to be going to",
                                                0,3,_DEFAULT_SHOT_MODE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CAMERA_CAPTURE_HDR,
                                                g_param_spec_int("hdr-capture", "HDR capture mode",
                                                "HDR(High Dynamic Range) Capture makes the captured image with proper exposure",
                                                CAMERASRC_HDR_CAPTURE_OFF, CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL, _DEFAULT_CAP_HDR,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_CAMERA_VIDEO_WIDTH,
                                                g_param_spec_int ("video-width", "Video width",
                                                "Width for video size to send via video-stream-cb",
                                                0, G_MAXINT, _DEFAULT_VIDEO_WIDTH,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_CAMERA_VIDEO_HEIGHT,
                                                g_param_spec_int ("video-height", "Video height",
                                                "Height for video size to send via video-stream-cb",
                                                0, G_MAXINT, _DEFAULT_VIDEO_HEIGHT,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef VDIS
    g_object_class_install_property(gobject_class, PROP_CAMERA_ENABLE_VDIS,
                                                g_param_spec_boolean("enable-vdis-mode", "Enable Video Digital Image Stabilization mode",
                                                "Anti-handshake on preview image, not captured image",
                                                _DEFAULT_ENABLE_VDIS_MODE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
    g_object_class_install_property(gobject_class, PROP_CAMERA_ENABLE_HYBRID_MODE,
                                                g_param_spec_boolean("enable-hybrid-mode", "Enable Video Recording Mode",
                                                "Enable Video Recording mode",
                                                _DEFAULT_ENABLE_HYBRID_MODE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CAMERA_RECORDING_HINT,
                                                g_param_spec_boolean("recording-hint", "Value to know recording status",
                                                "Value to know recording status",
                                                _DEFAULT_RECORDING_HINT,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_CAMERA_LOW_LIGHT_AUTO_DETECTION,
                                                g_param_spec_int ("low-light-detection", "Auto Low light detection",
                                                "Auto Low light detection enable/disable",
                                                LOW_LIGHT_DETECTION_OFF,LOW_LIGHT_DETECTION_ON,_DEFAULT_LOW_LIGHT_DETECTION,
                                                G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_VFLIP,
                                                g_param_spec_boolean ("vflip", "Flip vertically",
                                                "Flip camera input vertically",
                                                FALSE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_HFLIP,
                                                g_param_spec_boolean ("hflip", "Flip horizontally",
                                                "Flip camera input horizontally",
                                                FALSE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SENSOR_ORIENTATION,
                                                g_param_spec_int("sensor-orientation", "Sensor orientation",
                                                "Sensor mounted orientation, read-only",
                                                0, 360, 0,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#ifdef DUAL_CAMERA
    g_object_class_install_property(gobject_class, PROP_DUAL_CAMERA_MODE,
                                                g_param_spec_boolean ("dual-camera-mode", "Dual Camera Mode",
                                                "Dual Camera mode enable/disable",
                                                _DEFAULT_DUAL_CAMERA_MODE,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
    g_object_class_install_property(gobject_class, PROP_CAMERA_CPU_LOCK_MODE,
                                                g_param_spec_boolean ("camera-cpu-lock", "Camera CPU Lock",
                                                "Camera CPU Lock enable/disable",
                                                _DEFAULT_ENABLE_CAMERA_CLOCK_LOCK,
                                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_VT_MODE,
                                                g_param_spec_int("vt-mode", "Camera mode for VT call",
                                                "VT mode for fps-setting",
                                                VT_MODE_OFF, VT_MODE_ON, _DEFAULT_VT_MODE,
                                                G_PARAM_READWRITE));
    /**
    * GstCameraSrc::still-capture:
    * @camerasrc: the camerasrc instance
    * @buffer: the buffer that will be pushed - Main
    * @buffer: the buffer that will be pushed - Thumbnail
    * @buffer: the buffer that will be pushed - Screennail
    *
    * This signal gets emitted before sending the buffer.
    */
    gst_camerasrc_signals[SIGNAL_STILL_CAPTURE] =
                                        g_signal_new( "still-capture",
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET(GstCameraSrcClass, still_capture),
                                        NULL,
                                        NULL,
                                        gst_camerasrc_VOID__OBJECT_OBJECT,
                                        G_TYPE_NONE,
                                        3, /* Number of parameter */
                                        GST_TYPE_SAMPLE,  /* Main image buffer */
                                        GST_TYPE_SAMPLE,  /* Thumbnail image buffer */
                                        GST_TYPE_SAMPLE); /* Screennail image buffer */
    /**
    * GstCameraSrc::start-record:
    * @camerasrc: the camerasrc instance
    * @buffer: the buffer that will be pushed
    *
    * This signal gets emitted before sending the buffer.
    */
    gst_camerasrc_signals[SIGNAL_VIDEO_STREAM_CB] =
                                        g_signal_new( "video-stream-cb",
                                        G_TYPE_FROM_CLASS(klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET(GstCameraSrcClass, video_stream_cb),
                                        NULL,
                                        NULL,
                                        gst_camerasrc_VOID__OBJECT_VIDEO_STREAM,
                                        G_TYPE_NONE,
                                        1, /* Number of parameter */
                                        GST_TYPE_SAMPLE); /* record buffer */

    //wh01.cho:req-negotiation:+:To notify user of camerasrc, after changing resolution.
    /**
    * GstCameraSrc::nego-complete:
    * @camerasrc: the camerasrc instance
    * @start: when re-negotiation is finished.
    *
    */
    gst_camerasrc_signals[SIGNAL_NEGO_COMPLETE] =
                                        g_signal_new ("nego-complete",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GstCameraSrcClass, nego_complete),
                                        NULL,
                                        NULL,
                                        g_cclosure_marshal_generic,
                                        G_TYPE_NONE, 0);

    gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&src_factory));
    gst_element_class_set_metadata (element_class,
    "Camera Source GStreamer Plug-in",
    "Src/Video",
    "Spreadtrum camera HAL based GStreamer Plug-in",
    "Ravi Patil<ravi.spatil@samsung.com>");

    GST_DEBUG("LEAVED");
}


//TODO: is second paramenter needed
static void
gst_camerasrc_init (GstCameraSrc *camerasrc)
{
    GST_DEBUG_OBJECT (camerasrc, "ENTERED");
    camerasrc->ready_state = FALSE;
    camerasrc->width=_DEFAULT_WIDTH;
    camerasrc->height=_DEFAULT_HEIGHT;
    camerasrc->fps=_DEFAULT_FPS;
    camerasrc->format_str = "NV12";
    camerasrc->pix_format=_DEFAULT_PIX_FORMAT;
    camerasrc->colorspace=_DEFAULT_COLORSPACE;
    camerasrc->rotate=0;
    camerasrc->use_rotate_caps=FALSE;
    camerasrc->num_live_buffers = 0;
    camerasrc->qbuf_without_display = 0;
    camerasrc->num_video_buffers = 0;

    //Element properties
    camerasrc->high_speed_fps=0;
    camerasrc->fps_auto=FALSE;
    camerasrc->camera_id=_DEFAULT_CAMERA_ID;
    camerasrc->cap_fourcc=_DEFAULT_FOURCC;
    camerasrc->cap_format_str ="JPEG";
    camerasrc->cap_quality=_DEFAULT_CAP_QUALITY;
    camerasrc->prev_cap_width=_DEFAULT_CAP_WIDTH;
    camerasrc->cap_width=_DEFAULT_CAP_WIDTH;
    camerasrc->cap_height=_DEFAULT_CAP_HEIGHT;
    camerasrc->cap_interval=_DEFAULT_CAP_INTERVAL;
    camerasrc->cap_count=_DEFAULT_CAP_COUNT;
    camerasrc->cap_jpg_quality=_DEFAULT_CAP_JPG_QUALITY;
    camerasrc->cap_hdr=_DEFAULT_CAP_HDR;
    camerasrc->video_width = _DEFAULT_VIDEO_WIDTH;
    camerasrc->video_height = _DEFAULT_VIDEO_HEIGHT;
#ifdef VDIS
    camerasrc->enable_vdis_mode=_DEFAULT_ENABLE_VDIS_MODE;
#endif
    camerasrc->cap_provide_exif=_DEFAULT_CAP_PROVIDE_EXIF;
    camerasrc->enable_zsl_mode=_DEFAULT_ENABLE_ZSL_MODE;
    camerasrc->signal_still_capture=_DEFAULT_SIGNAL_STILL_CAPTURE;
    camerasrc->sensor_orientation = 0;

    camerasrc->low_light_detection=_DEFAULT_LOW_LIGHT_DETECTION;
    camerasrc->low_light_condition = LOW_LIGHT_NOT_DETECTED;

    camerasrc->face_detect=0;
    camerasrc->face_detected=FALSE;
    camerasrc->vflip=FALSE;
    camerasrc->hflip=FALSE;
#ifdef DUAL_CAMERA
    camerasrc->dual_camera_mode=_DEFAULT_DUAL_CAMERA_MODE;
#endif
    camerasrc->camera_cpu_clock_lock=_DEFAULT_ENABLE_CAMERA_CLOCK_LOCK;
    camerasrc->is_start_failed = FALSE;
    camerasrc->vt_mode = _DEFAULT_VT_MODE;
    camerasrc->shot_mode = _DEFAULT_SHOT_MODE;
    camerasrc->recording_hint = _DEFAULT_RECORDING_HINT;
    //Internal variable
    camerasrc->camera_handle=NULL;
    camerasrc->mode = VIDEO_IN_MODE_UNKNOWN;
    camerasrc->ion_fd = -1;
    camerasrc->firsttime = FALSE;
    camerasrc->firsttime_record = FALSE;
    camerasrc->command_list = g_queue_new ();
    g_mutex_init (&camerasrc->commandq_mutex);
    g_cond_init (&camerasrc->commandq_cond);

    camerasrc->preview_stream=initialize_preview((void*)camerasrc,(preview_cb)preview_frame_cb);
    camerasrc->record_stream=initialize_record((void*)camerasrc,(record_cb)record_frame_cb);
    camerasrc->snapshot_stream=initialize_snapshot((void*)camerasrc,(snapshot_done_cb)snapshot_done_signal);

    /* init mutex and cond */
    camerasrc->preview_buffer_lock = g_mutex_new ();
    camerasrc->preview_buffer_cond = g_cond_new ();
    camerasrc->video_buffer_lock = g_mutex_new ();
    camerasrc->video_buffer_cond = g_cond_new ();
    camerasrc->restart_camera_lock = g_mutex_new ();
    camerasrc->restart_camera_cond = g_cond_new ();
    camerasrc->capture_buffer_lock = g_mutex_new ();
    camerasrc->capture_buffer_cond = g_cond_new ();

    /* record command */
    camerasrc->cmd_list = g_queue_new ();
    camerasrc->cmd_lock = g_mutex_new ();
    camerasrc->cmd_cond = g_cond_new ();

    /* focus */
#ifdef CONFIG_CAMERA_AUTOFOCUS_NOT_SUPPORT
    camerasrc->is_focusing = FALSE;
    camerasrc->focus_mode = GST_CAMERASRC_FOCUS_MODE_PAN;
#else
    camerasrc->is_focusing = TRUE;
    camerasrc->focus_mode = GST_CAMERASRC_FOCUS_MODE_CONTINUOUS;
#endif
    camerasrc->focus_range = GST_CAMERASRC_FOCUS_RANGE_NORMAL;

    camerasrc->snapshot_buffer = NULL;
    camerasrc->snapshot_buffer_size = 0;

    camerasrc->batch_control_value_cache=0;
    camerasrc->batch_control_id_cache=CAMERASRC_PARAM_CONTROL_START;
    camerasrc->setting_value_cached=FALSE;

    /* snapshot handle */
    camerasrc->snapshot_handle = NULL;
#ifdef RAW_CB_FOR_SHUTTER_SOUND
    camerasrc->raw_capture_done_handle = NULL;
#endif

    camerasrc->received_capture_command = FALSE;
    camerasrc->last_buffer_cache = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));
    camerasrc->frame_count = 0;

    /* we operate in time */
    gst_base_src_set_format (GST_BASE_SRC (camerasrc), GST_FORMAT_TIME);
    gst_base_src_set_live (GST_BASE_SRC (camerasrc), TRUE);
    gst_base_src_set_do_timestamp (GST_BASE_SRC (camerasrc), TRUE);

    GST_DEBUG("LEAVED");
    return;
}


static gboolean
camerasrc_init (GstPlugin * camerasrc)
{
    return gst_element_register (camerasrc, "camerasrc", GST_RANK_NONE,
    GST_TYPE_CAMERASRC);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    camerasrc,
    "Camera source plug-in",
    camerasrc_init,
    PACKAGE_VERSION,
    "LGPL",
    "Samsung Electronics Co",
    "http://www.samsung.com")

    /* GstURIHandler interface */
static GstURIType
gst_camerasrc_uri_get_type (GType type)
{
    return GST_URI_SRC;
}

static const gchar * const*
gst_camerasrc_uri_get_protocols (GType type)
{
    static const gchar *protocols[] = { "camera", NULL };
    return protocols;
}

static gchar *
gst_camerasrc_uri_get_uri (GstURIHandler * handler)
{
    return strdup("camera://0");
}

static gboolean
gst_camerasrc_uri_set_uri (GstURIHandler * handler, const gchar * uri, GError **error)
{
    GstCameraSrc *camerasrc = GST_CAMERASRC (handler);
    const gchar *device = "0";
    if (strcmp (uri, "camera://") != 0) {
        device = uri + 9;
    }
    g_object_set (camerasrc, "camera-id", atoi(device), NULL);

    return TRUE;
}


static void
gst_camerasrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
    GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

    iface->get_type = gst_camerasrc_uri_get_type;
    iface->get_protocols = gst_camerasrc_uri_get_protocols;
    iface->get_uri = gst_camerasrc_uri_get_uri;
    iface->set_uri = gst_camerasrc_uri_set_uri;
}

