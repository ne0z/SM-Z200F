/*
 * camera_record.c
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

#include "camera_record.h"
#include "gstcamerasrc.h"
#include "CameraHALCInterface.h"

void record_timestamp_cb(int64_t cb_timestamp, const camera_memory_t *data, unsigned int index,void *user_data)
{

    camera_record_hw_intf *record_stream=(camera_record_hw_intf*)
                                                                            ((camerasrc_handle_t*)user_data)->record_stream;
    record_stream->timestamp = cb_timestamp;
}

camera_record_hw_intf* initialize_record(void* camerasrc,record_cb cb_fn)
{
    camera_record_hw_intf* record_stream=NULL;
    record_stream=malloc(sizeof(camera_record_hw_intf));

    record_stream->record_width = FRAME_VGA_WIDTH;
    record_stream->record_height = FRAME_VGA_HEIGHT;
#if SPRD_HAL_INTERFACE
    record_stream->record_format=yuv420sp;
#endif
    g_mutex_init (&record_stream->record_mutex);
    g_cond_init (&record_stream->record_cond);
    record_stream->record_frame_queue = g_queue_new ();
    record_stream->record_frame_cb=cb_fn;
    record_stream->camerasrc=camerasrc;
    record_stream->camera_handle=0;
    record_stream->ion_handle=0;
    record_stream->hfr_mode=0;
#ifdef VDIS
    record_stream->enable_vdis_mode = FALSE;
#endif
    return record_stream;
}

gboolean set_record_parameters(camera_record_hw_intf* record_stream)
{
    ///TODO: Set all parameters
    return FALSE;
}

void set_record_flip(camera_record_hw_intf* record_stream,int flip_mode)
{
    GstCameraSrc *camerasrc=(GstCameraSrc*)record_stream->camerasrc;
    gst_camerasrc_set_camera_control(camerasrc,
                CAMERASRC_PARAM_CONTROL_VIDEO_FLIP,
                flip_mode,
                0,
                NULL);
    return;
}


gboolean prepare_record(camera_record_hw_intf* record_stream)
{
    int hfr_mode_value = 0;
    camerasrc_handle_t *phandle = NULL;

    ALOGW("%s Enter",__func__);
    TRM_notify_recording_status(1);

    if (record_stream == NULL || record_stream->camera_handle == NULL) {
        ALOGE("NULL handle %p", record_stream);
        return FALSE;
    }

    phandle = (camerasrc_handle_t *)record_stream->camera_handle;
    GstCameraSrc *camerasrc=(GstCameraSrc*)record_stream->camerasrc;
    camerasrc_batch_ctrl_t* batch_control=(camerasrc_batch_ctrl_t*)malloc(sizeof(camerasrc_batch_ctrl_t));
    int64_t control_value=CAMERASRC_PARAM_CONTROL_START;
    if(!batch_control){
        ALOGE("%s failed to allocate batch_control str",__func__);
        return FALSE;
    }
#ifdef DUAL_CAMERA
    if (camerasrc->dual_camera_mode) {
        batch_control->dual_recording_hint = 1;
        control_value = control_value|CAMERASRC_PARAM_CONTROL_DUAL_RECORDING_HINT;
    } else
#endif
    {
        batch_control->video_width=record_stream->record_width;
        batch_control->video_height=record_stream->record_height;
        control_value=control_value|CAMERASRC_PARAM_CONTROL_VIDEO_SIZE;

        batch_control->video_format=record_stream->record_format;
        control_value=control_value|CAMERASRC_PARAM_CONTROL_VIDEO_FORMAT;

        ALOGI("record_stream->hfr_mode -- :%d", record_stream->hfr_mode);

#if SPRD_HAL_INTERFACE // no HRF mode
       hfr_mode_value = 0;
#endif

        batch_control->recording_hint=1;
        batch_control->hfr_value=hfr_mode_value;
        control_value=control_value|CAMERASRC_PARAM_CONTROL_RECORDING_HINT;
#ifdef VDIS
        batch_control->vdis_mode=record_stream->enable_vdis_mode;
        control_value=control_value|CAMERASRC_PARAM_CONTROL_VDIS;
#endif
    }

    GST_INFO("set_batch_camera_parameter:prepare record");
        set_batch_camera_parameter(phandle->cam_device_handle,
            control_value,
            batch_control);

    free(batch_control);

    return TRUE;
}


gboolean start_record(camera_record_hw_intf* record_stream, gboolean restart_stream)
{
    int ret=CAMERASRC_SUCCESS;

    ALOGI("start_record ++");

    camerasrc_handle_t *phandle = (camerasrc_handle_t *)record_stream->camera_handle;

    /* restart stream */
    if (restart_stream) {
        stop_preview(phandle->preview_stream,TRUE);
        prepare_record(record_stream);
        if(!start_preview(phandle->preview_stream,FALSE))
            return FALSE;
    }

    ret = start_recording(phandle->cam_device_handle);

    ALOGI("start_record -- : ret 0x%x", ret);

    if (CAMERASRC_SUCCESS == ret)
        return TRUE;

    return FALSE;
}


