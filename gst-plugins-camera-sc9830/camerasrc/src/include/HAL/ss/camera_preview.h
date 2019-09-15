/*
 * camera_preview.h
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
#ifndef __CAMERA_PREVIEW_H__
#define __CAMERA_PREVIEW_H__

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/camera.h>
#include "gstcamerasrc_types.h"

typedef void (*preview_cb)(void *bufs,void *user_data, int bufferIndex);
typedef int (*autofocus_callback_t) (void* usr_data, int state);
typedef int (*preview_metadata_callback_t) (void* usr_data, camera_frame_metadata_t *metadata);

#define PREVIEW_BUFFER_CNT 8

typedef struct {
    int    preview_format;
    int     preview_width;
    int     preview_height;
    const gchar *format_str;
    int     fps;
    gboolean fps_auto;
    gboolean recording_hint;

    GCond   preview_cond;
    GMutex  preview_mutex;
    GQueue*     preview_frame_queue;
    GMutex  screen_nail_mutex;
    GQueue*     screen_nail_queue;
    GMutex  preview_index_mutex;
    preview_cb preview_frame_cb;
    autofocus_callback_t af_cb;
    preview_metadata_callback_t preview_metadata_cb;
    void*   camerasrc;
    camerasrc_handle_t*   camera_handle;
    int     ion_handle;
    gboolean low_light_mode;
    gboolean prev_index_arr[PREVIEW_BUFFER_CNT];
}camera_preview_hw_intf;

camera_preview_hw_intf*
initialize_preview(void* camerasrc,preview_cb cb_fn);
gboolean
start_preview(camera_preview_hw_intf* preview_stream,gboolean zsl_mode);
void
set_preview_flip(camera_preview_hw_intf* preview_stream,int flip_mode);
gboolean
stop_preview(camera_preview_hw_intf* preview_stream,gboolean zsl_mode);
void
done_preview_frame(camera_preview_hw_intf* preview_stream, int bufferIndex);
void
preview_data_cb(const camera_memory_t *mem, camera_frame_metadata_t *metadata,unsigned int index,void *user_data);
void
screen_nail_data_cb(const camera_memory_t *mem,void *user_data);
void
free_preview(camera_preview_hw_intf* preview_stream);
gboolean
frame_available(camera_preview_hw_intf* preview_stream);
void
wait_for_frame(camera_preview_hw_intf* preview_stream);
GstBuffer *
get_preview_frame(camera_preview_hw_intf* preview_stream);
void
cache_preview_frame(camera_preview_hw_intf* preview_stream,GstBuffer * buffer);
void
signal_flush_frame(camera_preview_hw_intf* preview_stream);
void
get_preview_stream_mutex(camera_preview_hw_intf* preview_stream);
void
release_preview_stream_mutex(camera_preview_hw_intf* preview_stream);
camera_memory_t *
get_screen_nail_frame(camera_preview_hw_intf* preview_stream);
void
cache_screen_nail_frame(camera_preview_hw_intf* preview_stream,camera_memory_t * buffer);
void
free_cached_screen_nail_frame(camera_memory_t* frame);
gboolean
set_preview_index(camera_preview_hw_intf* preview_stream,int index);
#endif /* __CAMERA_PREVIEW_H__ */
