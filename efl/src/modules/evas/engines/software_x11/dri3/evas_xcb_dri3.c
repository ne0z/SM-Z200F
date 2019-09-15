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



#include "evas_common_private.h"
#include "evas_xcb_dri3.h"
#include <dlfcn.h>      /* dlopen,dlclose,etc */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SIZE_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#define ALIGNMENT_PITCH_ARGB 64
////////////////////////////////////
// libxcb-dri3.so
static void *lib_dri3 = NULL;
static xcb_dri3_open_cookie_t (*sym_xcb_dri3_open) (xcb_connection_t *c, Window drawable, uint32_t provider) = NULL;
static xcb_dri3_open_reply_t *(*sym_xcb_dri3_open_reply) (xcb_connection_t *c, xcb_dri3_open_cookie_t cookie, xcb_generic_error_t **e) = NULL;
static int *(*sym_xcb_dri3_open_reply_fds) (xcb_connection_t *c, xcb_dri3_open_reply_t *reply) = NULL;
static xcb_dri3_query_version_cookie_t (*sym_xcb_dri3_query_version) (xcb_connection_t *c, uint32_t major_version, uint32_t minor_version) = NULL;
static xcb_dri3_query_version_reply_t *(*sym_xcb_dri3_query_version_reply) (xcb_connection_t *c, xcb_dri3_query_version_cookie_t cookie, xcb_generic_error_t **e) = NULL;
static xcb_dri3_buffer_from_pixmap_cookie_t (*sym_xcb_dri3_buffer_from_pixmap) (xcb_connection_t *c, Pixmap pixmap) = NULL;
static xcb_dri3_buffer_from_pixmap_reply_t *(*sym_xcb_dri3_buffer_from_pixmap_reply) (xcb_connection_t *c, xcb_dri3_buffer_from_pixmap_cookie_t cookie, xcb_generic_error_t **e) = NULL;
static int *(*sym_xcb_dri3_buffer_from_pixmap_reply_fds) (xcb_connection_t *c, xcb_dri3_buffer_from_pixmap_reply_t *reply) = NULL;
static xcb_void_cookie_t (*sym_xcb_dri3_fence_from_fd_checked) (xcb_connection_t *c, Pixmap drawable, uint32_t fence, uint8_t initially_triggered, int32_t fence_fd) = NULL;
static xcb_void_cookie_t (*sym_xcb_dri3_pixmap_from_buffer_checked) (xcb_connection_t *c,
                                                                     Pixmap pixmap,
                                                                     Drawable drawable,
                                                                     uint32_t size,
                                                                     uint16_t width,
                                                                     uint16_t height,
                                                                     uint16_t stride,
                                                                     uint8_t depth,
                                                                     uint8_t bpp,
                                                                     int32_t pixmap_fd) = NULL;
static xcb_void_cookie_t (*sym_xcb_dri3_pixmap_from_buffer) (xcb_connection_t *c,
                                                             Pixmap pixmap,
                                                             Drawable drawable,
                                                             uint32_t size,
                                                             uint16_t width,
                                                             uint16_t height,
                                                             uint16_t stride,
                                                             uint8_t depth,
                                                             uint8_t bpp,
                                                             int32_t pixmap_fd) = NULL;
////////////////////////////////////


////////////////////////////////////
// libxcb-present.so
static void *lib_present = NULL;
static xcb_present_query_version_cookie_t (*sym_xcb_present_query_version) (xcb_connection_t *c, uint32_t major_version, uint32_t minor_version) = NULL;
static xcb_present_query_version_reply_t *(*sym_xcb_present_query_version_reply) (xcb_connection_t *c, xcb_present_query_version_cookie_t cookie, xcb_generic_error_t **e) = NULL;
static xcb_void_cookie_t (*sym_xcb_present_select_input_checked) (xcb_connection_t *c, xcb_present_event_t eid, Drawable window, uint32_t event_mask) = NULL;
static xcb_void_cookie_t (*sym_xcb_present_pixmap) (xcb_connection_t *c, xcb_window_t window, xcb_pixmap_t pixmap,
                                                    uint32_t serial, xcb_xfixes_region_t valid,
                                                    xcb_xfixes_region_t update, int16_t x_off, int16_t y_off,
                                                    xcb_randr_crtc_t target_crtc, xcb_sync_fence_t wait_fence,
                                                    xcb_sync_fence_t idle_fence, uint32_t options, uint64_t target_msc,
                                                    uint64_t divisor, uint64_t remainder, uint32_t notifies_len,
                                                    const xcb_present_notify_t *notifies) = NULL;
static xcb_void_cookie_t (*sym_xcb_present_pixmap_checked) (xcb_connection_t *c, Drawable window, Pixmap pixmap, uint32_t serial,
                                                            xcb_xfixes_region_t valid, XID update, int16_t x_off, int16_t y_off,
                                                            xcb_randr_crtc_t target_crtc, xcb_sync_fence_t wait_fence,
                                                            xcb_sync_fence_t idle_fence, uint32_t options, uint64_t target_msc, uint64_t divisor,
                                                            uint64_t remainder, uint32_t notifies_len, const xcb_present_notify_t *notifies) = NULL;
