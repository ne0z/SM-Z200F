/* GStreamer
 * Copyright (C) <2005> Julien Moutte <julien@moutte.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_XSHM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* HAVE_XSHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_XSHM
#include <X11/extensions/XShm.h>
#endif /* HAVE_XSHM */

#include <string.h>
#include <math.h>
#include <mm_types.h>

/* Object header */
#include "xvimageallocator.h"
#ifdef GST_EXT_XV_ENHANCEMENT
#include "xv_types.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>
#include <libdrm/drm.h>
#endif /* GST_EXT_XV_ENHANCEMENT */

/* Debugging category */
#include <gst/gstinfo.h>

/* Helper functions */
#include <gst/video/video.h>


GST_DEBUG_CATEGORY_STATIC (gst_debug_xvimageallocator);
#define GST_CAT_DEFAULT gst_debug_xvimageallocator
#ifdef GST_EXT_XV_ENHANCEMENT
#define MAX_DISPLAY_IN_XVIMAGESINK  20
extern Display *g_display_id[];
#endif
struct _GstXvImageMemory
{
  GstMemory parent;

  gint im_format;
  GstVideoRectangle crop;

#ifdef GST_EXT_XV_ENHANCEMENT
  GstBuffer *current_buffer;
#endif                          /* GST_EXT_XV_ENHANCEMENT */

  XvImage *xvimage;

#ifdef HAVE_XSHM
  XShmSegmentInfo SHMInfo;
#endif                          /* HAVE_XSHM */
};


struct _GstXvImageAllocator
{
  GstAllocator parent;

  GstXvContext *context;
};

struct _GstXvImageAllocatorClass
{
  GstAllocatorClass parent_class;
};

gboolean
gst_xvimage_memory_is_from_context (GstMemory * mem, GstXvContext * context)
{
  GstXvImageAllocator *alloc;

  if (!GST_IS_XVIMAGE_ALLOCATOR (mem->allocator))
    return FALSE;

  alloc = GST_XVIMAGE_ALLOCATOR_CAST (mem->allocator);

  if (alloc->context != context)
    return FALSE;

  return TRUE;
}

gint
gst_xvimage_memory_get_format (GstXvImageMemory * xvmem)
{
  g_return_val_if_fail (xvmem != NULL, FALSE);

  return xvmem->im_format;
}

XvImage *
gst_xvimage_memory_get_xvimage (GstXvImageMemory * xvmem)
{
  g_return_val_if_fail (xvmem != NULL, FALSE);

  return xvmem->xvimage;
}

gboolean
gst_xvimage_memory_get_crop (GstXvImageMemory * xvmem, GstVideoRectangle * crop)
{
  g_return_val_if_fail (xvmem != NULL, FALSE);

  if (crop)
    *crop = xvmem->crop;

  return TRUE;
}


/* X11 stuff */
static gboolean error_caught = FALSE;

static int
gst_xvimage_handle_xerror (Display * display, XErrorEvent * xevent)
{
  char error_msg[1024];

  XGetErrorText (display, xevent->error_code, error_msg, 1024);
#ifndef GST_EXT_XV_ENHANCEMENT
  GST_DEBUG ("xvimage triggered an XError. error: %s", error_msg);
  error_caught = TRUE;
#else
  int i = 0;
  for (i = 0; i < MAX_DISPLAY_IN_XVIMAGESINK; i++) {
    if (g_display_id[i] == display) {
      GST_ERROR ("xvimagesink triggered an XError. error: %s, display(%p), xevent->request_code: %d", error_msg, display, xevent->request_code);
      error_caught = TRUE;
      break;
    }
  }
  if (i == MAX_DISPLAY_IN_XVIMAGESINK) {
    GST_WARNING ("xvimagesink triggered an XError. error: %s, xevent->request_code: %d, but the display(%p) was not created in xvimagesink, skip it...", error_msg, xevent->request_code, display);
  }
#endif
  return 0;
}

