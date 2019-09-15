/*
 * gstcamerasrc_defs.h
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

#ifndef __GSTCAMERASRC_DEFS_H__
#define __GSTCAMERASRC_DEFS_H__

#include "sprd_log.h"

#ifndef YUV422_SIZE
#define YUV422_SIZE(width,height) ( ((width)*(height)) << 1 )
#endif

#ifndef YUV420_SIZE
#define YUV420_SIZE(width,height) ( ((width)*(height)*3) >> 1 )
#endif

#if !defined (PAGE_SHIFT)
#define PAGE_SHIFT sysconf(_SC_PAGESIZE)
#endif
#if !defined (PAGE_SIZE)
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif
#if !defined (PAGE_MASK)
#define PAGE_MASK (~(PAGE_SIZE-1))
#endif
#if !defined (PAGE_ALIGN)
#define PAGE_ALIGN(addr)    (((addr)+PAGE_SIZE-1)&PAGE_MASK)
#endif

#define ALIGN_SIZE_I420                 (1024<<2)
#define ALIGN_SIZE_NV12                 (1024<<6)
#define CAMERASRC_ALIGN(addr,size)      (((addr)+((size)-1))&(~((size)-1)))

#define ALIGN_SIZE_2K                       2048
#define ALIGN_SIZE_4K                       4096
#define ALIGN_SIZE_8K                       8192

#if !defined (CLEAR)
#define CLEAR(x)            memset (&(x), 0, sizeof (x))
#endif

#ifndef LOG_DEBUG
#define _ENABLE_CAMERASRC_DEBUG                 0
#else
#define _ENABLE_CAMERASRC_DEBUG                 0
#endif

#define CAMERASRC_ERRMSG_MAX_LEN               128
#define CAMERASRC_DEV_FD_INIT                   -1
#define _DEFAULT_WIDTH                          320
#define _DEFAULT_HEIGHT                         240
#define _DEFAULT_VIDEO_WIDTH                    320
#define _DEFAULT_VIDEO_HEIGHT                   240
#define _DEFAULT_HFR_MODE                       0
#define _DEFAULT_FPS                            15
#define _DEFAULT_HIGH_SPEED_FPS                 0
#define _DEFAULT_FPS_AUTO                       FALSE
#define _DEFAULT_PIX_FORMAT                     CAMERASRC_PIX_UYVY
#define _DEFAULT_FOURCC                         GST_MAKE_FOURCC ('J', 'P', 'E', 'G')
#define _DEFAULT_COLORSPACE                     CAMERASRC_COL_RAW
#define _DEFAULT_CAMERA_ID                      CAMERASRC_DEV_ID_PRIMARY
#define _DEFAULT_NUM_LIVE_BUFFER                0
#define _DEFAULT_BUFFER_COUNT                   0
#define _DEFAULT_BUFFER_CIRULATION_COUNT        0
#define _DEFAULT_BUFFER_RUNNING                 FALSE
#define _DEFAULT_DEQUE_WAITINGTIME              200     /* msec */
#define _DEFAULT_CAP_QUALITY                    GST_CAMERASRC_QUALITY_HIGH
#define _DEFAULT_CAP_JPG_QUALITY                95
#define _DEFAULT_CAP_WIDTH                      1600
#define _DEFAULT_CAP_HEIGHT                     1200
#define _DEFAULT_CAP_COUNT                      1
#define _DEFAULT_CAP_INTERVAL                   1
#define _DEFAULT_CAP_PROVIDE_EXIF               TRUE
#define _DEFAULT_SHOT_MODE              0
#ifdef CONFIG_CAMERA_ZSL_CAPTURE
#define _DEFAULT_ENABLE_ZSL_MODE                TRUE
#else
#define _DEFAULT_ENABLE_ZSL_MODE                FALSE
#endif
#define _DEFAULT_DO_FACE_DETECT                 FALSE
#define _DEFAULT_DO_AF                          FALSE
#define _DEFAULT_SIGNAL_AF                      FALSE
#define _DEFAULT_SIGNAL_STILL_CAPTURE           FALSE
#define _DEFAULT_PREVIEW_WIDTH                  _DEFAULT_WIDTH
#define _DEFAULT_PREVIEW_HEIGHT                 _DEFAULT_HEIGHT
#define _DEFAULT_KEEPING_BUFFER                 0
#define _DEFAULT_NUM_ALLOC_BUF                  6
#define _DEFAULT_FRAME_RATE                         15
#define _DEFAULT_CAP_HDR                       CAMERASRC_HDR_CAPTURE_OFF
#ifdef VDIS
#define _DEFAULT_ENABLE_VDIS_MODE               FALSE
#endif
#define _DEFAULT_ENABLE_HYBRID_MODE             FALSE
#define _DEFAULT_RECORDING_HINT                 FALSE

