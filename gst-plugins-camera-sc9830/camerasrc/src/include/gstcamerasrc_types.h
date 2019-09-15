/*
 * gstcamerasrc_types.h
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

#ifndef __GSTCAMERASRC_TYPES_H__
#define __GSTCAMERASRC_TYPES_H__

#include <stdio.h>
#include <malloc.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mm_types.h>
#include <sys/time.h>
#include <math.h>
#include <gst/gst.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include "gstcamerasrc_errors.h"
#include "gstcamerasrc_defs.h"

typedef enum {
    CAMERASRC_AUTO_FOCUS_RESULT_INVALID = -1,   /**< AF Invalid */
    CAMERASRC_AUTO_FOCUS_RESULT_RESTART = 0,    /**< AF Restart */
    CAMERASRC_AUTO_FOCUS_RESULT_FUCUSING,       /**< AF Focusing */
    CAMERASRC_AUTO_FOCUS_RESULT_FOCUSED,        /**< AF Focused */
    CAMERASRC_AUTO_FOCUS_RESULT_FAILED,         /**< AF failed */
    CAMERASRC_AUTO_FOCUS_RESULT_NUM,            /**< Number of AF result */
}camerasrc_auto_focus_result_t;

typedef enum {
    CAMERASRC_AUTO_FOCUS_STATUS_RELEASED,
    CAMERASRC_AUTO_FOCUS_STATUS_ONGOING,
    CAMERASRC_AUTO_FOCUS_STATUS_NUM,
}camerasrc_auto_focus_status_t;

typedef enum {
    CAMERASRC_DEV_ID_PRIMARY,
    CAMERASRC_DEV_ID_SECONDARY,
    CAMERASRC_DEV_ID_EXTENSION,
    CAMERASRC_DEV_ID_UNKNOWN,
    CAMERASRC_DEV_ID_NUM,
}camerasrc_dev_id_t;

typedef enum {
    CAMERASRC_PIX_NONE = -1,      /**< Default value or Not supported */
    CAMERASRC_PIX_YUV422P = 0,    /**< Pixel format like YYYYYYYYUUUUVVVV*/
    CAMERASRC_PIX_YUV420P,        /**< Pixel format like YYYYYYYYUUVV*/
    CAMERASRC_PIX_YUV420,         /**< Pixel format like YYYYYYYYUVUV*/
    CAMERASRC_PIX_SN12,           /**< YUV420 (interleaved, non-linear) */
    CAMERASRC_PIX_ST12,           /**< YUV420 (interleaved, tiled, non-linear) */
    CAMERASRC_PIX_YUY2,           /**< YUV 4:2:2 as for UYVY but with different component ordering within the u_int32 macropixel */
    CAMERASRC_PIX_RGGB8,          /**< Raw RGB Pixel format like CCD order, a pixel consists of 8 bits, Actually means JPEG + JPEG image output */
    CAMERASRC_PIX_RGGB10,         /**< Raw RGB Pixel format like CCD order, a pixel consists of 10 bits, Actually means JPEG + YUV image output */
    CAMERASRC_PIX_RGB565,         /**< Raw RGB Pixel format like CCD order, a pixel consists of 10 bits, Actually means JPEG + YUV image output */
    CAMERASRC_PIX_UYVY,           /**< YUV 4:2:2 */
    CAMERASRC_PIX_NV12,           /**< YUV 4:2:0, 8-bit Y plane followed by an interleaved U/V plane with 2x2 subsampling */
    CAMERASRC_PIX_NV21,           /**< YUV 4:2:0, 8-bit Y plane followed by an interleaved V/U plane with 2x2 subsampling */
    CAMERASRC_PIX_NUM,            /**< Number of pixel formats*/
}camerasrc_pix_format_t;

typedef enum {
    CAMERASRC_COL_NONE = -1,    /**< Default value or Not supported */
    CAMERASRC_COL_RAW,   /**< Non-compressed RGB/YUV pixel data*/
    CAMERASRC_COL_JPEG, /**< Compressed jpg data*/
    CAMERASRC_COL_NUM,  /**< Number of colorspace data*/
}camerasrc_colorspace_t;

typedef enum {
    VIDEO_IN_MODE_UNKNOWN,
    VIDEO_IN_MODE_PREVIEW,
    VIDEO_IN_MODE_VIDEO,
    VIDEO_IN_MODE_CAPTURE,
    VIDEO_IN_MODE_STOP,
}camerasrc_video_mode_t;

