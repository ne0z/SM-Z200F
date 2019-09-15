/* Copyright (c) 2012, The Linux Foundataion. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __CAMERAHDR_H__
#define __CAMERAHDR_H__

typedef struct _HDRsrc_buffer_t {
    /* Supports for Planes & DMA-buf */
    struct {
        unsigned int length;    /**< Size of stored data */
        unsigned char *start;   /**< Start address of data */
        int fd;                 /* dmabuf-fd */
    } planes[4];                    /* planes: SCMN_IMGB_MAX_PLANE */
    int num_planes;
    int width;                                      /**< width of image */
    int height;                                     /**< height of image */
} HDRsrc_buffer_t;

typedef enum {
	HDR_FORMAT_NONE = 0,
	HDR_FORMAT_NV12,
	HDR_FORMAT_NV21,
	HDR_FORMAT_NV16,
	HDR_FORMAT_NV61,
	HDR_FORMAT_UYVY,
	HDR_FORMAT_YU16
} HDRInputPictureFormat;

typedef enum {
    HDR_PROCESS_NO_ERROR = 0,
    HDR_PROCESS_IN_DATA_ERROR,
    HDR_PROCESS_MEM_ALLOC_ERROR,
    HDR_PROCESS_OPERATION_ERROR
} hdr_err_t;

#define HDR_BRACKET_FRAME_NUM                   3
#define NUM_OF_CPU_CORES                             4
#define YUV422_BUFFER_SIZE(width,height) ( ((width)*(height)) << 1 )

int run_hdr_processing(HDRsrc_buffer_t *input_buffer,unsigned int img_width,unsigned int img_height,HDRInputPictureFormat format,unsigned char **result_data,void* data_cb,void* user_data);

#endif /* __CAMERAHDR_H__ */
