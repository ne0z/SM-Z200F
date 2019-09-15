/*
 * camera_snapshot.c
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

#include "camera_snapshot.h"
#include "gstcamerasrc.h"
#include "CameraHALCInterface.h"
#define MAX_BURST_FRAME 20
#if _ENABLE_CAMERASRC_DEBUG
#include <stdio.h>
uint8_t* frame_buf[MAX_BURST_FRAME];
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
static void *take_picture_thread(void *data)
{
    camera_snapshot_hw_intf *snapshot_stream = (camera_snapshot_hw_intf *)data;
    camerasrc_handle_t *phandle = NULL;
    unsigned long current_time = 0;

    if (snapshot_stream == NULL) {
        GST_ERROR("handle is NULL");
        return NULL;
    }
    GstCameraSrc *camerasrc=(GstCameraSrc*)snapshot_stream->camerasrc;
    ALOGE("start take_picture_thread");

    g_mutex_lock(&snapshot_stream->prev_restart_aft_snapshot_mutex);

    while (snapshot_stream->take_picture_thread_run) {
        ALOGE("wait signal for take picture");
        g_cond_wait(&snapshot_stream->prev_restart_aft_snapshot_cond, &snapshot_stream->prev_restart_aft_snapshot_mutex);
        ALOGE("received signal for take picture - thread run %d", snapshot_stream->take_picture_thread_run);
        if (snapshot_stream->take_picture_thread_run == FALSE) {
            break;
        }

        phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
        current_time = gst_get_current_time();

        /* sleep if next capture time is not reached */
        if (snapshot_stream->next_capture_time > current_time && !camerasrc->cap_hdr ) {
            ALOGE("sleep time %u ms", snapshot_stream->next_capture_time - current_time);
            usleep((snapshot_stream->next_capture_time - current_time)*1000);
        }

        ALOGE("take_picture_internal");

        /* capture */
        take_picture(phandle->cam_device_handle);
        snapshot_stream->next_capture_time += snapshot_stream->capture_interval;
    }

    g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);

    ALOGE("exit take_picture_thread");

    return NULL;
}
void
snapshot_data_cb(const camera_memory_t *mem,void *user_data){
    camera_snapshot_hw_intf *snapshot_stream = (camera_snapshot_hw_intf*)((camerasrc_handle_t*)user_data)->snapshot_stream;
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
    GstCameraSrc *camerasrc = (GstCameraSrc*)snapshot_stream->camerasrc;

    if (mem == NULL) {
        snapshot_stream->no_of_callbacks++;
        ALOGE("snapshot_data_cb - mBurstCount:%d longshot %d burst %d",
              snapshot_stream->no_of_callbacks, snapshot_stream->longshot_mode, snapshot_stream->burst_mode);

        /* set next capture time when first event */
        if (snapshot_stream->no_of_callbacks == 1) {
            snapshot_stream->next_capture_time = gst_get_current_time() + snapshot_stream->capture_interval;
        }

        g_mutex_lock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
        if (snapshot_stream->no_of_callbacks < snapshot_stream->capture_num &&
            snapshot_stream->burst_mode) {
            ALOGE("take picture signal send");
            g_cond_signal(&snapshot_stream->prev_restart_aft_snapshot_cond);
            g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
            return;
        } else {
            ALOGE("Extra Call back after BustMode false");
            g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
            return;
        }
    }
    ALOGW("%s E Data 0x%x,  Size %d",__func__, mem->data, mem->size);
    g_mutex_lock(camerasrc->capture_buffer_lock);

    snapshot_stream->snapshot_data = mem;
    GST_DEBUG("snapshot_data_cb ++");

    if (snapshot_stream->m_hdr_mode) {	  // ht1211.kim@samsung.com HDR implement
        // alloc m_bracket_buffer sequentially.
        if(!snapshot_stream->m_bracket_buffer_count) {
		GST_WARNING("capture-done sent for early capture sound");
	      //  gst_camerasrc_post_message_int(camerasrc, "camerasrc-Capture", "capture-done", TRUE);
	        gst_camerasrc_post_message_int(camerasrc, "camerasrc-HDR", "progress", 0);
        }
        snapshot_stream->m_bracket_buffer[snapshot_stream->m_bracket_buffer_count].planes[0].start = (char *)malloc(mem->size);
        memcpy(snapshot_stream->m_bracket_buffer[snapshot_stream->m_bracket_buffer_count].planes[0].start,mem->data,mem->size);
        // set bracket data when each callback comes
        snapshot_stream->m_bracket_buffer[snapshot_stream->m_bracket_buffer_count].planes[0].length= (int) mem->size;
        snapshot_stream->m_bracket_buffer_count += 1;
        if (snapshot_stream->m_bracket_buffer_count != HDR_BRACKET_FRAME_NUM) {
            // wait [HDR_BRACKET_FRAME_NUM] bracket frames
            if(snapshot_stream->snapshot_data){
                snapshot_stream->snapshot_data->release(snapshot_stream->snapshot_data);
                snapshot_stream->snapshot_data = NULL;
                GST_WARNING_OBJECT(camerasrc,"free hdr snapshot data");
            }
            g_mutex_unlock(camerasrc->capture_buffer_lock);
            return;
        }
    }

    if (!snapshot_stream->m_hdr_mode) {	// not HDR mode
        snapshot_stream->capture_data_buffer = mem->data;
        if (snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('J', 'P', 'E', 'G')) {
             snapshot_stream->capture_data_size = mem->size;
        } else if (snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('N', 'V', '1', '2') ||
            snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('S', 'N', '1', '2')) {
            snapshot_stream->capture_data_size = (snapshot_stream->snapshot_width*snapshot_stream->snapshot_height * 3) / 2;
             ALOGE("capture_data_size =%d",snapshot_stream->capture_data_size);
        }
    } else {
        snapshot_stream->capture_data_buffer = NULL;
        snapshot_stream->capture_data_size = 0;
    }
    g_mutex_unlock(camerasrc->capture_buffer_lock);

    if (snapshot_stream->capture_num > 1) {
        if (!((snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('N', 'V', '1', '2') ||
            snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('S', 'N', '1', '2')) && snapshot_stream->capture_num==4 && snapshot_stream->capture_interval ==1)) {//Check LLS mode or not.
            snapshot_stream->no_of_callbacks++;
            g_mutex_lock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
            if (snapshot_stream->no_of_callbacks < snapshot_stream->capture_num &&
                    snapshot_stream->burst_mode) {
                ALOGE("take picture signal send on no longshot");
                g_cond_signal(&snapshot_stream->prev_restart_aft_snapshot_cond);
            }
            g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
        }
    }

    g_mutex_lock(&camerasrc->snapshot_handle->thread_lock);
    GST_WARNING_OBJECT(camerasrc, "send snapshot start signal");
    g_cond_signal(&camerasrc->snapshot_handle->thread_cond);
    g_mutex_unlock(&camerasrc->snapshot_handle->thread_lock);
    return;
}
void
snapshot_raw_data_cb(const camera_memory_t *mem,void *user_data){
    camera_snapshot_hw_intf *snapshot_stream = (camera_snapshot_hw_intf*)((camerasrc_handle_t*)user_data)->snapshot_stream;
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
    GstCameraSrc *camerasrc = (GstCameraSrc*)snapshot_stream->camerasrc;
    ALOGW("%s E Data 0x%x,  Size %d",__func__, mem->data, mem->size);

    snapshot_stream->snapshot_data = mem;
    snapshot_stream->capture_data_buffer = mem->data;
    snapshot_stream->capture_data_size = snapshot_stream->snapshot_height*snapshot_stream->snapshot_width*3/2;

    g_mutex_lock(&camerasrc->snapshot_handle->thread_lock);
    GST_WARNING_OBJECT(camerasrc, "send snapshot start signal");
    g_cond_signal(&camerasrc->snapshot_handle->thread_cond);
    g_mutex_unlock(&camerasrc->snapshot_handle->thread_lock);
    return;
}

