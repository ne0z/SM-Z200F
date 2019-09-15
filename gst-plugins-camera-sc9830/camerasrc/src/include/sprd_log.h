/*
**Copyright (c) 2014 - 2015 Samsung Electronics Co., Ltd.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef __SPRD_LOG_H__
#define __SPRD_LOG_H__

#include <dlog/dlog.h>

#if ENABLE_ALL_DLOG
#define _SLOG(class, tag , format, arg...)  SLOG(class, tag ,format, ##arg)
#else
#define _SLOG(class, tag , format, arg...)  do{}while(0)
#endif

#include <stdio.h>

#define SPRD_CAMERA_DEBUG_TAG "CAMERA_FW_HAL"

#undef CDBG
#undef ALOGE
#undef ALOGI
#undef ALOGD
#undef ALOGV
#undef ALOGW
#undef CDBG_HIGH
#undef CDBG_ERROR
#undef CDBG_LOW

#define CDBG(fmt, args...) _SLOG (LOG_DEBUG, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define ALOGE(fmt, args...) SLOG (LOG_ERROR, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define ALOGI(fmt, args...) _SLOG (LOG_INFO, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define ALOGD(fmt, args...) _SLOG (LOG_DEBUG, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define ALOGV(fmt, args...) _SLOG (LOG_VERBOSE, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define ALOGW(fmt, args...) SLOG (LOG_WARN, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)

#define CDBG_HIGH(fmt, args...) _SLOG (LOG_WARN, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define CDBG_ERROR(fmt, args...) SLOG (LOG_ERROR, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)
#define CDBG_LOW(fmt, args...) _SLOG (LOG_INFO, SPRD_CAMERA_DEBUG_TAG, fmt, ##args)

#endif /* __SPRD_LOG_H__ */

