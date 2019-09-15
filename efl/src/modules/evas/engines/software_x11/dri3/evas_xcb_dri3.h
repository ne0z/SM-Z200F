/*
 * Copyright (c) 2015 Samsung, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "../evas_engine.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>

#define DRI3_MAX_BACK   3
#define DRI3_BACK_ID(i) (i)
#define DRI3_FRONT_ID   (DRI3_MAX_BACK)

#define DRI3_NUM_BUFFERS        (1 + DRI3_MAX_BACK)

#define XCB_PACKED   __attribute__((__packed__))

typedef uint32_t xcb_sync_fence_t;

#define XCB_PRESENT_MAJOR_VERSION 1
#define XCB_PRESENT_MINOR_VERSION 0

typedef uint32_t xcb_present_event_t;
typedef uint32_t xcb_randr_crtc_t;

typedef enum xcb_present_event_enum_t {
    XCB_PRESENT_EVENT_CONFIGURE_NOTIFY = 0,
    XCB_PRESENT_EVENT_COMPLETE_NOTIFY = 1,
    XCB_PRESENT_EVENT_IDLE_NOTIFY = 2,
    XCB_PRESENT_EVENT_REDIRECT_NOTIFY = 3
} xcb_present_event_enum_t;

typedef enum xcb_present_event_mask_t {
    XCB_PRESENT_EVENT_MASK_NO_EVENT = 0,
    XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY = 1,
    XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY = 2,
    XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY = 4,
    XCB_PRESENT_EVENT_MASK_REDIRECT_NOTIFY = 8
} xcb_present_event_mask_t;

typedef enum xcb_present_option_t {
    XCB_PRESENT_OPTION_NONE = 0,
    XCB_PRESENT_OPTION_ASYNC = 1,
    XCB_PRESENT_OPTION_COPY = 2,
    XCB_PRESENT_OPTION_UST = 4
} xcb_present_option_t;

typedef enum xcb_present_complete_kind_t {
    XCB_PRESENT_COMPLETE_KIND_PIXMAP = 0,
    XCB_PRESENT_COMPLETE_KIND_NOTIFY_MSC = 1
} xcb_present_complete_kind_t;

typedef enum xcb_present_complete_mode_t {
    XCB_PRESENT_COMPLETE_MODE_COPY = 0,
    XCB_PRESENT_COMPLETE_MODE_FLIP = 1,
    XCB_PRESENT_COMPLETE_MODE_SKIP = 2
} xcb_present_complete_mode_t;

typedef struct xcb_present_generic_event_t {
    uint8_t             response_type;
    uint8_t             extension;
    uint16_t            sequence;
    uint32_t            length;
    uint16_t            evtype;
    uint8_t             pad0[2];
    xcb_present_event_t event;
} xcb_present_generic_event_t;


typedef struct xcb_present_query_version_reply_t {
    uint8_t  response_type;
    uint8_t  pad0;
    uint16_t sequence;
    uint32_t length;
    uint32_t major_version;
    uint32_t minor_version;
} xcb_present_query_version_reply_t;

typedef struct xcb_present_query_version_cookie_t {
    unsigned int sequence;
} xcb_present_query_version_cookie_t;

typedef struct xcb_present_notify_t {
    xcb_window_t window;
    uint32_t     serial;
} xcb_present_notify_t;

#define XCB_PRESENT_CONFIGURE_NOTIFY 0
#define XCB_PRESENT_COMPLETE_NOTIFY 1
#define XCB_PRESENT_IDLE_NOTIFY 2
#define XCB_PRESENT_REDIRECT_NOTIFY 3

typedef struct xcb_present_configure_notify_event_t {
    uint8_t             response_type;
    uint8_t             extension;
    uint16_t            sequence;
    uint32_t            length;
    uint16_t            event_type;
    uint8_t             pad0[2];
    xcb_present_event_t event;
    xcb_window_t        window;
    int16_t             x;
    int16_t             y;
    uint16_t            width;
    uint16_t            height;
    int16_t             off_x;
    int16_t             off_y;
    uint32_t            full_sequence;
    uint16_t            pixmap_width;
    uint16_t            pixmap_height;
    uint32_t            pixmap_flags;
} xcb_present_configure_notify_event_t;

typedef struct xcb_present_complete_notify_event_t {
    uint8_t             response_type;
    uint8_t             extension;
    uint16_t            sequence;
    uint32_t            length;
    uint16_t            event_type;
    uint8_t             kind;
    uint8_t             mode;
    xcb_present_event_t event;
    xcb_window_t        window;
    uint32_t            serial;
    uint64_t            ust;
    uint32_t            full_sequence;
    uint64_t            msc;
} XCB_PACKED xcb_present_complete_notify_event_t;

typedef struct xcb_present_idle_notify_event_t {
    uint8_t             response_type;
    uint8_t             extension;
    uint16_t            sequence;
    uint32_t            length;
    uint16_t            event_type;
    uint8_t             pad0[2];
    xcb_present_event_t event;
    xcb_window_t        window;
    uint32_t            serial;
    xcb_pixmap_t        pixmap;
    xcb_sync_fence_t    idle_fence;
    uint32_t            full_sequence;
} xcb_present_idle_notify_event_t;

typedef uint32_t xcb_xfixes_region_t;


typedef struct xcb_special_event xcb_special_event_t;


#define XCB_DRI3_MAJOR_VERSION 1
#define XCB_DRI3_MINOR_VERSION 0


/**
 * @brief xcb_dri3_query_version_cookie_t
 **/