camera_snapshot_hw_intf*
initialize_snapshot(void* camerasrc,snapshot_done_cb cb_signal){

    camera_snapshot_hw_intf* snapshot_stream=NULL;
    snapshot_stream=malloc(sizeof(camera_snapshot_hw_intf));

    snapshot_stream->snapshot_data = NULL;
    snapshot_stream->snapshot_width = IMAGE_1MP_WIDTH;
    snapshot_stream->snapshot_height = IMAGE_1MP_HEIGHT;
    snapshot_stream->snapshot_format = CAMERASRC_SNAPSHOT_FORMAT_JPEG;
    snapshot_stream->thumbnail_width = _THUMBNAIL_WIDTH;
    snapshot_stream->thumbnail_height = _THUMBNAIL_HEIGHT;
    snapshot_stream->capture_interval=_DEFAULT_CAP_INTERVAL;
    snapshot_stream->capture_num=_DEFAULT_CAP_COUNT;
    g_mutex_init (&snapshot_stream->prev_restart_aft_snapshot_mutex);
    g_cond_init (&snapshot_stream->prev_restart_aft_snapshot_cond);

    snapshot_stream->cb_signal = cb_signal;
    snapshot_stream->camerasrc = camerasrc;
    snapshot_stream->camera_handle = 0;
    snapshot_stream->ion_handle = 0;
    snapshot_stream->no_of_callbacks = 0;
    snapshot_stream->burst_mode = FALSE;
    snapshot_stream->longshot_mode = FALSE;
    snapshot_stream->m_hdr_mode = 0;
    snapshot_stream->m_pre_hdr_mode = 0;
    snapshot_stream->m_bracket_buffer_count =0;
    snapshot_stream->flash_mode = 0;
    snapshot_stream->restart_flag = FALSE;
    snapshot_stream->take_picture_thread_id = NULL;
    g_mutex_init (&snapshot_stream->cmd_mutex);
    g_cond_init(&snapshot_stream->cmd_cond);

    snapshot_stream->take_picture_thread_run = TRUE;
    pthread_create(&snapshot_stream->take_picture_thread_id, NULL, take_picture_thread, (void *)snapshot_stream);
    return snapshot_stream;
}