static xcb_void_cookie_t (*sym_xcb_present_notify_msc) (xcb_connection_t *c,
                                                        Drawable window,
                                                        uint32_t serial,
                                                        uint64_t target_msc,
                                                        uint64_t divisor,
                                                        uint64_t remainder) = NULL;
static xcb_extension_t *sym_xcb_present_id = NULL;
////////////////////////////////////

////////////////////////////////////
//libxshmfence.so.1
static void *lib_xshmfence = NULL;
static int (*sym_xshmfence_alloc_shm) (void) = NULL;
static struct xshmfence *(*sym_xshmfence_map_shm) (int fd) = NULL;
static void (*sym_xshmfence_unmap_shm) (struct xshmfence *f) = NULL;
static int (*sym_xshmfence_trigger)(struct xshmfence *f) = NULL;
static int (*sym_xshmfence_await)(struct xshmfence *f) = NULL;
static int (*sym_xshmfence_query)(struct xshmfence *f) = NULL;
static void (*sym_xshmfence_reset)(struct xshmfence *f) = NULL;
////////////////////////////////////

////////////////////////////////////
//libxcb-sync.so
static void *lib_sync = NULL;
static xcb_void_cookie_t (*sym_xcb_sync_destroy_fence) (xcb_connection_t *c, xcb_sync_fence_t fence) = NULL;
static xcb_void_cookie_t (*sym_xcb_sync_trigger_fence) (xcb_connection_t *c, xcb_sync_fence_t fence) = NULL;
////////////////////////////////////

////////////////////////////////////
//libX11-xcb.so
static void *lib_xcb = NULL;
static xcb_connection_t *(*sym_XGetXCBConnection)(Display *dpy) = NULL;
////////////////////////////////////


////////////////////////////////////
// libXfixes.so.3
static void *xfixes_lib = NULL;
static Bool (*sym_XFixesQueryExtension) (Display *display, int *event_base_return, int *error_base_return) = NULL;
static Status (*sym_XFixesQueryVersion) (Display *display, int *major_version_return, int *minor_version_return) = NULL;
static XID (*sym_XFixesCreateRegion) (Display *display, XRectangle *rectangles, int nrectangles) = NULL;
static void (*sym_XFixesDestroyRegion) (Display *dpy, XID region) = NULL;
////////////////////////////////////////////////////////////////////////////

static int xfixes_ev_base = 0, xfixes_err_base = 0;
static int xfixes_major = 0, xfixes_minor = 0;

