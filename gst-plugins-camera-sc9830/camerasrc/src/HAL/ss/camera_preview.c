/*
 * camera_preview.c
 *
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Aditi Narula<aditi.n@samsung.com>
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
#include "camera_preview.h"
#include "gstcamerasrc.h"
#include "CameraHALCInterface.h"
#if _ENABLE_CAMERASRC_DEBUG
#include <stdio.h>
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

#define PANORAMA_SHOT_CAMERA_WIDTH      2048
#define PANORAMA_SHOT_CAMERA_HEIGHT     1536
#define DRAMA_SHOT_FPS                  30

void
preview_data_cb(const camera_memory_t *mem, camera_frame_metadata_t *metadata, unsigned int index,void *user_data){
    ALOGI("%s E mem->size(%d),index(%d)",__func__,mem->size,index);
    camera_preview_hw_intf *preview_stream=(camera_preview_hw_intf*)((camerasrc_handle_t*)user_data)->preview_stream;
    GstCameraSrc *camerasrc = (GstCameraSrc*)preview_stream->camerasrc;

    if(!(set_preview_index(preview_stream, index))) {
        ALOGE("set_preview_index is failed. ");
    }

    if(LOW_LIGHT_DETECTION_ON==camerasrc->low_light_detection && metadata){
        if(camerasrc->low_light_condition != metadata->needLLS){
            ALOGW("Low light condition changed to [%d]",metadata->needLLS);
            gst_camerasrc_post_message_int(camerasrc,
                                                                "camerasrc-Light",
                                                                "low-light-state",
                                                                metadata->needLLS);
            camerasrc->low_light_condition = metadata->needLLS;
        }
    }

#ifdef CONFIG_SS_PREVIEW_HEAP
    preview_stream->preview_frame_cb(mem,preview_stream->camerasrc, index);
#else
    int framesize=(preview_stream->preview_width*preview_stream->preview_height*3/2);
   framesize = (framesize + 256 - 1) & ~(256 - 1);   //Fixing allignment issue in QCIF

    char* framemem=(char*)mem->data;
    camera_memory_t* camera_frame = (camera_memory_t*)malloc(sizeof(camera_memory_t));
    memset(camera_frame,0,sizeof(camera_memory_t));
    camera_frame->data=framemem;
    camera_frame->size=framesize;
    camera_frame->release=0;
    camera_frame->handle=mem->handle;
    camera_frame->physical_addr=mem->physical_addr;
    preview_stream->preview_frame_cb(camera_frame,preview_stream->camerasrc, index);
#endif
    ALOGI("%s  X",__func__);
    return;
}

void
screen_nail_data_cb(const camera_memory_t *mem,void *user_data){
    if(mem){
        camera_preview_hw_intf *preview_stream=(camera_preview_hw_intf*)
                                        ((camerasrc_handle_t*)user_data)->preview_stream;
        cache_screen_nail_frame(preview_stream,mem);
    }
    return;
}

camera_preview_hw_intf*
initialize_preview(void* camerasrc,preview_cb cb_fn){
    int i = 0;
    camera_preview_hw_intf* preview_stream=NULL;
    preview_stream=malloc(sizeof(camera_preview_hw_intf));

    preview_stream->preview_width = FRAME_VGA_WIDTH;
    preview_stream->preview_height = FRAME_VGA_HEIGHT;
    preview_stream->preview_format = CAMERASRC_PREVIEW_FORMAT_YUV420SP;
    preview_stream->fps = _DEFAULT_FPS;
    preview_stream->fps_auto = _DEFAULT_FPS_AUTO;

    g_mutex_init (&preview_stream->preview_mutex);
    g_cond_init (&preview_stream->preview_cond);
    preview_stream->preview_frame_queue = g_queue_new ();

    g_mutex_init (&preview_stream->screen_nail_mutex);
    preview_stream->screen_nail_queue = g_queue_new ();

    preview_stream->preview_frame_cb=cb_fn;
    preview_stream->camerasrc=camerasrc;
    preview_stream->camera_handle=0;
    preview_stream->ion_handle=0;
    preview_stream->low_light_mode=FALSE;

    g_mutex_init (&preview_stream->preview_index_mutex);
    for(i = 0; i<PREVIEW_BUFFER_CNT; i++)
    {
        preview_stream->prev_index_arr[i] = FALSE;
    }
    return preview_stream;
}

gboolean
set_preview_index(camera_preview_hw_intf* preview_stream,int index){
    g_mutex_lock (&preview_stream->preview_index_mutex);
    if(preview_stream->prev_index_arr[index] == FALSE) {
        preview_stream->prev_index_arr[index] = TRUE;
        g_mutex_unlock (&preview_stream->preview_index_mutex);
        return TRUE;
    } else {
        ALOGE("%s failed to set preview buffer index because this buffer's index[%d] is already set ",__func__,index);
        g_mutex_unlock (&preview_stream->preview_index_mutex);
        return FALSE;
    }
}

gboolean
start_preview(camera_preview_hw_intf* preview_stream,gboolean zsl_mode){

    ALOGW("%s E with dimension[%dX%d] preview_stream->fps_auto %d preview_stream->fps %d",__func__,preview_stream->preview_width,preview_stream->preview_height,preview_stream->fps_auto,preview_stream->fps);
    int ret=CAMERASRC_SUCCESS;
    int shot_mode = CAMERASRC_SHOT_MODE_AUTO;
    camerasrc_handle_t *phandle=(camerasrc_handle_t *)preview_stream->camera_handle;
    camerasrc_batch_ctrl_t* batch_control=(camerasrc_batch_ctrl_t*)malloc(sizeof(camerasrc_batch_ctrl_t));
    GstCameraSrc *camerasrc=(GstCameraSrc*)preview_stream->camerasrc;
    int64_t control_value=CAMERASRC_PARAM_CONTROL_START;
    if(!batch_control){
        ALOGE("%s failed to allocate batch_control str",__func__);
        return FALSE;
    }

    batch_control->preview_width=preview_stream->preview_width;
    batch_control->preview_height=preview_stream->preview_height;
    control_value=control_value|CAMERASRC_PARAM_CONTROL_PREVIEW_SIZE;

    batch_control->preview_format=preview_stream->preview_format;
    control_value=control_value|CAMERASRC_PARAM_CONTROL_PREVIEW_FORMAT;

    batch_control->fps = preview_stream->fps_auto ? 0 : preview_stream->fps;
    control_value=control_value|CAMERASRC_PARAM_CONTROL_FPS;

    if (camerasrc->camera_id == CAMERASRC_DEV_ID_SECONDARY) {
        batch_control->fps_min=preview_stream->fps_auto ? _MIN_FPS_FRONT: preview_stream->fps;
    } else {
        if(camerasrc->shot_mode == 3) {  // for continous shot in low light condition, set the min fps as 15
                batch_control->fps_min=preview_stream->fps_auto ? _MIN_FPS_REAR_CONTINOUS: preview_stream->fps;
                ALOGE("Shot mode : %d , fps min is =%d", camerasrc->shot_mode , batch_control->fps_min);
        } else {
                batch_control->fps_min=preview_stream->fps_auto ? _MIN_FPS_REAR: preview_stream->fps;
        }
    }

    batch_control->fps_max=preview_stream->fps_auto ? _MAX_FPS : preview_stream->fps;
    control_value=control_value|CAMERASRC_PARAM_CONTROL_FPS_RANGE;

    /* check shot mode */
    if(!preview_stream->fps_auto && preview_stream->fps==30) {
        batch_control->recording_hint=1;
        control_value=control_value|CAMERASRC_PARAM_CONTROL_RECORDING_HINT;
    }
    if(camerasrc->shot_mode == 2) {
        shot_mode = CAMERASRC_SHOT_MODE_PANORAMA;
    }
    ALOGE("Shot mode : %d", shot_mode);
    batch_control->shoting_mode=shot_mode;
    control_value=control_value|CAMERASRC_PARAM_CONTROL_SHOOTING_MODE;

    set_batch_camera_parameter(phandle->cam_device_handle,control_value,batch_control);

    free(batch_control);

    if(camerasrc->shot_mode == 2) {
        send_command(preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_PANORAMA_MODE, 1, 0);
    } else {
        send_command(preview_stream->camera_handle->cam_device_handle,CAMERA_CMD_PANORAMA_MODE, 0, 0);
    }

    int i = 0;
    g_mutex_lock (&preview_stream->preview_index_mutex);
    for(i = 0; i<PREVIEW_BUFFER_CNT; i++)
    {
        preview_stream->prev_index_arr[i] = FALSE;
    }
    g_mutex_unlock (&preview_stream->preview_index_mutex);

    ret = start_camera_preview(phandle->cam_device_handle);

    if(CAMERASRC_SUCCESS==ret) {
        return TRUE;
    } else {
        ALOGE("%s is failed. ret(%d)",__func__, ret);
        return FALSE;
    }
}