void
set_snapshot_dimension(camera_snapshot_hw_intf* snapshot_stream, int width, int height){
    if (!snapshot_stream || width < 1 || height < 1) {
        ALOGI("NULL handle[%p] or invalid size[%dx%d]", snapshot_stream, width, height);
        return;
    }

    gst_camerasrc_set_camera_control((GstCameraSrc *)snapshot_stream->camerasrc,
        CAMERASRC_PARAM_CONTROL_SNAPSHOT_SIZE,
        width,
        height,
        NULL);

    snapshot_stream->snapshot_width = width;
    snapshot_stream->snapshot_height = height;

    return;
}

gboolean
start_snapshot(camera_snapshot_hw_intf* snapshot_stream){
    int ret=CAMERASRC_SUCCESS;
    ALOGI("%s E with Size [%d %d] Format[%d]",__func__,
		snapshot_stream->snapshot_height,snapshot_stream->snapshot_width, snapshot_stream->snapshot_format);

    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
    GstCameraSrc *camerasrc=(GstCameraSrc*)snapshot_stream->camerasrc;

    ALOGE("snapshot_stream->capture_num =%d, ",snapshot_stream->capture_num);
    if (snapshot_stream->capture_num > 1){ //Burst Shot
        if (snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('J', 'P', 'E', 'G')) {
            ALOGE("JPEG");
            snapshot_stream->longshot_mode = TRUE;
        } else  if (snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('N', 'V', '1', '2') ||
               snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('S', 'N', '1', '2')){
            ALOGE("YUV");
            snapshot_stream->longshot_mode = FALSE;
            if (snapshot_stream->capture_num == 4) {
		ALOGE("LLS capture start");
                send_command(phandle->cam_device_handle,CAMERA_CMD_LOW_LIGHT_MODE, 1, 0);
		set_snapshot_shotmode(snapshot_stream,CAMERA_SHOT_MODE_LLS);
            }
        }
	set_camera_parameter(phandle->cam_device_handle,CAMERASRC_PARAM_CONTROL_FLASH_MODE,0,0,NULL);

        snapshot_stream->burst_mode = TRUE;
        snapshot_stream->no_of_callbacks = 0;
        ALOGE("start_snapshot snapshot_stream->capture_num > 1, %d ",snapshot_stream->longshot_mode);
    } else {
	if(camerasrc->cap_hdr && snapshot_stream->capture_fourcc == GST_MAKE_FOURCC ('J', 'P', 'E', 'G')) {
		ALOGW("HDR capture start");
		if(snapshot_stream->flash_mode != 0) {
			set_camera_parameter(phandle->cam_device_handle,CAMERASRC_PARAM_CONTROL_FLASH_MODE,0,0,NULL);
			ALOGW("Due to HDR capture, set flash off forcely");
		}
		send_command(phandle->cam_device_handle,CAMERA_CMD_HDR_MODE, 1, 0);
		g_mutex_lock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
		ALOGE("take picture signal send for HDR capture");
		g_cond_signal(&snapshot_stream->prev_restart_aft_snapshot_cond);
		g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
		return TRUE;
	} else {
          ALOGW("normal capture start");
          set_snapshot_shotmode(snapshot_stream,CAMERA_SHOT_MODE_NORMAL);
	}
    }

    snapshot_stream->next_capture_time = gst_get_current_time() + snapshot_stream->capture_interval;

    ret = take_picture(phandle->cam_device_handle);

    if (CAMERASRC_SUCCESS == ret) {
        return TRUE;
    }
    return FALSE;
}