static GstMemory *
gst_xvimage_allocator_dummy_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  return NULL;
}

static void
gst_xvimage_allocator_free (GstAllocator * allocator, GstMemory * gmem)
{
  GstXvImageMemory *mem = (GstXvImageMemory *) gmem;
  GstXvImageAllocator *alloc = (GstXvImageAllocator *) allocator;
  GstXvContext *context;

  if (gmem->parent)
    goto sub_mem;

  context = alloc->context;

  GST_DEBUG_OBJECT (allocator, "free memory %p", mem);

  g_mutex_lock (&context->lock);

#ifdef HAVE_XSHM
  if (context->use_xshm) {
    if (mem->SHMInfo.shmaddr != ((void *) -1)) {
      GST_DEBUG_OBJECT (allocator, "XServer ShmDetaching from 0x%x id 0x%lx",
          mem->SHMInfo.shmid, mem->SHMInfo.shmseg);
      XShmDetach (context->disp, &mem->SHMInfo);
      XSync (context->disp, FALSE);
      shmdt (mem->SHMInfo.shmaddr);
      mem->SHMInfo.shmaddr = (void *) -1;
    }
    if (mem->xvimage)
      XFree (mem->xvimage);
  } else
#endif /* HAVE_XSHM */
  {
    if (mem->xvimage) {
      g_free (mem->xvimage->data);
      XFree (mem->xvimage);
    }
  }

  XSync (context->disp, FALSE);

  g_mutex_unlock (&context->lock);

sub_mem:
  g_slice_free (GstXvImageMemory, mem);
}

static gpointer
gst_xvimage_memory_map (GstXvImageMemory * mem, gsize maxsize,
    GstMapFlags flags)
{
  return mem->xvimage->data + mem->parent.offset;
}

static gboolean
gst_xvimage_memory_unmap (GstXvImageMemory * mem)
{
  return TRUE;
}

static GstXvImageMemory *
gst_xvimage_memory_share (GstXvImageMemory * mem, gssize offset, gsize size)
{
  GstXvImageMemory *sub;
  GstMemory *parent;

  /* We can only share the complete memory */
  if (offset != 0)
    return NULL;
  if (size != -1 && size != mem->xvimage->data_size)
    return NULL;

  GST_DEBUG ("share memory %p", mem);

  /* find the real parent */
  if ((parent = mem->parent.parent) == NULL)
    parent = (GstMemory *) mem;

  if (size == -1)
    size = mem->parent.size - offset;

  /* the shared memory is always readonly */
  sub = g_slice_new (GstXvImageMemory);

  gst_memory_init (GST_MEMORY_CAST (sub), GST_MINI_OBJECT_FLAGS (parent) |
      GST_MINI_OBJECT_FLAG_LOCK_READONLY, mem->parent.allocator,
      &mem->parent, mem->parent.maxsize, mem->parent.align,
      mem->parent.offset + offset, size);

  sub->im_format = mem->im_format;
  sub->crop = mem->crop;
  sub->xvimage = mem->xvimage;
#ifdef HAVE_XSHM
  sub->SHMInfo = mem->SHMInfo;
#endif

  return sub;
}

static GstXvImageMemory *
gst_xvimage_memory_copy (GstMemory * gmem, gssize offset, gsize size)
{
  GstXvImageMemory *mem, *copy;

  mem = (GstXvImageMemory *) gmem;

  /* We can only copy the complete memory */
  if (offset != 0)
    return NULL;
  if (size != -1 && size != mem->xvimage->data_size)
    return NULL;

  GST_DEBUG ("copy memory %p", mem);

  copy = (GstXvImageMemory *)
      gst_xvimage_allocator_alloc (GST_XVIMAGE_ALLOCATOR_CAST (gmem->allocator),
      mem->im_format, mem->xvimage->width, mem->xvimage->height, &mem->crop,
      NULL);

  memcpy (copy->xvimage->data + copy->parent.offset,
      mem->xvimage->data + mem->parent.offset, mem->xvimage->data_size);

  return copy;
}