gboolean unprepare_record(camera_record_hw_intf* record_stream)
{
    camerasrc_handle_t *phandle = NULL;

    ALOGI("unprepare_record ++");
	TRM_notify_recording_status(0);

    if (record_stream == NULL || record_stream->camera_handle == NULL) {
        ALOGE("NULL handle %p", record_stream);
        return FALSE;
    }

    phandle = (camerasrc_handle_t *)record_stream->camera_handle;
    GstCameraSrc *camerasrc=(GstCameraSrc*)record_stream->camerasrc;
#ifdef DUAL_CAMERA
    if (camerasrc->dual_camera_mode) {
        set_camera_parameter(phandle->cam_device_handle,
                             CAMERASRC_PARAM_CONTROL_DUAL_RECORDING_HINT,
                             0, 0,
                             NULL);
    } else
#endif
    {
        set_camera_parameter(phandle->cam_device_handle,
                             CAMERASRC_PARAM_CONTROL_RECORDING_HINT,
                             0, 0,
                             NULL);
    }
    ALOGI("unprepare_record --");

    return TRUE;
}


gboolean stop_record(camera_record_hw_intf* record_stream, gboolean restart_stream)
{
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)record_stream->camera_handle;

    ALOGI("stop_record ++");

    stop_recording(phandle->cam_device_handle);

    if (restart_stream) {
        stop_preview(phandle->preview_stream,FALSE);
        unprepare_record(record_stream);
        if(!start_preview(phandle->preview_stream,TRUE))
            return FALSE;
    }

    ALOGI("stop_record --");

    return TRUE;
}


void done_record_frame(camera_record_hw_intf* record_stream,void* opaque)
{
    camerasrc_handle_t *phandle=(camerasrc_handle_t *)record_stream->camera_handle;
    release_recording_frame(phandle->cam_device_handle,opaque);
    return;
}

void free_record(camera_record_hw_intf* record_stream)
{
    g_mutex_clear (&record_stream->record_mutex);
    g_cond_clear (&record_stream->record_cond);
    g_queue_free (record_stream->record_frame_queue);
    record_stream->record_frame_queue = NULL;
    free(record_stream);
    return;
}


gboolean record_frame_available(camera_record_hw_intf* record_stream)
{
    gboolean ret=TRUE;
    if(g_queue_is_empty (record_stream->record_frame_queue))
        ret = FALSE;
    return ret;
}


void wait_for_record_frame(camera_record_hw_intf* record_stream)
{
    GTimeVal abstimeout;
    if(g_queue_is_empty (record_stream->record_frame_queue)){
        g_get_current_time(&abstimeout);
        g_time_val_add(&abstimeout, PREVIEW_FRAME_MAX_WAIT_TIME);
        g_cond_timed_wait(&record_stream->record_cond, &record_stream->record_mutex,&abstimeout);
    }
    return;
}


GstBuffer *get_record_frame(camera_record_hw_intf* record_stream)
{
    return g_queue_pop_head (record_stream->record_frame_queue);
}


void cache_record_frame(camera_record_hw_intf* record_stream, GstBuffer * buffer)
{
    if(record_stream->record_frame_queue)
        g_queue_push_tail (record_stream->record_frame_queue, buffer);
    g_cond_broadcast (&record_stream->record_cond);
}


void get_record_stream_mutex(camera_record_hw_intf* record_stream)
{
    g_mutex_lock (&record_stream->record_mutex);
}


void release_record_stream_mutex(camera_record_hw_intf* record_stream)
{
    g_mutex_unlock (&record_stream->record_mutex);
}