typedef enum {
    GST_CAMERASRC_QUALITY_LOW,
    GST_CAMERASRC_QUALITY_HIGH,
}camerasrc_capture_quality;

typedef enum {
    CAMERASRC_SENSOR_MODE_CAMERA = 0,
    CAMERASRC_SENSOR_MODE_MOVIE,
}camerasrc_sensor_mode_t;

typedef enum {
    PREVIEW_BUFFER = 0,
    RECORD_BUFFER,
}camerasrc_buffer_type_t;

typedef enum {
    CAMERASRC_HDR_CAPTURE_OFF,
    CAMERASRC_HDR_CAPTURE_ON,
    CAMERASRC_HDR_CAPTURE_ON_AND_ORIGINAL,
} camerasrc_hdr_mode;

typedef enum {
    CAMERASRC_FLIP_NONE,
    CAMERASRC_FLIP_HORIZONTAL,
    CAMERASRC_FLIP_VERTICAL,
    CAMERASRC_FLIP_BOTH,
} camerasrc_flip_mode;

typedef enum {
	CAMERASRC_FACE_DETECT_OFF,
	CAMERASRC_FACE_DETECT_ON,
} camerasrc_face_detect_mode;

typedef enum {
    LOW_LIGHT_DETECTION_OFF,
    LOW_LIGHT_DETECTION_ON,
} camerasrc_low_light_detection_mode;

typedef enum {
    LOW_LIGHT_NOT_DETECTED,
    LOW_LIGHT_IS_DETECTED,
} camerasrc_low_light_condition;

typedef enum {
    LOW_LIGHT_MODE_OFF,
    LOW_LIGHT_MODE_ON,
} camerasrc_low_light_mode;

typedef enum {
    VT_MODE_OFF = 0,
    VT_MODE_ON
} camerasrc_vt_mode;

typedef enum {
    CAMERASRC_PREVIEW_FORMAT_YUV420SP = 0,
    CAMERASRC_PREVIEW_FORMAT_YUV420P
} camerasrc_preview_format_mode;

typedef enum {
    CAMERASRC_SNAPSHOT_FORMAT_JPEG = 0,
} camerasrc_snapshot_format_mode;

/* Shot mode */
typedef enum {
    CAMERASRC_SHOT_MODE_NORMAL         = 0x00,
    CAMERASRC_SHOT_MODE_AUTO           = 0x01,
    CAMERASRC_SHOT_MODE_BEAUTY_FACE    = 0x02,
    CAMERASRC_SHOT_MODE_BEST_PHOTO     = 0x03,
    CAMERASRC_SHOT_MODE_DRAMA          = 0x04,
    CAMERASRC_SHOT_MODE_BEST_FACE      = 0x05,
    CAMERASRC_SHOT_MODE_ERASER         = 0x06,
    CAMERASRC_SHOT_MODE_PANORAMA       = 0x07,
    CAMERASRC_SHOT_MODE_3D_PANORAMA    = 0x08,
    CAMERASRC_SHOT_MODE_RICH_TONE      = 0x09,
    CAMERASRC_SHOT_MODE_NIGHT          = 0x0A,
    CAMERASRC_SHOT_MODE_STORY          = 0x0B,
    CAMERASRC_SHOT_MODE_AUTO_PORTRAIT  = 0x0C,
    CAMERASRC_SHOT_MODE_PET            = 0x0D,
    CAMERASRC_SHOT_MODE_GOLF           = 0x0E,
    CAMERASRC_SHOT_MODE_ANIMATED_SCENE = 0x0F,
    CAMERASRC_SHOT_MODE_NIGHT_SCENE    = 0x10,
    CAMERASRC_SHOT_MODE_SPORTS         = 0x11,
    CAMERASRC_SHOT_MODE_AQUA           = 0x12,
    CAMERASRC_SHOT_MODE_MAGIC          = 0x13,
    CAMERASRC_SHOT_MODE_OUTFOCUS       = 0x14,
    CAMERASRC_SHOT_MODE_3DTOUR         = 0x15,
    CAMERASRC_SHOT_MODE_SEQUENCE       = 0x16,
    CAMERASRC_SHOT_MODE_LIGHT_TRACE    = 0x17,
    CAMERASRC_SHOT_MODE_KIDS           = 0x18,
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    CAMERASRC_THIRD_PARTY_BLACKBOX_MODE   = 0x19,
    CAMERASRC_THIRD_PARTY_VTCALL_MODE = 0x20,
    CAMERASRC_THIRD_PARTY_HANGOUT_MODE = 0x21,
#endif
    CAMERASRC_SHOT_MODE_PRO = 0x22,
    CAMERASRC_SHOT_MODE_FRONT_PANORAMA = 0x1B,
    CAMERASRC_SHOT_MODE_SELFIE_ALARM = 0x1C,
    CAMERASRC_SHOT_MODE_INTERACTIVE = 0x1D,
    CAMERASRC_SHOT_MODE_DUAL = 0x1E,
    CAMERASRC_SHOT_MODE_FASTMOTION = 0x1F,
    CAMERASRC_SHOT_MODE_MAX,
}camerasrc_shot_mode;