typedef struct xcb_dri3_query_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_query_version_cookie_t;

/** Opcode for xcb_dri3_query_version. */
#define XCB_DRI3_QUERY_VERSION 0


/**
 * @brief xcb_dri3_query_version_reply_t
 **/
typedef struct xcb_dri3_query_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t major_version; /**<  */
    uint32_t minor_version; /**<  */
} xcb_dri3_query_version_reply_t;

/**
 * @brief xcb_dri3_open_cookie_t
 **/
typedef struct xcb_dri3_open_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_open_cookie_t;


/**
 * @brief xcb_dri3_open_reply_t
 **/
typedef struct xcb_dri3_open_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  nfd; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad0[24]; /**<  */
} xcb_dri3_open_reply_t;

/**
 * @brief xcb_dri3_buffer_from_pixmap_cookie_t
 **/
typedef struct xcb_dri3_buffer_from_pixmap_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_buffer_from_pixmap_cookie_t;

/**
 * @brief xcb_dri3_buffer_from_pixmap_reply_t
 **/
typedef struct xcb_dri3_buffer_from_pixmap_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  nfd; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t size; /**<  */
    uint16_t width; /**<  */
    uint16_t height; /**<  */
    uint16_t stride; /**<  */
    uint8_t  depth; /**<  */
    uint8_t  bpp; /**<  */
    uint8_t  pad0[12]; /**<  */
} xcb_dri3_buffer_from_pixmap_reply_t;


typedef struct _dri3_info {
   xcb_connection_t    *conn;     /* connection to xserver */
   Display             *disp;
   int         	      drm_fd;
   tbm_bufmgr           bufmgr;
} dri3_info;

typedef struct _dri3_buffer {
   int                  size;
   int                  w;
   int                  h;
   int                  stride;
   int                  depth;
   int                  bpp;
   int                  tbm_fd;
   int                  buffer_fd;
   tbm_bo               bo;
   Pixmap               pixmap;

   struct xshmfence     *shm_fence;
   int                  fence_fd;
   xcb_sync_fence_t     sync_fence;

   struct xshmfence     *wait_shm_fence;
   int                  wait_fence_fd;
   xcb_sync_fence_t     wait_sync_fence;

   int                  busy;
   uint64_t             last_swap;
   Eina_Bool            own_pixmap;
} dri3_buffer;