#define gst_xvimage_allocator_parent_class parent_class
G_DEFINE_TYPE (GstXvImageAllocator, gst_xvimage_allocator, GST_TYPE_ALLOCATOR);

static void gst_xvimage_allocator_finalize (GObject * object);

#define GST_XVIMAGE_ALLOCATOR_NAME "xvimage"

static void
gst_xvimage_allocator_class_init (GstXvImageAllocatorClass * klass)
{
  GObjectClass *gobject_class;
  GstAllocatorClass *allocator_class;

  gobject_class = (GObjectClass *) klass;
  allocator_class = (GstAllocatorClass *) klass;

  gobject_class->finalize = gst_xvimage_allocator_finalize;

  allocator_class->alloc = gst_xvimage_allocator_dummy_alloc;
  allocator_class->free = gst_xvimage_allocator_free;

  GST_DEBUG_CATEGORY_INIT (gst_debug_xvimageallocator, "xvimageallocator", 0,
      "xvimageallocator object");
}

static void
gst_xvimage_allocator_init (GstXvImageAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_XVIMAGE_ALLOCATOR_NAME;
  alloc->mem_map = (GstMemoryMapFunction) gst_xvimage_memory_map;
  alloc->mem_unmap = (GstMemoryUnmapFunction) gst_xvimage_memory_unmap;
  alloc->mem_share = (GstMemoryShareFunction) gst_xvimage_memory_share;
  alloc->mem_copy = (GstMemoryShareFunction) gst_xvimage_memory_copy;
  /* fallback is_span */

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static void
gst_xvimage_allocator_finalize (GObject * object)
{
  GstXvImageAllocator *alloc = GST_XVIMAGE_ALLOCATOR (object);

  GST_DEBUG_OBJECT (object, "finalize");

  gst_xvcontext_unref (alloc->context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GstXvImageAllocator *
gst_xvimage_allocator_new (GstXvContext * context)
{
  GstXvImageAllocator *alloc;

  g_return_val_if_fail (GST_IS_XVCONTEXT (context), NULL);

  alloc = g_object_new (GST_TYPE_XVIMAGE_ALLOCATOR, NULL);
  alloc->context = gst_xvcontext_ref (context);

  return alloc;
}

GstXvContext *
gst_xvimage_allocator_peek_context (GstXvImageAllocator * allocator)
{
  g_return_val_if_fail (GST_IS_XVIMAGE_ALLOCATOR (allocator), NULL);

  return allocator->context;
}

GstMemory *
gst_xvimage_allocator_alloc (GstXvImageAllocator * allocator, gint im_format,
    gint padded_width, gint padded_height, GstVideoRectangle * crop,
    GError ** error)
{
  int (*handler) (Display *, XErrorEvent *);
  gboolean success = FALSE;
  GstXvContext *context;
  gint align = 15, offset;
  GstXvImageMemory *mem;

  context = allocator->context;

  mem = g_slice_new (GstXvImageMemory);

  mem->im_format = im_format;
#ifdef HAVE_XSHM
  mem->SHMInfo.shmaddr = ((void *) -1);
  mem->SHMInfo.shmid = -1;
#endif
  mem->crop = *crop;

  GST_DEBUG_OBJECT (allocator, "creating image %p (%dx%d) cropped %dx%d-%dx%d",
      mem, padded_width, padded_height, crop->x, crop->y, crop->w, crop->h);

  g_mutex_lock (&context->lock);

  /* Setting an error handler to catch failure */
  error_caught = FALSE;
  handler = XSetErrorHandler (gst_xvimage_handle_xerror);

#ifdef HAVE_XSHM
  if (context->use_xshm) {
    int expected_size;

    mem->xvimage = XvShmCreateImage (context->disp,
        context->xv_port_id, im_format, NULL, padded_width, padded_height,
        &mem->SHMInfo);
    if (!mem->xvimage || error_caught) {
      g_mutex_unlock (&context->lock);

      /* Reset error flag */
      error_caught = FALSE;

      /* Push a warning */
      GST_WARNING_OBJECT (allocator,
          "could not XvShmCreateImage a %dx%d image", padded_width,
          padded_height);

#ifdef GST_EXT_XV_ENHANCEMENT
      /* must not change "use_xshm",
         because it causes memory curruption when buffer created by XvShmCreateImage */
      g_mutex_lock (&context->lock);
      goto create_failed;
#else
      /* Retry without XShm */
      context->use_xshm = FALSE;

      /* Hold X mutex again to try without XShm */
      g_mutex_lock (&context->lock);
      goto no_xshm;
#endif
    }

    /* we have to use the returned data_size for our shm size */
    GST_LOG_OBJECT (allocator, "XShm image size is %d",
        mem->xvimage->data_size);

    /* calculate the expected size.  This is only for sanity checking the
     * number we get from X. */
    switch (im_format) {
      case GST_MAKE_FOURCC ('I', '4', '2', '0'):
      case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
      {
        gint pitches[3];
        gint offsets[3];
        guint plane;

        offsets[0] = 0;
        pitches[0] = GST_ROUND_UP_4 (padded_width);
        offsets[1] = offsets[0] + pitches[0] * GST_ROUND_UP_2 (padded_height);
        pitches[1] = GST_ROUND_UP_8 (padded_width) / 2;
        offsets[2] =
            offsets[1] + pitches[1] * GST_ROUND_UP_2 (padded_height) / 2;
        pitches[2] = GST_ROUND_UP_8 (pitches[0]) / 2;

        expected_size =
            offsets[2] + pitches[2] * GST_ROUND_UP_2 (padded_height) / 2;

        for (plane = 0; plane < mem->xvimage->num_planes; plane++) {
          GST_DEBUG_OBJECT (allocator,
              "Plane %u has a expected pitch of %d bytes, " "offset of %d",
              plane, pitches[plane], offsets[plane]);
        }
        break;
      }
      case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
      case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
        expected_size = padded_height * GST_ROUND_UP_4 (padded_width * 2);
        break;
#ifdef GST_EXT_XV_ENHANCEMENT
      case GST_MAKE_FOURCC ('S', 'T', '1', '2'):
      case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
        expected_size = sizeof (MMVideoBuffer);
        break;
#endif /* GST_EXT_XV_ENHANCEMENT */
      default:
        expected_size = 0;
        break;
    }
    if (expected_size != 0 && mem->xvimage->data_size != expected_size) {
#ifndef GST_EXT_XV_ENHANCEMENT
      GST_WARNING_OBJECT (allocator,
          "unexpected XShm image size (got %d, expected %d)",
          mem->xvimage->data_size, expected_size);
#endif
    }
    GST_INFO ("expected XShm image size (got %d, expected %d)",
        mem->xvimage->data_size, expected_size);
    /* Be verbose about our XvImage stride */
    {
      guint plane;

      for (plane = 0; plane < mem->xvimage->num_planes; plane++) {
        GST_DEBUG_OBJECT (allocator, "Plane %u has a pitch of %d bytes, "
            "offset of %d", plane, mem->xvimage->pitches[plane],
            mem->xvimage->offsets[plane]);
      }
    }

    /* get shared memory */
    mem->SHMInfo.shmid =
        shmget (IPC_PRIVATE, mem->xvimage->data_size + align, IPC_CREAT | 0777);
    if (mem->SHMInfo.shmid == -1)
      goto shmget_failed;

    /* attach */
    mem->SHMInfo.shmaddr = shmat (mem->SHMInfo.shmid, NULL, 0);
    if (mem->SHMInfo.shmaddr == ((void *) -1))
      goto shmat_failed;

    /* now we can set up the image data */
    mem->xvimage->data = mem->SHMInfo.shmaddr;
    mem->SHMInfo.readOnly = FALSE;

    if (XShmAttach (context->disp, &mem->SHMInfo) == 0)
      goto xattach_failed;

    XSync (context->disp, FALSE);

    /* Delete the shared memory segment as soon as we everyone is attached.
     * This way, it will be deleted as soon as we detach later, and not
     * leaked if we crash. */
    shmctl (mem->SHMInfo.shmid, IPC_RMID, NULL);

    GST_DEBUG_OBJECT (allocator, "XServer ShmAttached to 0x%x, id 0x%lx",
        mem->SHMInfo.shmid, mem->SHMInfo.shmseg);
  } else
  no_xshm:
#endif /* HAVE_XSHM */
  {
    mem->xvimage = XvCreateImage (context->disp,
        context->xv_port_id, im_format, NULL, padded_width, padded_height);
    if (!mem->xvimage || error_caught)
      goto create_failed;

    /* we have to use the returned data_size for our image size */
    mem->xvimage->data = g_malloc (mem->xvimage->data_size + align);

    XSync (context->disp, FALSE);
  }

  if ((offset = ((guintptr) mem->xvimage->data & align)))
    offset = (align + 1) - offset;

  GST_DEBUG_OBJECT (allocator, "memory %p, align %d, offset %d",
      mem->xvimage->data, align, offset);

  /* Reset error handler */
  error_caught = FALSE;
  XSetErrorHandler (handler);

  gst_memory_init (GST_MEMORY_CAST (mem), 0,
      GST_ALLOCATOR_CAST (allocator), NULL, mem->xvimage->data_size + align,
      align, offset, mem->xvimage->data_size);

  g_mutex_unlock (&context->lock);

  success = TRUE;

beach:
  if (!success) {
    g_slice_free (GstXvImageMemory, mem);
    mem = NULL;
  }

  return GST_MEMORY_CAST (mem);

  /* ERRORS */
create_failed:
  {
    g_mutex_unlock (&context->lock);
    /* Reset error handler */
    error_caught = FALSE;
    XSetErrorHandler (handler);
    /* Push an error */
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
        "could not XvShmCreateImage a %dx%d image", padded_width,
        padded_height);
    goto beach;
  }
#ifdef HAVE_XSHM
shmget_failed:
  {
    g_mutex_unlock (&context->lock);
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
        "could not get shared memory of %d bytes", mem->xvimage->data_size);
    goto beach;
  }
shmat_failed:
  {
    g_mutex_unlock (&context->lock);
    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
        "Failed to shmat: %s", g_strerror (errno));
    /* Clean up the shared memory segment */
    shmctl (mem->SHMInfo.shmid, IPC_RMID, NULL);
    goto beach;
  }
xattach_failed:
  {
    /* Clean up the shared memory segment */
    shmctl (mem->SHMInfo.shmid, IPC_RMID, NULL);
    g_mutex_unlock (&context->lock);

    g_set_error (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_WRITE,
        "Failed to XShmAttach");
    goto beach;
  }
#endif
}