static Eina_Bool
_lib_init()
{
   if (xfixes_lib) return EINA_TRUE;
   lib_dri3 = dlopen("libxcb-dri3.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib_dri3)
      {
         WRN("Can't load libxcb-dri3.so");
         goto err;
      }
   lib_present = dlopen("libxcb-present.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib_present)
      {
         WRN("Can't load libxcb-present.so");
         goto err;
      }
   lib_xshmfence = dlopen("libxshmfence.so.1", RTLD_NOW | RTLD_LOCAL);
   if (!lib_xshmfence)
      {
         WRN("Can't load libxshmfence.so.1");
         goto err;
      }
   lib_sync = dlopen("libxcb-sync.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib_sync)
      {
         WRN("Can't load libxcb-sync.so");
         goto err;
      }
   lib_xcb = dlopen("libX11-xcb.so", RTLD_NOW | RTLD_LOCAL);
   if (!lib_xcb)
      lib_xcb = dlopen("libX11-xcb.so.1", RTLD_NOW | RTLD_LOCAL);
   if (!lib_xcb)
      lib_xcb = dlopen("libX11-xcb.so.1.0.0", RTLD_NOW | RTLD_LOCAL);
   if (!lib_xcb)
      {
         WRN("Can't load libX11-xcb.so");
         goto err;
      }
   xfixes_lib = dlopen("libXfixes.so.3", RTLD_NOW | RTLD_LOCAL);
   if (!xfixes_lib)
      {
         WRN("Can't load libXfixes.so.3");
         goto err;
      }
#define SYM(l, x) \
      do { sym_ ## x = dlsym(l, #x); \
      if (!sym_ ## x) { \
            WRN("Can't load symbol "#x); \
            goto err; \
      } \
      } while (0)

   SYM(lib_dri3, xcb_dri3_open);
   SYM(lib_dri3, xcb_dri3_open_reply);
   SYM(lib_dri3, xcb_dri3_open_reply_fds);
   SYM(lib_dri3, xcb_dri3_query_version);
   SYM(lib_dri3, xcb_dri3_query_version_reply);
   SYM(lib_dri3, xcb_dri3_buffer_from_pixmap);
   SYM(lib_dri3, xcb_dri3_buffer_from_pixmap_reply);
   SYM(lib_dri3, xcb_dri3_buffer_from_pixmap_reply_fds);
   SYM(lib_dri3, xcb_dri3_fence_from_fd_checked);
   SYM(lib_dri3, xcb_dri3_pixmap_from_buffer_checked);
   SYM(lib_dri3, xcb_dri3_pixmap_from_buffer);


   SYM(lib_sync, xcb_sync_destroy_fence);
   SYM(lib_sync, xcb_sync_trigger_fence);

   SYM(lib_present, xcb_present_query_version);
   SYM(lib_present, xcb_present_query_version_reply);
   SYM(lib_present, xcb_present_select_input_checked);
   SYM(lib_present, xcb_present_pixmap);
   SYM(lib_present, xcb_present_pixmap_checked);
   SYM(lib_present, xcb_present_notify_msc);
   SYM(lib_present, xcb_present_id);

   SYM(lib_xshmfence, xshmfence_alloc_shm);
   SYM(lib_xshmfence, xshmfence_map_shm);
   SYM(lib_xshmfence, xshmfence_unmap_shm);
   SYM(lib_xshmfence, xshmfence_trigger);
   SYM(lib_xshmfence, xshmfence_await);
   SYM(lib_xshmfence, xshmfence_query);
   SYM(lib_xshmfence, xshmfence_reset);

   SYM(lib_xcb, XGetXCBConnection);

   SYM(xfixes_lib, XFixesQueryExtension);
   SYM(xfixes_lib, XFixesQueryVersion);
   SYM(xfixes_lib, XFixesCreateRegion);
   SYM(xfixes_lib, XFixesDestroyRegion);

   return EINA_TRUE;
   err:
   if (lib_dri3)
      {
         dlclose(lib_dri3);
         lib_dri3 = NULL;
      }
   if (lib_present)
      {
         dlclose(lib_present);
         lib_present = NULL;
      }
   if (lib_xshmfence)
      {
         dlclose(lib_xshmfence);
         lib_xshmfence = NULL;
      }
   if (lib_sync)
      {
         dlclose(lib_sync);
         lib_sync = NULL;
      }
   if (lib_xcb)
      {
         dlclose(lib_xcb);
         lib_xcb = NULL;
      }
   if (xfixes_lib)
      {
         dlclose(xfixes_lib);
         xfixes_lib = NULL;
      }
   return EINA_FALSE;
}


void
dri3_fence_reset(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   sym_xshmfence_reset(buffer->shm_fence);
}

void
dri3_fence_set(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   sym_xshmfence_trigger(buffer->shm_fence);
}

void
dri3_fence_trigger(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;
   if (!buffer->sync_fence) return;

   sym_xcb_sync_trigger_fence(info.conn, buffer->sync_fence);
}

void
dri3_fence_await(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   xcb_flush(info.conn);
   sym_xshmfence_await(buffer->shm_fence);
}

int
dri3_fence_triggered(dri3_buffer *buffer)
{
   if (!buffer) return 0;
   if (!buffer->shm_fence) return 0;

   return sym_xshmfence_query(buffer->shm_fence);
}


void
dri3_wait_fence_set(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   sym_xshmfence_trigger(buffer->wait_shm_fence);
}

void
dri3_wait_fence_reset(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   sym_xshmfence_reset(buffer->wait_shm_fence);
}

void
dri3_wait_fence_trigger(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;
   if (!buffer->sync_fence) return;

   sym_xcb_sync_trigger_fence(info.conn, buffer->wait_sync_fence);
}

int
dri3_wait_fence_triggered(dri3_buffer *buffer)
{
   if (!buffer) return 0;
   if (!buffer->shm_fence) return 0;

   return sym_xshmfence_query(buffer->wait_shm_fence);
}

void
dri3_wait_fence_await(dri3_buffer *buffer)
{
   if (!buffer) return;
   if (!buffer->shm_fence) return;

   xcb_flush(info.conn);
   sym_xshmfence_await(buffer->wait_shm_fence);
}


static void
_dri3_update_num_back(dri3_drawable *drawable)
{
   drawable->num_back = 1;
   if (drawable->flipping)
      drawable->num_back++;
   if (drawable->swap_interval == 0)
      drawable->num_back++;
}

static void
dri3_destroy_pixmap(xcb_pixmap_t pixmap)
{
   if (!pixmap) return;

   xcb_void_cookie_t cookie;
   xcb_generic_error_t *error;

   cookie = xcb_free_pixmap_checked(info.conn, pixmap);
   error = xcb_request_check(info.conn, cookie);
   if (error)
      {
         WRN("xcb_free_pixmap_checked() has failed. (error_code : %d)", error->error_code);
         free(error);
      }
}

/** dri3_destroy_buffer
 *
 * Free everything associated with one render buffer including pixmap, fence
 * stuff and the tbm
 */
void
dri3_destroy_buffer(dri3_buffer *buffer)
{
   if (!buffer) return;

   if (buffer->own_pixmap) dri3_destroy_pixmap(buffer->pixmap);
   if (buffer->sync_fence > 0 && info.conn) sym_xcb_sync_destroy_fence(info.conn, buffer->sync_fence);
   if (buffer->shm_fence) sym_xshmfence_unmap_shm(buffer->shm_fence);
   if (buffer->bo) sym_tbm_bo_unref(buffer->bo);
   if (buffer->buffer_fd > 0) close(buffer->buffer_fd);
   free(buffer);
}

void
dri3_destroy_drawable(dri3_drawable *drawable)
{
   if (!drawable) return;

   int i;
   for (i = 0; i < DRI3_NUM_BUFFERS; i++)
      {
         if (drawable->buffers[i])
            dri3_destroy_buffer (drawable->buffers[i]);
      }

   if (drawable->special_event)
      xcb_unregister_for_special_event(info.conn, drawable->special_event);

   free(drawable);
}


dri3_drawable *
dri3_create_drawable(XID window, int w EINA_UNUSED, int h EINA_UNUSED, int depth)
{
   dri3_drawable *drawable;

   drawable = calloc(1, sizeof(dri3_drawable));
   if (!drawable) return NULL;

   drawable->window = window;
   drawable->depth = depth;
   drawable->swap_interval = 0; /* default interval is 1 */

   _dri3_update_num_back(drawable);

   return drawable;
}



/*
 * Process one Present event
 */
static void
_dri3_handle_present_event(dri3_drawable *drawable, xcb_present_generic_event_t *ge)
{
   switch (ge->evtype) {
      case XCB_PRESENT_CONFIGURE_NOTIFY:
         {
            xcb_present_configure_notify_event_t *ce = (void *) ge;

            drawable->width = ce->width;
            drawable->height = ce->height;
            break;
         }
      case XCB_PRESENT_COMPLETE_NOTIFY:
         {
            xcb_present_complete_notify_event_t *ce = (void *) ge;

            /* Compute the processed SBC number from the received 32-bit serial number merged
             * with the upper 32-bits of the sent 64-bit serial number while checking for
             * wrap
             */
            if (ce->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP)
               {
                  drawable->recv_sbc = (drawable->send_sbc & 0xffffffff00000000LL) | ce->serial;
                  if (drawable->recv_sbc > drawable->send_sbc)
                     drawable->recv_sbc -= 0x100000000;
                  switch (ce->mode)
                  {
                     case XCB_PRESENT_COMPLETE_MODE_FLIP:
                        drawable->flipping = 1;
                        break;
                     case XCB_PRESENT_COMPLETE_MODE_COPY:
                        drawable->flipping = 0;
                        break;
                  }
                  _dri3_update_num_back(drawable);
               }
            else
               {
                  drawable->recv_msc_serial = ce->serial;
                  DBG("Get complete notify MSC");
               }
            drawable->ust = ce->ust;
            drawable->msc = ce->msc;
            break;
         }
      case XCB_PRESENT_EVENT_IDLE_NOTIFY:
         {
            xcb_present_idle_notify_event_t *ie = (void *) ge;
            unsigned int b;

            for (b = 0; b < sizeof(drawable->buffers) / sizeof(drawable->buffers[0]); b++)
               {
                  dri3_buffer *buffer = drawable->buffers[b];

                  if (buffer && buffer->pixmap == ie->pixmap)
                     {
                        buffer->busy = 0;
                        if ((unsigned int)drawable->num_back <= b && b < DRI3_MAX_BACK)
                           {
                              dri3_destroy_buffer(buffer);
                              drawable->buffers[b] = NULL;
                           }
                        break;
                     }
               }
            break;
         }
      default :
         {
            DBG ("Get unknown notify");
            break;
         }
   }
   free(ge);
}



static int
_dri3_wait_for_event(dri3_drawable *drawable)
{
   xcb_generic_event_t *ev;
   xcb_present_generic_event_t *ge;

   xcb_flush (info.conn);
   ev = xcb_wait_for_special_event(info.conn, drawable->special_event);
   if (!ev)
      return 0;
   ge = (void *) ev;
   _dri3_handle_present_event(drawable, ge);

   return 1;
}



/**
 * Get the X server to send an event when the target msc/divisor/remainder is
 * reached.
 */
int
dri3_wait_for_msc(dri3_drawable *drawable, int64_t target_msc, int64_t divisor,
                  int64_t remainder, int64_t *ust, int64_t *msc, int64_t *sbc)
{
   if (!drawable) return 0;

   uint32_t msc_serial;

   /* Ask for the an event for the target MSC */
   msc_serial = ++drawable->send_msc_serial;
   sym_xcb_present_notify_msc(info.conn,
                              drawable->window,
                              msc_serial,
                              target_msc,
                              divisor,
                              remainder);

   xcb_flush(info.conn);

   /* Wait for the event */
   if (drawable->special_event)
      {
         while ((int32_t) (msc_serial - drawable->recv_msc_serial) > 0)
            {
               if (!_dri3_wait_for_event(drawable))
                  return 0;
            }
      }

   *ust = drawable->ust;
   *msc = drawable->msc;
   *sbc = drawable->recv_sbc;

   return 1;
}

/** dri3_drawable_get_msc
 *
 * Return the current UST/MSC/SBC triplet by asking the server
 * for an event
 */
int
dri3_drawable_get_msc(dri3_drawable *drawable, int64_t *ust, int64_t *msc, int64_t *sbc)
{
   if (!drawable) return 0;

   return dri3_wait_for_msc(drawable, 0, 0, 0, ust, msc,sbc);
}

/** dri3_wait_for_sbc
 *
 * Wait for the completed swap buffer count to reach the specified
 * target. Presumably the application knows that this will be reached with
 * outstanding complete events, or we're going to be here awhile.
 */
int
dri3_wait_for_sbc(dri3_drawable *drawable, uint64_t target_sbc, uint64_t *ust, uint64_t *msc, uint64_t *sbc)
{
   if (!drawable) return 0;

   while (drawable->recv_sbc < target_sbc)
      {
         if (!_dri3_wait_for_event(drawable))
            return 0;
      }

   *ust = drawable->ust;
   *msc = drawable->msc;
   *sbc = drawable->recv_sbc;

   return 1;
}

#if 0 // NOT USED
static xcb_gcontext_t
_dri3_drawable_gc(dri3_drawable *drawable)
{
   if (!drawable->gc)
      {
         uint32_t v = 0;

         xcb_create_gc(info.conn,
                       (drawable->gc = xcb_generate_id(info.conn)),
                       drawable->window,
                       XCB_GC_GRAPHICS_EXPOSURES,
                       &v);
      }
   return drawable->gc;
}


static void
dri3_copy_area(xcb_drawable_t    src_drawable,
               xcb_drawable_t    dst_drawable,
               xcb_gcontext_t    gc,
               int16_t           src_x,
               int16_t           src_y,
               int16_t           dst_x,
               int16_t           dst_y,
               uint16_t          width,
               uint16_t          height)
{
   xcb_void_cookie_t cookie;

   cookie = xcb_copy_area_checked(info.conn,
                                  src_drawable,
                                  dst_drawable,
                                  gc,
                                  src_x,
                                  src_y,
                                  dst_x,
                                  dst_y,
                                  width,
                                  height);
   xcb_discard_reply(info.conn, cookie.sequence);
}
#endif

/** dri3_error_free
 *
 * Free the back buffer.
 */
static void *
dri3_error_free(dri3_buffer *buffer)
{
   if (!buffer) return NULL;

   if (buffer->shm_fence) sym_xshmfence_unmap_shm(buffer->shm_fence);
   else if (buffer->fence_fd > 0) close(buffer->fence_fd);
   if (buffer->bo) sym_tbm_bo_unref(buffer->bo);
   if (buffer->buffer_fd > 0) close(buffer->buffer_fd);
   free(buffer);
   return NULL;
}

static dri3_buffer *
dri3_alloc_render_buffer(Drawable draw, int w, int h, int depth, int bpp)
{
   dri3_buffer *buffer;

   buffer = calloc(1, sizeof(dri3_buffer));
   if (!buffer) return NULL;

   /* Create an xshmfence object and
    * prepare to send that to the X server
    */
   buffer->fence_fd = sym_xshmfence_alloc_shm();
   if (buffer->fence_fd < 0) return dri3_error_free(buffer);
   buffer->shm_fence = sym_xshmfence_map_shm(buffer->fence_fd);
   if (!buffer->shm_fence) return dri3_error_free(buffer);
   buffer->sync_fence = 0; /* sync_fence is set at create_pixmap_from_buffer */

   buffer->stride = SIZE_ALIGN((w * bpp)>>3, ALIGNMENT_PITCH_ARGB);
   buffer->size = h * buffer->stride;
   buffer->w = w;
   buffer->h = h;
   buffer->depth = depth;
   buffer->bpp = bpp;
   buffer->bo = sym_tbm_bo_alloc(info.bufmgr, buffer->size, TBM_BO_DEFAULT);
   if (!buffer->bo) return dri3_error_free(buffer);
   buffer->tbm_fd = sym_tbm_bo_export_fd(buffer->bo);
   if (buffer->tbm_fd < 0) return dri3_error_free(buffer);

   xcb_void_cookie_t cookie;
   xcb_generic_error_t *error;

   Pixmap pixmap = xcb_generate_id(info.conn);

   cookie = sym_xcb_dri3_pixmap_from_buffer_checked(info.conn,
                                                    pixmap,
                                                    draw,
                                                    buffer->size,
                                                    buffer->w,
                                                    buffer->h,
                                                    buffer->stride,
                                                    buffer->depth,
                                                    buffer->bpp,
                                                    buffer->tbm_fd);
   error = xcb_request_check(info.conn, cookie);
   if (error)
      {
         ERR("xcb_dri3_pixmap_from_buffer_checked() has failed. (error_code : %d)", error->error_code);
         free(error);
         return dri3_error_free(buffer);
      }

   buffer->pixmap = pixmap;
   buffer->own_pixmap = EINA_TRUE;

   if (!buffer->sync_fence)
      {
         xcb_sync_fence_t sync_fence;
         xcb_void_cookie_t fence_cookie;

         sync_fence = xcb_generate_id(info.conn);
         fence_cookie = sym_xcb_dri3_fence_from_fd_checked(info.conn,
                                                           pixmap,
                                                           sync_fence,
                                                           0,
                                                           buffer->fence_fd);
         error = xcb_request_check(info.conn, fence_cookie);
         if (error)
            {
               ERR("xcb_dri3_fence_from_fd_checked() has failed. (error_code : %d)", error->error_code);
               if (sync_fence > 0) sym_xcb_sync_destroy_fence(info.conn, sync_fence);
               free(error);
               return dri3_error_free(buffer);
            }
         buffer->sync_fence = sync_fence;
      }

   /* Mark the buffer as idle
    */
   dri3_fence_set(buffer);
   return buffer;
}

/** dri3_flush_present_events
 *
 * Process any present events that have been received from the X server
 */
static void
_dri3_flush_present_events(dri3_drawable *drawable)
{
   /* Check to see if any configuration changes have occurred
    * since we were last invoked
    */
   if (drawable->special_event)
      {
         xcb_generic_event_t *ev;

         while ((ev = xcb_poll_for_special_event(info.conn, drawable->special_event)) != NULL)
            {
               xcb_present_generic_event_t *ge = (void *) ev;
               _dri3_handle_present_event(drawable, ge);
            }
      }
}


/** dri3_update_drawable
 *
 * Called the first time we use the drawable and then
 * after we receive present configure notify events to
 * track the geometry of the drawable
 */
static int
_dri3_update_drawable(dri3_drawable *drawable)
{
   /* First time through, go get the current drawable geometry
    */
   if (drawable->width == 0 || drawable->height == 0 || drawable->depth == 0)
      {
         xcb_get_geometry_cookie_t geom_cookie;
         xcb_get_geometry_reply_t *geom_reply;
         xcb_void_cookie_t cookie;

         /* Try to select for input on the window.
          *
          * If the drawable is a window, this will get our events
          * delivered.
          *
          * Otherwise, we'll get a BadWindow error back from this request which
          * will let us know that the drawable is a pixmap instead.
          */
         cookie = sym_xcb_present_select_input_checked(info.conn,
                                                       (drawable->eid = xcb_generate_id(info.conn)),
                                                       drawable->window,
                                                       XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY|
                                                       XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY|
                                                       XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);

         /* Create an XCB event queue to hold present events outside of the usual
          * application event queue
          */
         drawable->special_event = xcb_register_for_special_xge(info.conn,
                                                                sym_xcb_present_id,
                                                                drawable->eid,
                                                                drawable->stamp);

         geom_cookie = xcb_get_geometry(info.conn, drawable->window);
         geom_reply = xcb_get_geometry_reply(info.conn, geom_cookie, NULL);
         if(!geom_reply) return 0;

         drawable->width = geom_reply->width;
         drawable->height = geom_reply->height;
         drawable->depth = geom_reply->depth;

         free(geom_reply);

         /* Check to see if our select input call failed. If it failed with a
          * BadWindow error, then assume the drawable is a pixmap. Destroy the
          * special event queue created above and mark the drawable as a pixmap
          */
         xcb_generic_error_t *error;
         error = xcb_request_check(info.conn, cookie);
         if (error)
            {
               ERR("xcb_get_geometry_reply() has failed. (error_code : %d)", error->error_code);
               xcb_unregister_for_special_event(info.conn, drawable->special_event);
               drawable->special_event = NULL;
               free(error);
               return 0;
            }
      }
   _dri3_flush_present_events(drawable);

   return 1;
}

/** dri3_get_pixmap_buffer
 *
 * Get the DRM object for a pixmap from the X server
 */
dri3_buffer *
dri3_get_pixmap_buffer(Pixmap pixmap)
{
   if (!info.conn)
      {
         ERR("xcb_connection is NULL. dri3_get_pixmap_buffer() has failed.");
         return NULL;
      }

   xcb_dri3_buffer_from_pixmap_cookie_t cookie;
   xcb_dri3_buffer_from_pixmap_reply_t *reply;

   cookie = sym_xcb_dri3_buffer_from_pixmap(info.conn,
                                            pixmap);
   reply = sym_xcb_dri3_buffer_from_pixmap_reply(info.conn,
                                                 cookie,
                                                 NULL);
   if (!reply)
      {
         ERR("xcb_dri3_buffer_from_pixmap_reply() has failed.");
         return NULL;
      }

   dri3_buffer *buffer;
   buffer = calloc(1, sizeof(dri3_buffer));
   if (!buffer) return NULL;

   buffer->size = reply->size;
   buffer->w = reply->width;
   buffer->h = reply->height;
   buffer->stride = reply->stride;
   buffer->depth = reply->depth;
   buffer->bpp = reply->bpp;
   buffer->buffer_fd = sym_xcb_dri3_buffer_from_pixmap_reply_fds(info.conn, reply)[0];
   buffer->bo = sym_tbm_bo_import_fd(info.bufmgr, buffer->buffer_fd);
   free(reply);
   if (!buffer->bo) return dri3_error_free(buffer);

   buffer->pixmap = pixmap;
   buffer->own_pixmap = EINA_FALSE;

   return buffer;
}


/** dri3_find_back
 *
 * Find an idle back buffer. If there isn't one, then
 * wait for a present idle notify event from the X server
 */
static int
_dri3_find_back(dri3_drawable *drawable)
{
   int  b;
   xcb_generic_event_t *ev;
   xcb_present_generic_event_t *ge;

   do {
        for (b = 0; b < drawable->num_back; b++)
          {
            int id = DRI3_BACK_ID((b + drawable->cur_back_id) % drawable->num_back);
            dri3_buffer *buffer = drawable->buffers[id];

            if (!buffer || !buffer->busy)
              {
                drawable->cur_back_id = id;
                return id;
              }
          }
        xcb_flush(info.conn);
        ev = xcb_wait_for_special_event(info.conn, drawable->special_event);
        if (ev)
          {
             ge = (void *) ev;
             _dri3_handle_present_event(drawable, ge);
          }
   } while (ev);

   return -1;
}


/** dri3_get_backbuffers
 *
 * Find a back buffer, allocating new ones as necessary
 */
dri3_buffer *
dri3_get_backbuffers(dri3_drawable *drawable, uint32_t *stamp)
{
   if (!_dri3_update_drawable(drawable))
      return 0;

   int buf_id = -1;
   buf_id = _dri3_find_back(drawable);
   if (buf_id < 0) return 0;

   dri3_buffer *back_buffer;
   back_buffer = drawable->buffers[buf_id];

   /* Allocate a new buffer if there isn't an old one, or if that
    * old one is the wrong size
    */
   if (!back_buffer || back_buffer->w != drawable->width || back_buffer->h != drawable->height)
      {
         dri3_buffer *new_buffer;

         /* Allocate the new buffers */
         new_buffer = dri3_alloc_render_buffer(drawable->window, drawable->width, drawable->height, drawable->depth, 32);
         if (!new_buffer)
            return 0;

         if (back_buffer) dri3_destroy_buffer(back_buffer);

         back_buffer = new_buffer;
         drawable->buffers[buf_id] = back_buffer;
      }

   drawable->stamp = stamp;

   dri3_fence_await(back_buffer);

   /* Return the requested buffer */
   return back_buffer;
}

/** dri3_swap_buffers
 *
 * Make the current back buffer visible using the present extension
 */
int64_t
dri3_swap_buffers(dri3_drawable *drawable, int64_t target_msc, int64_t divisor, int64_t remainder, XID region)
{
   if (!drawable) return 0;

   int64_t ret = 0;
   int buf_id = DRI3_BACK_ID(drawable->cur_back_id);
   dri3_buffer *back;
   back = drawable->buffers[buf_id];

   _dri3_flush_present_events(drawable);

   if (back)
      {

         if (back->w != drawable->width || back->h != drawable->height)
            {
               DBG("buffer and drawable size mismatch, frame skip");
               return ret;
            }

         dri3_fence_reset(back);

         /* Compute when we want the frame shown by taking the last known successful
          * MSC and adding in a swap interval for each outstanding swap request
          */
         ++drawable->send_sbc;
         if (target_msc == 0)
            target_msc = drawable->msc + drawable->swap_interval * (drawable->send_sbc - drawable->recv_sbc);

         uint32_t options = XCB_PRESENT_OPTION_NONE;
         if (drawable->swap_interval == 0)
            options |= XCB_PRESENT_OPTION_ASYNC;

         back->busy = 1;
         back->last_swap = drawable->send_sbc;

         xcb_void_cookie_t cookie;
         cookie = sym_xcb_present_pixmap_checked(info.conn,
                                                 drawable->window,
                                                 back->pixmap,
                                                 (uint32_t) drawable->send_sbc,
                                                 0,                                    /* valid */
                                                 region,                               /* update */
                                                 0,                                    /* x_off */
                                                 0,                                    /* y_off */
                                                 0,                                    /* target_crtc */
                                                 0,                                    /* wait fence */
                                                 back->sync_fence,                     /* idle fence */
                                                 options,                              /* XCB_PRESENT_OPTION_NONE,*/
                                                 target_msc,
                                                 divisor,
                                                 remainder, 0, NULL);

         xcb_generic_error_t *error;
         error = xcb_request_check(info.conn, cookie);
         if (error)
            {
               ERR("xcb_present_pixmap_checked() has failed. (error_code : %d)", error->error_code);
               free(error);
            }

         ret = (int64_t) drawable->send_sbc;

         xcb_flush(info.conn);

         if (drawable->stamp)
            ++(*drawable->stamp);

      }
   return ret;
}

void
dri3_set_buffer_age(dri3_drawable *drawable)
{
   if (!drawable) return;
   drawable->buffer_age = 0;

   int back_id = DRI3_BACK_ID(drawable->cur_back_id);
   if (back_id < 0 || !drawable->buffers[back_id]) return;

   if (drawable->buffers[back_id]->last_swap != 0)
      {
         int age = drawable->send_sbc - drawable->buffers[back_id]->last_swap + 1;
         if ( (age < drawable->num_back + 1) && drawable->flipping )
            drawable->buffer_age = age;
      }
}

int
dri3_get_buffer_age(dri3_drawable *drawable)
{
   if (!drawable) return 0;
   int back_id = DRI3_BACK_ID(_dri3_find_back(drawable));

   if (back_id < 0 || !drawable->buffers[back_id])
      return 0;

   if (drawable->buffers[back_id]->last_swap != 0)
      {
         int age = drawable->send_sbc - drawable->buffers[back_id]->last_swap + 1;
         if ( (age > drawable->num_back + 1) || !drawable->flipping ) return 0;
         else return age;
      }
   else
      return 0;
}

void
dri3_deinit_dri3()
{
   if (info.bufmgr)
     {
       sym_tbm_bufmgr_deinit(info.bufmgr);
       if (info.drm_fd > 0)
          {
             close(info.drm_fd);
             info.drm_fd = 0;
          }
       info.bufmgr = NULL;
     }
}

int
dri3_init_dri3(Display *dpy)
{
   if (!_lib_init())
      {
         INF("dri3 lib initialization failed. we are going to try dri2");
         return 0;
      }

   xcb_dri3_open_cookie_t cookie;
   xcb_dri3_open_reply_t* reply;

   memset (&info, 0x0, sizeof(dri3_info));
   /* Open the connection to the X server */
   info.disp = dpy;
   info.conn = sym_XGetXCBConnection(dpy);

   Window root = RootWindow(dpy, DefaultScreen(dpy));
   if (xcb_connection_has_error(info.conn))
      {
         INF("xcb Connection has failed. we are going to try dri2");
         return 0;
      }

   cookie = sym_xcb_dri3_open(info.conn, root, 0);
   reply = sym_xcb_dri3_open_reply(info.conn, cookie, NULL);

   if (reply && reply->nfd == 1)
      {
         info.drm_fd = sym_xcb_dri3_open_reply_fds(info.conn, reply)[0];
         info.bufmgr = sym_tbm_bufmgr_init(info.drm_fd);
      }
   if (reply) free(reply);
   if (!info.bufmgr)
      {
         INF("dri3 initialization failed. we are going to try dri2");
         return 0;
      }

   return 1;
}

int
dri3_query_dri3()
{
   xcb_dri3_query_version_cookie_t cookie;
   xcb_dri3_query_version_reply_t *reply;

   cookie = sym_xcb_dri3_query_version(info.conn,
                                       XCB_DRI3_MAJOR_VERSION,
                                       XCB_DRI3_MINOR_VERSION);
   reply = sym_xcb_dri3_query_version_reply(info.conn,
                                            cookie,
                                            NULL);
   if (reply)
      free(reply);
   else
      {
         INF("xcb_dri3_query_version_reply() has failed. we are going to try dri2");
         return 0;
      }

   return 1;
}


int
dri3_query_present()
{
   xcb_present_query_version_cookie_t cookie;
   xcb_present_query_version_reply_t *reply;

   cookie = sym_xcb_present_query_version(info.conn,
                                          XCB_PRESENT_MAJOR_VERSION,
                                          XCB_PRESENT_MINOR_VERSION);

   reply = sym_xcb_present_query_version_reply(info.conn,
                                               cookie,
                                               NULL);
   if (!reply)
      {
         INF("xcb_present_query_version_reply() has failed. we are going to try dri2");
         return 0;
      }

   free(reply);

   return 1;
}

int
dri3_XFixes_query(Display *disp)
{
   if (!sym_XFixesQueryExtension(disp, &xfixes_ev_base, &xfixes_err_base))
      {
         INF("XFixes extension not in xserver");
         return 0;
      }
   sym_XFixesQueryVersion(disp, &xfixes_major, &xfixes_minor);
   return 1;
}


void
dri3_get_data(dri3_buffer *buffer, RGBA_Image *im)
{
   if(!buffer || buffer->w < 0 || buffer->h <0 || !buffer->bo) return;

   tbm_bo_handle bo_handle;
   im->image.data = NULL;

   bo_handle = sym_tbm_bo_map (buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
   if (!bo_handle.ptr) return;

   im->image.data = bo_handle.ptr;
   im->cache_entry.w = buffer->stride/4;

   sym_tbm_bo_unmap (buffer->bo);
}

void *
dri3_tbm_bo_handle_map(dri3_buffer *buffer)
{
   if(!buffer || buffer->w < 0 || buffer->h <0) return NULL;

   tbm_bo_handle bo_handle;

   bo_handle = sym_tbm_bo_map (buffer->bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
   if(!bo_handle.ptr) return NULL;

   return bo_handle.ptr;
}

void
dri3_tbm_import(dri3_buffer *buffer)
{
   if(!buffer || !buffer->bo) return;

   buffer->bo = sym_tbm_bo_import_fd (info.bufmgr, buffer->buffer_fd);
}

void
dri3_tbm_bo_handle_unmap(dri3_buffer *buffer)
{
   if (!buffer || !buffer->bo) return;
   sym_tbm_bo_unmap(buffer->bo);
}

void
dri3_tbm_bo_handle_unref(dri3_buffer *buffer)
{
   if (!buffer || !buffer->bo) return;
   sym_tbm_bo_unref(buffer->bo);
}


void
dri3_set_swap_interval(dri3_drawable *drawable, int interval)
{
   if (!drawable) return;

   drawable->swap_interval = interval;
   _dri3_update_num_back(drawable);
}

int
dri3_get_swap_interval(dri3_drawable *drawable)
{
   if (!drawable) return 0;

   return drawable->swap_interval;
}

XID
dri3_XFixes_create_region(Display *disp, XRectangle *xrects, int nrects)
{
   return sym_XFixesCreateRegion(disp, xrects, nrects);;
}

void
dri3_XFixes_destory_region(Display *disp, XID region)
{
   sym_XFixesDestroyRegion(disp, region);
}