#define _DEFAULT_LOW_LIGHT_DETECTION        LOW_LIGHT_DETECTION_OFF
#define _DEFAULT_LOW_LIGHT_MODE                 LOW_LIGHT_MODE_OFF

#ifdef DUAL_CAMERA
#define _DEFAULT_DUAL_CAMERA_MODE               FALSE
#endif
#define _DEFAULT_ENABLE_CAMERA_CLOCK_LOCK       FALSE
#define _DEFAULT_VT_MODE                        VT_MODE_OFF

#define _MIN_FPS_FRONT                          8    /* Request from system team */
#define _MIN_FPS_REAR                           8    /* Request from system team */
#define _MIN_FPS_REAR_CONTINOUS                 17    /* Request from system team */
#define _MAX_FPS                                30    /* Max for auto fps */

#define _MAX_NUM_ALLOC_BUF                      100
#define _MAX_TRIAL_WAIT_FRAME                   10
#define _PAD_ALLOC_RETRY_PERIOD                 25
#define _MAX_PAD_ALLOC_RETRY_COUNT              300
#define _MAX_PREVIEW_START_TIMEOUT              100

#define _CONTINUOUS_SHOT_MARGIN                 50

#define _FD_DEFAULT     (-1)
#define _FD_MIN         (-1)
#define _FD_MAX         (1<<15) /* 2^15 == 32768 */

#define _THUMBNAIL_WIDTH    320
#define _THUMBNAIL_HEIGHT   240

#define MM_CAMERA_CH_PREVIEW_MASK    (0x01 << MM_CAMERA_CH_PREVIEW)
#define MM_CAMERA_CH_VIDEO_MASK      (0x01 << MM_CAMERA_CH_VIDEO)
#define MM_CAMERA_CH_SNAPSHOT_MASK   (0x01 << MM_CAMERA_CH_SNAPSHOT)

#define MCI_FN_ENABLE
//#define MCI_FN_ENABLE_PREVIEW
//#define MCI_FN_ENABLE_SNAPSHOT
//#define MCI_FN_ENABLE_VIDEO

#define PREVIEW_BUF_NUMBER                        4
#define VIDEO_BUF_NUMBER                            4

//Assuming preview to be faster than 3.3 FPS
#define PREVIEW_FRAME_MAX_WAIT_TIME     300000
//Wait for 1.5 secs for capture to be completed
#define SNAPSHOT_FRAME_MAX_WAIT_TIME     1500000

#define PREVIEW_FRAME_MEM_ALIGN             1
#define ENABLE_TIMING_DATA                           1