/* We are called with the x_lock taken */
static void
gst_xwindow_draw_borders (GstXWindow * window, GstVideoRectangle * rect)
{
  gint t1, t2;
  GstXvContext *context;

  g_return_if_fail (window != NULL);
  g_return_if_fail (rect != NULL);

  context = window->context;

  XSetForeground (context->disp, window->gc, context->black);

  /* Left border */
  if (rect->x > window->render_rect.x) {
    XFillRectangle (context->disp, window->win, window->gc,
        window->render_rect.x, window->render_rect.y,
        rect->x - window->render_rect.x, window->render_rect.h);
  }

  /* Right border */
  t1 = rect->x + rect->w;
  t2 = window->render_rect.x + window->render_rect.w;
  if (t1 < t2) {
    XFillRectangle (context->disp, window->win, window->gc,
        t1, window->render_rect.y, t2 - t1, window->render_rect.h);
  }

  /* Top border */
  if (rect->y > window->render_rect.y) {
    XFillRectangle (context->disp, window->win, window->gc,
        window->render_rect.x, window->render_rect.y,
        window->render_rect.w, rect->y - window->render_rect.y);
  }

  /* Bottom border */
  t1 = rect->y + rect->h;
  t2 = window->render_rect.y + window->render_rect.h;
  if (t1 < t2) {
    XFillRectangle (context->disp, window->win, window->gc,
        window->render_rect.x, t1, window->render_rect.w, t2 - t1);
  }
}