typedef struct _dri3_drawable {
   Drawable             window;
   int                  width;
   int                  height;
   int                  depth;
   int                  swap_interval; /* swap interval */

   /* SBC numbers are tracked by using the serial numbers
    * in the present request and complete events
    */
   uint64_t             send_sbc;
   uint64_t             recv_sbc;

   /* Last received UST/MSC values */
   uint64_t             ust, msc;

   /* Serial numbers for tracking wait_for_msc events */
   uint32_t             send_msc_serial;
   uint32_t             recv_msc_serial;

   dri3_buffer          *buffers[DRI3_NUM_BUFFERS];
   dri3_buffer          *back_buffer;
   int                  cur_back_id;
   int                  num_back;
   int                  buffer_age;

   uint32_t             *stamp;

   xcb_present_event_t  eid;
   xcb_gcontext_t       gc;
   xcb_special_event_t  *special_event;

   uint8_t              flipping;

} dri3_drawable;

typedef struct _dri3_fence {
   struct               xshmfence *shm_fence;
   int                  fence_fd;
   xcb_sync_fence_t     sync_fence;
} dri3_fence;

dri3_info info;

/* dri3 initialization */
int   dri3_init_dri3(Display *dpy);
void  dri3_deinit_dri3();

int   dri3_query_dri3();
int   dri3_query_present();
int   dri3_XFixes_query(Display *disp);


/* dri3 buffer */
dri3_buffer  *dri3_get_pixmap_buffer(Pixmap pixmap);
void          dri3_destroy_buffer(dri3_buffer *buffer);

/* dri3 fence */
void  dri3_fence_set (dri3_buffer *buffer);
void  dri3_fence_reset (dri3_buffer *buffer);
void  dri3_fence_trigger (dri3_buffer *buffer);
int   dri3_fence_triggered (dri3_buffer *buffer);
void  dri3_fence_await (dri3_buffer *buffer);

void  dri3_wait_fence_set (dri3_buffer *buffer);
void  dri3_wait_fence_reset (dri3_buffer *buffer);
void  dri3_wait_fence_trigger (dri3_buffer *buffer);
int   dri3_wait_fence_triggered (dri3_buffer *buffer);
void  dri3_wait_fence_await (dri3_buffer *buffer);



void  dri3_tbm_import(dri3_buffer *buffer);
void  *dri3_tbm_bo_handle_map(dri3_buffer *buffer);
void  dri3_tbm_bo_handle_unmap(dri3_buffer *buffer);
void  dri3_tbm_bo_handle_unref(dri3_buffer *buffer);

/*dri3 drawables */
dri3_drawable *dri3_create_drawable (XID window, int w, int h, int depth);
void  dri3_destroy_drawable (dri3_drawable *drawable);

XID   dri3_XFixes_create_region(Display *disp,XRectangle *xrects, int nrects);
void  dri3_XFixes_destory_region(Display *disp, XID region);
void  dri3_get_data (dri3_buffer *buffer, RGBA_Image *im);
dri3_buffer *dri3_get_backbuffers (dri3_drawable *drawable, uint32_t *stamp);
int64_t dri3_swap_buffers (dri3_drawable *drawable, int64_t target_msc, int64_t divisor, int64_t remainder, XID region);
int   dri3_get_buffer_age(dri3_drawable *drawable);

int   dri3_drawable_get_msc (dri3_drawable *drawable, int64_t *ust, int64_t *msc, int64_t *sbc);
int   dri3_wait_for_msc (dri3_drawable *drawable, int64_t target_msc, int64_t divisor, int64_t remainder, int64_t *ust, int64_t *msc, int64_t *sbc);
int   dri3_wait_for_sbc (dri3_drawable *drawable, uint64_t target_sbc, uint64_t *ust, uint64_t *msc, uint64_t *sbc);
void  dri3_set_swap_interval (dri3_drawable *drawable, int interval);
int   dri3_get_swap_interval (dri3_drawable *drawable);


