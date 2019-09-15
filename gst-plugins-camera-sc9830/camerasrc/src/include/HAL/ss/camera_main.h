/*
 * camera_main.h
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

#ifndef __CAMERA_MAIN_H__
#define __CAMERA_MAIN_H__

#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gstcamerasrc_types.h"

int
create_camera_handle(camerasrc_handle_t **phandle);
int
open_camera_device(camerasrc_handle_t *phandle, int camera_id, int *sensor_orientation);
int
close_camera_device(camerasrc_handle_t *phandle);
int
set_camera_control(camerasrc_handle_t *phandle,int64_t control_type,int value_1,int value_2,char* string_1);
int
get_camera_control(camerasrc_handle_t *phandle,int64_t control_type,int* value_1,int* value_2,char* string_1);
int
start_camera_auto_focus(camerasrc_handle_t *phandle);
int
cancel_camera_auto_focus(camerasrc_handle_t *phandle);
int
set_low_light_auto_detection(camerasrc_handle_t *phandle,int lld_mode);
int
set_low_light_mode(camerasrc_handle_t *phandle,int llm_mode);
int
set_face_detection(camerasrc_handle_t *phandle,int fd_mode);
int
set_camera_launch_setting(camerasrc_handle_t *phandle, int64_t control_id,camerasrc_batch_ctrl_t* control_value);
#endif /* __CAMERA_MAIN_H__ */
