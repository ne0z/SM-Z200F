/*
 * Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 #ifndef __DRM_INTERFACE_H_
#define __DRM_INTERFACE_H_
#include "gspscaler.h"

#define IOMMU_ENABLED
#define ION_DEVICE "/dev/ion"

enum {
  TBM_DONT_MAP_LOCALLY = 0x00000100,
  TBM_NO_CACHING = 0x00000200
};

gboolean
drm_initialise(GstGSPScaler *gspscaler);
void
drm_release(GstGSPScaler *gspscaler);
unsigned int
drm_convert_dmabuf_gemname(GstGSPScaler *gspscaler, unsigned int dmabuf_fd, unsigned int *gem_handle);
gboolean
drm_allocate_buffer(GstGSPScaler *gspscaler,GstGSPScalerBufferInfo buf_info[],guint size,guint num);
tbm_bo_handle
drm_tbm_buffer_map(GstGSPScaler *gspscaler,GstGSPScalerBufferInfo* buf_info,int device);
void
drm_tbm_buffer_unmap(GstGSPScaler *gspscaler,GstGSPScalerBufferInfo* buf_info);
void
drm_close_gem (GstGSPScaler *gspscaler, unsigned int *gem_handle);
int
drm_ipp_set_property (GstGSPScaler *gspscaler,enum drm_sprd_ipp_cmd cmd, int prop_id, int wb_hz);
gboolean
drm_ipp_queue_buf (GstGSPScaler *gspscaler, struct drm_sprd_ipp_queue_buf *buf);
gboolean
drm_ipp_cmd_ctrl_play (GstGSPScaler *gspscaler);
void
drm_ipp_set_buffers(GstGSPScaler *gspscaler, int prop_id);
gboolean
drm_ipp_queue_buffers(GstGSPScaler *gspscaler, int prop_id);

#endif //__DRM_INTERFACE_H_