typedef struct _camerasrc_handle_t {
    void* preview_stream;
    void* snapshot_stream;
    void* record_stream;
    void* cam_device_handle;
    void* hw_device_data;
    struct camera_info *cam_info_data;
} camerasrc_handle_t;

typedef struct _camerasrc_rect_t {
    int x;
    int y;
    int width;
    int height;
}camerasrc_rect_t;


/*! @struct camerasrc_buffer_t
 *  @brief data buffer
 *  Image data buffer
 */
typedef struct _camerasrc_buffer_t {
	/* Supports for Planes & DMA-buf */
	struct {
		unsigned int length;    /**< Size of stored data */
		unsigned char *start;   /**< Start address of data */
		int fd;                 /* dmabuf-fd */
	} planes[4];                    /* planes: SCMN_IMGB_MAX_PLANE */
	int num_planes;
	int width;                                      /**< width of image */
	int height;                                     /**< height of image */
} camerasrc_buffer_t;

/*! @struct camerasrc_ctrl_menu_t
 *  @brief For querying menu of specified controls
 */
typedef struct {
    int menu_index;                                             /**< What number is used for accessing this menu */
    char menu_name[MAX_SZ_CTRL_NAME_STRING];                    /**< name of each menu */
}camerasrc_ctrl_menu_t;

/*! @struct camerasrc_ctrl_info_t
 *  @brief For querying controls detail
 */
typedef struct {
    int64_t camsrc_ctrl_id;                             /**< camsrc camera control ID for controlling this */
    int v4l2_ctrl_id;                                           /**< v4l2 ctrl id, user not need to use this. see @struct camerasrc_ctrl_t */
    int ctrl_type;                                              /**< Type of this control */
    char ctrl_name[MAX_SZ_CTRL_NAME_STRING];                    /**< Name of this control */
    int min;                                                    /**< minimum value */
    int max;                                                    /**< maximum value */
    int step;                                                   /**< unit of the values */
    int default_val;                                            /**< Default value of the array or range */
    int num_ctrl_menu;                                          /**< In the case of array type control, number of supported menu information */
    camerasrc_ctrl_menu_t ctrl_menu[MAX_NUM_CTRL_MENU];         /**< @struct camerasrc_ctrl_menu_t for detailed each menu information*/
} camerasrc_ctrl_info_t;

/*! @struct camerasrc_ctrl_list_info_t
 *  @brief For querying controls
 */
typedef struct {
    int num_ctrl_list_info;                                     /**< Number of supported controls */
    camerasrc_ctrl_info_t ctrl_info[MAX_NUM_CTRL_LIST_INFO];    /**< @struct camerasrc_ctrl_info_t for each control information */
} camerasrc_ctrl_list_info_t;

typedef enum {
	INTERFACE_NONE,
	INTERFACE_COLOR_BALANCE,
	INTERFACE_CAMERA_CONTROL,
} GstInterfaceType;

static char *camerasrc_ctrl_label[CAMERASRC_COLOR_CTRL_NUM] =
{
    "brightness",               /**< label for CAMERASRC_CTRL_BRIGHTNESS */
    "contrast",                 /**< label for CAMERASRC_CTRL_CONTRAST */
    "white balance",            /**< label for CAMERASRC_CTRL_WHITE_BALANCE */
    "color tone",               /**< label for CAMERASRC_CTRL_COLOR_TONE */
    "saturation",               /**< label for CAMERASRC_CTRL_SATURATION */
    "sharpness",                /**< label for CAMERASRC_CTRL_SHARPNESS */
};

#endif /*__GSTCAMERASRC_TYPES_H__*/