#define CAMERAHAL_CPPINTF_DECL_START extern "C" \
{

#define CAMERAHAL_CPPINTF_DECL_END }

#define CAMERAHAL_CPPINTF_DEF_START    CAMERAHAL_CPPINTF_DECL_START
#define CAMERAHAL_CPPINTF_DEF_END      CAMERAHAL_CPPINTF_DECL_END

#define HDR_CAPTURE_SUPPORT 0

typedef enum {
    CAMERASRC_COLOR_CTRL_BRIGHTNESS = 0,          /**< Brightness control entry*/
    CAMERASRC_COLOR_CTRL_CONTRAST,                /**< Contrast control entry*/
    CAMERASRC_COLOR_CTRL_WHITE_BALANCE,           /**< White balance control entry*/
    CAMERASRC_COLOR_CTRL_COLOR_TONE,              /**< Color tone control entry*/
    CAMERASRC_COLOR_CTRL_SATURATION,              /**< Saturation value control */
    CAMERASRC_COLOR_CTRL_SHARPNESS,               /**< Sharpness value control */
    CAMERASRC_COLOR_CTRL_NUM,                     /**< Number of Controls*/
}camerasrc_color_ctrl_t;

#define CAMERASRC_PARAM_CONTROL_START  (int64_t)(1)
#define CAMERASRC_PARAM_CONTROL_PREVIEW_SIZE CAMERASRC_PARAM_CONTROL_START<<1
#define CAMERASRC_PARAM_CONTROL_PREVIEW_FORMAT CAMERASRC_PARAM_CONTROL_START<<2
#define CAMERASRC_PARAM_CONTROL_VIDEO_SIZE CAMERASRC_PARAM_CONTROL_START<<3
#define CAMERASRC_PARAM_CONTROL_VIDEO_FORMAT CAMERASRC_PARAM_CONTROL_START<<4
#define CAMERASRC_PARAM_CONTROL_RECORDING_HINT CAMERASRC_PARAM_CONTROL_START<<5
#define CAMERASRC_PARAM_CONTROL_SNAPSHOT_SIZE CAMERASRC_PARAM_CONTROL_START<<6
#define CAMERASRC_PARAM_CONTROL_SNAPSHOT_FORMAT CAMERASRC_PARAM_CONTROL_START<<7
#define CAMERASRC_PARAM_CONTROL_FPS_MODE CAMERASRC_PARAM_CONTROL_START<<8
#define CAMERASRC_PARAM_CONTROL_FPS CAMERASRC_PARAM_CONTROL_START<<9
#define CAMERASRC_PARAM_CONTROL_BURST_CONFIG CAMERASRC_PARAM_CONTROL_START<<10
#define CAMERASRC_PARAM_CONTROL_FACE_DETECT CAMERASRC_PARAM_CONTROL_START<<11
#define CAMERASRC_PARAM_CONTROL_FLASH_MODE CAMERASRC_PARAM_CONTROL_START<<12
#define CAMERASRC_PARAM_CONTROL_BRIGHTNESS CAMERASRC_PARAM_CONTROL_START<<13
#define CAMERASRC_PARAM_CONTROL_CONTRAST CAMERASRC_PARAM_CONTROL_START<<14
#define CAMERASRC_PARAM_CONTROL_COLOR_TONE CAMERASRC_PARAM_CONTROL_START<<15
#define CAMERASRC_PARAM_CONTROL_PROGRAM_MODE CAMERASRC_PARAM_CONTROL_START<<16
#define CAMERASRC_PARAM_CONTROL_PARTCOLOR_SRC CAMERASRC_PARAM_CONTROL_START<<17
#define CAMERASRC_PARAM_CONTROL_PARTCOLOR_DST CAMERASRC_PARAM_CONTROL_START<<18
#define CAMERASRC_PARAM_CONTROL_PARTCOLOR_MODE CAMERASRC_PARAM_CONTROL_START<<19
#define CAMERASRC_PARAM_CONTROL_WIDE_DYNAMIC_RANGE CAMERASRC_PARAM_CONTROL_START<<20
#define CAMERASRC_PARAM_CONTROL_SATURATION CAMERASRC_PARAM_CONTROL_START<<21
#define CAMERASRC_PARAM_CONTROL_SHARPNESS CAMERASRC_PARAM_CONTROL_START<<22
#define CAMERASRC_PARAM_CONTROL_PHOTOMETRY CAMERASRC_PARAM_CONTROL_START<<23
#define CAMERASRC_PARAM_CONTROL_METERING CAMERASRC_PARAM_CONTROL_START<<24
#define CAMERASRC_PARAM_CONTROL_WB CAMERASRC_PARAM_CONTROL_START<<25
#define CAMERASRC_PARAM_CONTROL_ISO CAMERASRC_PARAM_CONTROL_START<<26
#define CAMERASRC_PARAM_CONTROL_ANTI_SHAKE CAMERASRC_PARAM_CONTROL_START<<27
#define CAMERASRC_PARAM_CONTROL_LOW_LIGHT_DETECTION CAMERASRC_PARAM_CONTROL_START<<28
#define CAMERASRC_PARAM_CONTROL_BURST_SHOT CAMERASRC_PARAM_CONTROL_START<<29
#define CAMERASRC_PARAM_CONTROL_ZOOM CAMERASRC_PARAM_CONTROL_START<<30
#define CAMERASRC_PARAM_CONTROL_AF CAMERASRC_PARAM_CONTROL_START<<31
#define CAMERASRC_PARAM_CONTROL_AF_AREAS CAMERASRC_PARAM_CONTROL_START<<32
#define CAMERASRC_PARAM_CONTROL_SCENE_MODE CAMERASRC_PARAM_CONTROL_START<<33
#define CAMERASRC_PARAM_CONTROL_HDR CAMERASRC_PARAM_CONTROL_START<<34
#define CAMERASRC_PARAM_CONTROL_PREVIEW_FLIP CAMERASRC_PARAM_CONTROL_START<<35
#define CAMERASRC_PARAM_CONTROL_VIDEO_FLIP CAMERASRC_PARAM_CONTROL_START<<36
#define CAMERASRC_PARAM_CONTROL_SNAPSHOT_FLIP CAMERASRC_PARAM_CONTROL_START<<37
#define CAMERASRC_PARAM_CONTROL_SHOOTING_MODE CAMERASRC_PARAM_CONTROL_START<<38
#ifdef DUAL_CAMERA
#define CAMERASRC_PARAM_CONTROL_DUAL_CAMERA_MODE CAMERASRC_PARAM_CONTROL_START<<39
#endif
#ifdef VDIS
#define CAMERASRC_PARAM_CONTROL_VDIS CAMERASRC_PARAM_CONTROL_START<<40
#endif
#define CAMERASRC_PARAM_CONTROL_JPEG_QUALITY CAMERASRC_PARAM_CONTROL_START<<41
#ifdef DUAL_CAMERA
#define CAMERASRC_PARAM_CONTROL_DUAL_RECORDING_HINT CAMERASRC_PARAM_CONTROL_START<<42
#endif
#define CAMERASRC_PARAM_CONTROL_AE_LOCK CAMERASRC_PARAM_CONTROL_START<<43
#define CAMERASRC_PARAM_CONTROL_AWB_LOCK CAMERASRC_PARAM_CONTROL_START<<44
#define CAMERASRC_PARAM_CONTROL_VT_MODE CAMERASRC_PARAM_CONTROL_START<<45
#define CAMERASRC_PARAM_CONTROL_FPS_RANGE CAMERASRC_PARAM_CONTROL_START<<46
#define CAMERASRC_PARAM_CONTROL_END CAMERASRC_PARAM_CONTROL_START<<47

typedef struct {
    int preview_width;
    int preview_height;
    int preview_format;
    int video_width;
    int video_height;
    int video_format;
    int recording_hint;
    int hfr_value;
    int snapshot_width;
    int snapshot_height;
    int snapshot_format;
    int fps_mode;
    int fps;
    int fps_min;
    int fps_max;
    int burst_number;
    int burst_interval;
    int face_detect;
    int flash_mode;
    int brightness_value;
    int contrast_value;
    int color_tone_value;
    int program_mode;
    int partcolor_src;
    int partcolor_dest;
    int partcolor_mode;
    int wdr_value;
    int saturation_value;
    int sharpness_value;
    int photometry_value;
    int metering_value;
    int wb_value;
    int iso_value;
    int antishake_value;
    int lld_value;
    int burst_shot_config;
    int zoom_value;
    int af_config;
    char * af_area;
    int scene_mode;
    int hdr_ae_bracket;
    int preview_flip;
    int video_flip;
    int snapshot_flip;
    int shoting_mode;
#ifdef DUAL_CAMERA
    int dual_cam_mode;
#endif
#ifdef VDIS
    int vdis_mode;
#endif
    int jpeg_quality;
#ifdef DUAL_CAMERA
    int dual_recording_hint;
#endif
    int ae_lock;
    int awb_lock;
    int vt_mode;
} camerasrc_batch_ctrl_t;

#if ENABLE_TIMING_DATA
#define TIMER_START(arg...) \
if(arg){ \
    g_timer_reset(arg); \
}else{ \
    arg=g_timer_new(); \
}
#define TIMER_MSG(fmt,arg...) \
if(arg){ \
    fprintf(stderr,"\n\x1b[44m\x1b[37m CameraSrc:TIMER [%s:%05d]  " fmt ,__func__, __LINE__,g_timer_elapsed(arg,NULL) ); \
    fprintf(stderr,"\x1b[0m\n"); \
}
#define TIMER_STOP(arg...) \
if(arg){ \
    g_timer_stop(arg); \
    g_timer_destroy(arg); \
    arg = NULL; \
}
#else
#define TIMER_START(arg...)
#define TIMER_MSG(fmt,arg...)
#define TIMER_STOP(arg...)
#endif


#define ION_DEVICE                          "/dev/ion"

#define IMAGE_1MP_WIDTH      1280
#define IMAGE_1MP_HEIGHT      960
#define IMAGE_2MP_WIDTH      1600
#define IMAGE_2MP_HEIGHT     1200
#define IMAGE_3MP_WIDTH      2048
#define IMAGE_3MP_HEIGHT     1536
#define IMAGE_5MP_WIDTH      2592
#define IMAGE_5MP_HEIGHT     1944
#define IMAGE_5MP_BALTIC_WIDTH      2608
#define IMAGE_5MP_BALTIC_HEIGHT     1960

#define FRAME_VGA_WIDTH                             640
#define FRAME_VGA_HEIGHT                            480
#define FRAME_WVGA_WIDTH                        800
#define FRAME_WVGA_HEIGHT                       480


#define MAX_NUM_CTRL_LIST_INFO  64
#define MAX_NUM_CTRL_MENU       64
#define MAX_SZ_CTRL_NAME_STRING 32
#define MAX_SZ_DEV_NAME_STRING  32

#define HDR_BRACKET_FRAME_NUM                   3
#define SCREEN_NAIL_BUFFER_HANDLE           -32

#endif /*__GSTCAMERASRC_DEFS_H__*/
