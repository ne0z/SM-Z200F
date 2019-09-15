#ifndef __VIDEOEFFECTFILTER_UTILITY_H__
#define __VIDEOEFFECTFILTER_UTILITY_H__
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/Xlib.h>
#include <TizenGLManager.h>
#include <SecImagingNativeFilterDefs.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>
#include <tbm_bufmgr.h>
#include <exynos_drm.h>
#include <sys/ioctl.h>
#include <mm_types.h>
#include <glib.h>
#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>
#include "Image_filter.h"
#include <stdio.h>

#define startfunc               GST_DEBUG("+-  START -------------------------");
#define endfunc                 GST_DEBUG("+-  END  --------------------------");

#define IF_FREE(object) do { \
    if(object) { \
        free(object); \
        object = NULL; \
    } \
} while (0)


#define SURFACE_COUNT 6
#define MAX_INPUT_BUFFER 30

typedef struct _InputBuffer{
    int dma_buf_fd;
    int gem_handle;
    tbm_bo bo;
    tbm_surface_h surface;
}InputBuffer;

typedef struct _Appdata{
    guint32 fourcc;

    int fd;
    tbm_bufmgr bufmgr;

    Display *display;
    Window window;

    TizenGLManager_t *manager;
    tbm_surface_h surface;
    tbm_bo input_bo;

    tbm_surface_h output_surface;

    int width;
    int height;
    int buf_share_method;
    int pix_index;

    image_filter_h image_filter_handle;
    gboolean ready_for_glm;
    gboolean has_created_glm;
    gboolean surface_using[SURFACE_COUNT];
    tbm_surface_h surface_buffer[SURFACE_COUNT];

    InputBuffer in_buf[MAX_INPUT_BUFFER];

}AppData;

typedef struct _OutputData {
    tbm_surface_h surface;
    int buf_share_method;
    gint pixmap_id;
    GstClockTime timestamp;
    GstClockTime duration;
} OutputData;

typedef struct __videffect_buffer{

    unsigned int size;
    unsigned char* data;
    int width;
    int height;

}videffect_buffer;

gint get_available_pixmap_id(AppData *app_data);

void init_gl_manager(AppData *app_data);
void create_gl_manager(AppData *app_data);
void create_window(AppData *app_data);

void destroy_window(AppData *app_data);
void destroy_gl_manager(AppData *app_data);
void destroy_app_data(AppData *app_data);

AppData *init_app_data();

int tbm_export_buffer(AppData *app_data, tbm_bo bo, tbm_surface_info_s surface_info, tbm_surface_h *buffer);
OutputData* process_input_buffer(AppData *app_data, GstBuffer *buffer);
#endif