void
set_preview_flip(camera_preview_hw_intf* preview_stream,int flip_mode)
{
    GstCameraSrc *camerasrc=(GstCameraSrc*)preview_stream->camerasrc;
    gst_camerasrc_set_camera_control(camerasrc,
                CAMERASRC_PARAM_CONTROL_PREVIEW_FLIP,
                flip_mode,
                0,
                NULL);
    return;
}

gboolean
stop_preview(camera_preview_hw_intf* preview_stream,gboolean zsl_mode){
    ALOGW("%s E with dimension[%dX%d]",__func__,preview_stream->preview_width,preview_stream->preview_height);
    camerasrc_handle_t *phandle=(camerasrc_handle_t *)preview_stream->camera_handle;
    GstCameraSrc *camerasrc=(GstCameraSrc*)preview_stream->camerasrc;
    int i = 0;
    stop_camera_preview(phandle->cam_device_handle);
    g_mutex_lock (&preview_stream->preview_index_mutex);
    for(i = 0; i<PREVIEW_BUFFER_CNT; i++)
    {
        preview_stream->prev_index_arr[i] = FALSE;
    }
    g_mutex_unlock (&preview_stream->preview_index_mutex);
    return TRUE;
}

void
done_preview_frame(camera_preview_hw_intf* preview_stream,int bufferIndex){
    camerasrc_handle_t *phandle=(camerasrc_handle_t *)preview_stream->camera_handle;
    g_mutex_lock (&preview_stream->preview_index_mutex);
    if(preview_stream->prev_index_arr[bufferIndex] == TRUE) {
        preview_stream->prev_index_arr[bufferIndex] = FALSE;
        g_mutex_unlock (&preview_stream->preview_index_mutex);
        release_preview_frame(phandle->cam_device_handle,bufferIndex);
    } else {
        g_mutex_unlock (&preview_stream->preview_index_mutex);
        ALOGE("[%s] This frame index[%d] is already released.",__func__,bufferIndex);
    }
}