void
gst_xvimage_memory_render (GstXvImageMemory * mem, GstVideoRectangle * src_crop,
    GstXWindow * window, GstVideoRectangle * dst_crop, gboolean draw_border)
{
  GstXvContext *context;
  XvImage *xvimage;
#ifdef GST_EXT_XV_ENHANCEMENT
  int ret = 0;
  int (*handler) (Display *, XErrorEvent *) = NULL;
  XV_DATA_PTR img_data = NULL;

  static gboolean is_fb = FALSE;
  static gboolean init_swap_done = FALSE;
#endif /* GST_EXT_XV_ENHANCEMENT */

  context = window->context;

  g_mutex_lock (&context->lock);
  xvimage = gst_xvimage_memory_get_xvimage (mem);

  if (draw_border) {
    gst_xwindow_draw_borders (window, dst_crop);
  }
#ifdef HAVE_XSHM
  if (context->use_xshm) {
    GST_LOG ("XvShmPutImage with image %dx%d and window %dx%d, from xvimage %"
        GST_PTR_FORMAT, src_crop->w, src_crop->h,
        window->render_rect.w, window->render_rect.h, mem);

#ifdef GST_EXT_XV_ENHANCEMENT
    /* set error handler */
    error_caught = FALSE;
    handler = XSetErrorHandler (gst_xvimage_handle_xerror);

    /* src input indicates the status when degree is 0 */
    /* dst input indicates the area that src will be shown regardless of rotate */
    if (context->xim_transparenter && context->dsp_test) {
        GST_LOG_OBJECT (mem, "Transparent related issue.");

        /* We directly use DRI2 buffers instead of XImage to set the transparenter. */
        /* And, in order to do this properly, DRI2 swap for visible buffer is needed whenever FB flag is changed. */
        unsigned int attachments[1] = {DRI2BufferBackLeft};
        int dri2_out_temp,dri2_out_width,dri2_out_height;
        DRI2Buffer *pDRI2Buffer = DRI2GetBuffers(context->disp,window->win, &dri2_out_width, &dri2_out_height, attachments, 1, &dri2_out_temp);
        if(!pDRI2Buffer)
            GST_ERROR("failed to get DRI2 buffers for transparenter." );
        else
        {
            if (!init_swap_done)
            {
                DRI2SwapBuffers(context->disp, window->win, 0, 0, 0, &dri2_out_temp);
                init_swap_done = TRUE;
            }
            if (((pDRI2Buffer[0].flags & 0x2) && !is_fb) || (!(pDRI2Buffer[0].flags & 0x2) && is_fb))
            {
                GST_DEBUG("[helius] width=%d height=%d name=%d flags=0x%x", dri2_out_width, dri2_out_height, pDRI2Buffer[0].name, pDRI2Buffer[0].flags );
                tbm_bo bo = tbm_bo_import(context->tbm, pDRI2Buffer[0].name);
                memset(tbm_bo_map(bo,TBM_DEVICE_CPU,TBM_OPTION_WRITE).ptr,0,dri2_out_width * dri2_out_height * 4);
                tbm_bo_unmap(bo);
                tbm_bo_unref(bo);

                DRI2SwapBuffers(context->disp, window->win, 0, 0, 0, &dri2_out_temp);
                is_fb = !is_fb;
            }
            XFree(pDRI2Buffer);
        }
    }

    g_mutex_lock (context->display_buffer_lock);
#ifndef _TIZEN_PUBLIC_
    if (context->displaying_buffer_count > 4) {
      GST_WARNING
          ("[Info] displaying_buffer_count which was put to X server is [%d].",
          context->displaying_buffer_count);
    }
#else
    if (context->displaying_buffer_count > 3) {
      GST_WARNING
          ("too many buffers are pushed. skip this... [displaying_buffer_count %d]",
          context->displaying_buffer_count);
    }
#endif
    g_mutex_unlock (context->display_buffer_lock);
    ret = XvShmPutImage (context->disp,
        context->xv_port_id,
        window->win,
        window->gc, xvimage,
        src_crop->x, src_crop->y, src_crop->w, src_crop->h,
        dst_crop->x, dst_crop->y, dst_crop->w, dst_crop->h, FALSE);
    GST_LOG_OBJECT (mem, "XvShmPutImage return value [%d]", ret);

#else /* GST_EXT_XV_ENHANCEMENT */
    XvShmPutImage (context->disp,
        context->xv_port_id,
        window->win,
        window->gc, xvimage,
        src_crop->x, src_crop->y, src_crop->w, src_crop->h,
        dst_crop->x, dst_crop->y, dst_crop->w, dst_crop->h, FALSE);
#endif /* GST_EXT_XV_ENHANCEMENT */
  } else
#endif /* HAVE_XSHM */
  {
    XvPutImage (context->disp,
        context->xv_port_id,
        window->win,
        window->gc, xvimage,
        src_crop->x, src_crop->y, src_crop->w, src_crop->h,
        dst_crop->x, dst_crop->y, dst_crop->w, dst_crop->h);
  }
  XSync (context->disp, FALSE);

#ifdef HAVE_XSHM
#ifdef GST_EXT_XV_ENHANCEMENT
  if (ret || error_caught) {
    GST_WARNING
        ("putimage error : ret %d, error_caught %d, displaying buffer count %d",
        ret, error_caught, context->displaying_buffer_count);

#ifndef _TIZEN_PUBLIC_
    if(context->recording_hint) {
      g_mutex_lock (context->vblank_lock);
      if(context->vblank_recieved ) {
        GST_WARNING("because of error, we will not set vblank_recieved as FALSE");
      }
      g_mutex_unlock (context->vblank_lock);
    }
#endif
    /* release gem handle */
    img_data = (XV_DATA_PTR) gst_xvimage_memory_get_xvimage (mem)->data;
    if (img_data) {
      unsigned int gem_name[XV_BUF_PLANE_NUM] = { 0, };
      context->last_added_buffer_index = context->temp_added_buffer_index ;
      gem_name[0] = img_data->YBuf;
      gem_name[1] = img_data->CbBuf;
      gem_name[2] = img_data->CrBuf;
      gst_xvcontext_remove_displaying_buffer (context, gem_name);
    }
  }
#ifndef _TIZEN_PUBLIC_
  else {
    if(context->recording_hint) {
      g_mutex_lock (context->vblank_lock);
      context->vblank_recieved = FALSE;
      g_mutex_unlock (context->vblank_lock);
    }
  }
#endif
  /* Reset error handler */
  error_caught = FALSE;
  XSetErrorHandler (handler);
#endif /* GST_EXT_XV_ENHANCEMENT */
#endif /* HAVE_XSHM */

  g_mutex_unlock (&context->lock);
}

#ifdef GST_EXT_XV_ENHANCEMENT
GstBuffer *
gst_xvimage_memory_get_buffer (GstXvImageMemory * mem)
{
  g_return_val_if_fail (mem, NULL);
  return mem->current_buffer;
}

void
gst_xvimage_memory_set_buffer (GstXvImageMemory * mem, GstBuffer * new_buf)
{
  g_return_if_fail (mem);
  mem->current_buffer = new_buf;
}
#endif /* GST_EXT_XV_ENHANCEMENT */
