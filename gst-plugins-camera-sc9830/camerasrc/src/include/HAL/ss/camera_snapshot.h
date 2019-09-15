/*
 * camera_snapshot.h
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

#ifndef __CAMERA_SNAPSHOT_H__
#define __CAMERA_SNAPSHOT_H__

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/camera.h>
#include "gstcamerasrc_types.h"

#define USE_NEXT_CAPTURE_TIME

#ifdef RAW_CB_FOR_SHUTTER_SOUND
typedef int (*raw_snapshot_done_cb_t) (void* usr_data);
#endif
typedef void (*snapshot_done_cb)(void* snapshot_stream);
typedef void (*snapshot_raw_done_cb)(void* snapshot_stream);
typedef struct {
    int    snapshot_format;
    int     snapshot_width;
    int     snapshot_height;
    int     thumbnail_width;
    int     thumbnail_height;
    void* capture_data_buffer;
    int capture_data_size;
    int capture_num;
    int capture_interval;
    int capture_fourcc;

	 // for burst shot
    GCond   prev_restart_aft_snapshot_cond;
    GMutex  prev_restart_aft_snapshot_mutex;

    pthread_t take_picture_thread_id;
    gboolean take_picture_thread_run;
#ifdef RAW_CB_FOR_SHUTTER_SOUND
    raw_snapshot_done_cb_t raw_capture_cb;
#endif
    snapshot_done_cb cb_signal;
    int no_of_callbacks;
    gboolean burst_mode;
    gboolean longshot_mode;
    void*   camerasrc;
    camerasrc_handle_t*   camera_handle;
    int     ion_handle;
    int     m_hdr_mode;
    int     m_pre_hdr_mode;
    camerasrc_buffer_t  m_bracket_buffer[HDR_BRACKET_FRAME_NUM];
    int m_bracket_buffer_count;
    int flash_mode;
    GCond cmd_cond;
    GMutex  cmd_mutex;
    gboolean  restart_flag;
    camera_memory_t *snapshot_data;
#ifdef USE_NEXT_CAPTURE_TIME
    unsigned long next_capture_time;
    unsigned long next_captured_time;
#endif
}camera_snapshot_hw_intf;

camera_snapshot_hw_intf* initialize_snapshot(void* camerasrc,snapshot_done_cb cb_signal);
void set_snapshot_dimension(camera_snapshot_hw_intf* snapshot_stream, int width, int height);
gboolean start_snapshot(camera_snapshot_hw_intf* snapshot_stream);
#ifdef CONFIG_CAMERA_ZSL_CAPTURE
gboolean stop_snapshot(camera_snapshot_hw_intf* snapshot_stream);
#endif
gboolean cancel_snapshot(camera_snapshot_hw_intf* snapshot_stream);
void set_snapshot_flip(camera_snapshot_hw_intf* snapshot_stream,int flip_mode);
void snapshot_data_cb(const camera_memory_t *mem,void *user_data);
void free_snapshot(camera_snapshot_hw_intf* snapshot_stream);
#endif /* __CAMERA_SNAPSHOT_H__ */
