/*
 * camera_record.h
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

#ifndef __CAMERA_RECORD_H__
#define __CAMERA_RECORD_H__

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gstcamerasrc_types.h"
#include <hardware/camera.h>
typedef void (*record_cb)(void *bufs,void *user_data);

typedef struct {
    int    record_format;
    int     record_width;
    int     record_height;

    GCond   record_cond;
    GMutex      record_mutex;
    GQueue*     record_frame_queue;
    record_cb record_frame_cb;
    void*   camerasrc;
    camerasrc_handle_t*   camera_handle;
    int     ion_handle;
    int64_t timestamp;
    int hfr_mode;
#ifdef VDIS
    int enable_vdis_mode;
#endif
}camera_record_hw_intf;

void record_timestamp_cb(int64_t cb_timestamp, const camera_memory_t *data, unsigned int index,void *user_data);
camera_record_hw_intf* initialize_record(void* camerasrc,record_cb cb_fn);
void set_record_flip(camera_record_hw_intf* record_stream,int flip_mode);
gboolean set_record_parameters(camera_record_hw_intf* record_stream);
gboolean prepare_record(camera_record_hw_intf* record_stream);
gboolean start_record(camera_record_hw_intf* record_stream, gboolean restart_stream);
gboolean unprepare_record(camera_record_hw_intf* record_stream);
gboolean stop_record(camera_record_hw_intf* record_stream, gboolean restart_stream);
void free_record(camera_record_hw_intf* record_stream);
void record_data_cb(const camera_memory_t *mem,void *user_data);
gboolean record_frame_available(camera_record_hw_intf* record_stream);
void wait_for_record_frame(camera_record_hw_intf* record_stream);
GstBuffer *get_record_frame(camera_record_hw_intf* record_stream);
void cache_record_frame(camera_record_hw_intf* record_stream,GstBuffer * buffer);
void get_record_stream_mutex(camera_record_hw_intf* record_stream);
void release_record_stream_mutex(camera_record_hw_intf* record_stream);

#endif /* __CAMERA_RECORD_H__ */