#ifdef CONFIG_CAMERA_ZSL_CAPTURE
gboolean
stop_snapshot(camera_snapshot_hw_intf* snapshot_stream)
{
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
    GstCameraSrc *camerasrc=(GstCameraSrc*)snapshot_stream->camerasrc;

    snapshot_stream->burst_mode = FALSE;
    snapshot_stream->longshot_mode = FALSE;
    GST_DEBUG("Setting BurstMode FALSE %d",snapshot_stream->burst_mode);
    if (snapshot_stream->capture_num > 1) {//Burst Shot
        snapshot_stream->no_of_callbacks = 0;
        GST_WARNING("start_snapshot snapshot_stream->capture_num > 1  and num=%d",snapshot_stream->capture_num);
        set_camera_parameter(phandle->cam_device_handle,CAMERASRC_PARAM_CONTROL_FLASH_MODE,snapshot_stream->flash_mode,0,NULL);
   //     send_command(phandle->cam_device_handle,CAMERA_CMD_LOW_LIGHT_MODE, 0, 0);
    }
    if(camerasrc->cap_hdr) {
        GST_WARNING("HDR capture stop, so we reset the related parameters");
        set_camera_parameter(phandle->cam_device_handle,CAMERASRC_PARAM_CONTROL_FLASH_MODE,snapshot_stream->flash_mode,0,NULL);
    }
    if(camerasrc->firsttime == FALSE && camerasrc->face_detect){
        GST_WARNING("Face detection need to be re-setting(%d)", camerasrc->face_detect);
        send_command(phandle->cam_device_handle,CAMERA_CMD_START_FACE_DETECTION, camerasrc->face_detect, 0);
    }
    snapshot_stream->capture_num = 1;
}
#endif

void
set_snapshot_flip(camera_snapshot_hw_intf* snapshot_stream,int flip_mode){
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
//    send_command(phandle->cam_device_handle,CAMERA_CMD_SET_SNAPSHOT_FLIP,flip_mode, 0);
//    Following alternative way can also be expored by setting the snapshot stream property.Not working now.
    set_camera_parameter(phandle->cam_device_handle,
                                     CAMERASRC_PARAM_CONTROL_SNAPSHOT_FLIP,
                                     flip_mode, 0,NULL);
}

void
set_snapshot_shotmode(camera_snapshot_hw_intf* snapshot_stream,int shot_mode){
    camerasrc_handle_t *phandle = (camerasrc_handle_t *)snapshot_stream->camera_handle;
    set_camera_parameter(phandle->cam_device_handle,
                                     CAMERASRC_PARAM_CONTROL_SHOOTING_MODE,
                                     shot_mode, 0,NULL);
    GST_ERROR("set shot mode =%d",shot_mode);
}

gboolean
cancel_snapshot(camera_snapshot_hw_intf* snapshot_stream){
    int ret=CAMERASRC_SUCCESS;
    camerasrc_handle_t *phandle=(camerasrc_handle_t *)snapshot_stream->camera_handle;
    ret = cancel_picture(phandle->cam_device_handle);
    return ret;
}

void
free_snapshot(camera_snapshot_hw_intf* snapshot_stream){
    g_mutex_lock(&snapshot_stream->prev_restart_aft_snapshot_mutex);
    snapshot_stream->take_picture_thread_run = FALSE;
    g_cond_signal(&snapshot_stream->prev_restart_aft_snapshot_cond);
    g_mutex_unlock(&snapshot_stream->prev_restart_aft_snapshot_mutex);

    GST_DEBUG("join take picture thread");
    if(snapshot_stream->take_picture_thread_id !=NULL)
      pthread_join(snapshot_stream->take_picture_thread_id, NULL);

    g_mutex_clear (&snapshot_stream->prev_restart_aft_snapshot_mutex);
    g_cond_clear (&snapshot_stream->prev_restart_aft_snapshot_cond);
    g_mutex_clear (&snapshot_stream->cmd_mutex);
    g_cond_clear (&snapshot_stream->cmd_cond);
    free(snapshot_stream);
    return;
}