void
free_preview(camera_preview_hw_intf* preview_stream){
    g_mutex_clear (&preview_stream->preview_mutex);
    g_cond_clear (&preview_stream->preview_cond);
    g_queue_free (preview_stream->preview_frame_queue);
    g_mutex_clear (&preview_stream->screen_nail_mutex);
    g_queue_free (preview_stream->screen_nail_queue);
    g_mutex_clear (&preview_stream->preview_index_mutex);
    preview_stream->preview_frame_queue = NULL;
    free(preview_stream);
    return;
}

gboolean
frame_available(camera_preview_hw_intf* preview_stream){
    gboolean ret=TRUE;
    if(g_queue_is_empty (preview_stream->preview_frame_queue))
        ret = FALSE;
    return ret;
}

void
wait_for_frame(camera_preview_hw_intf* preview_stream){
    GTimeVal abstimeout;
    if(g_queue_is_empty (preview_stream->preview_frame_queue)){
        g_get_current_time(&abstimeout);
        g_time_val_add(&abstimeout, PREVIEW_FRAME_MAX_WAIT_TIME);
        g_cond_timed_wait(&preview_stream->preview_cond, &preview_stream->preview_mutex,&abstimeout);
    }
    return;
}

GstBuffer *
get_preview_frame(camera_preview_hw_intf* preview_stream){
    return g_queue_pop_head (preview_stream->preview_frame_queue);
}

void
cache_preview_frame(camera_preview_hw_intf* preview_stream,GstBuffer * buffer){
    if(buffer && preview_stream->preview_frame_queue)
        g_queue_push_tail (preview_stream->preview_frame_queue, buffer);
    g_cond_broadcast (&preview_stream->preview_cond);
}

void
signal_flush_frame(camera_preview_hw_intf* preview_stream){
    g_cond_broadcast (&preview_stream->preview_cond);
}

void
get_preview_stream_mutex(camera_preview_hw_intf* preview_stream){
    g_mutex_lock (&preview_stream->preview_mutex);
}

void
release_preview_stream_mutex(camera_preview_hw_intf* preview_stream){
    g_mutex_unlock (&preview_stream->preview_mutex);
}

camera_memory_t *
get_screen_nail_frame(camera_preview_hw_intf* preview_stream){
    camera_memory_t* ret=NULL;
    if(preview_stream->screen_nail_queue){
        g_mutex_lock (&preview_stream->screen_nail_mutex);
        if(!g_queue_is_empty (preview_stream->screen_nail_queue)){
            ret = g_queue_pop_head (preview_stream->screen_nail_queue);
        }else{
            ret = NULL;
        }
        g_mutex_unlock (&preview_stream->screen_nail_mutex);
    }
    return ret;
}

void
cache_screen_nail_frame(camera_preview_hw_intf* preview_stream,camera_memory_t * buffer){
    if(preview_stream->screen_nail_queue){
        g_mutex_lock (&preview_stream->screen_nail_mutex);
        g_queue_push_tail (preview_stream->screen_nail_queue, buffer);
        g_mutex_unlock (&preview_stream->screen_nail_mutex);
    }
    return;
}

void
free_cached_screen_nail_frame(camera_memory_t* frame){
    if(frame){
        frame->release(frame);
        frame = NULL;
    }
    return;
}
