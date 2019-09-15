#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/xshmfence.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/sync.h>

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <Ecore_Buffer.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

#include "ecore_buffer_private.h"

typedef struct _Ecore_Buffer_Module_X11_Dri3_Data Ecore_Buffer_Module_X11_Dri3_Data;
typedef struct _Ecore_Buffer_X11_Dri3_Data Ecore_Buffer_X11_Dri3_Data;

struct _Ecore_Buffer_Module_X11_Dri3_Data {
     tbm_bufmgr tbm_mgr;
};

struct _Ecore_Buffer_X11_Dri3_Data {
     Ecore_X_Pixmap pixmap;
     void *tbm_surface;
     int w;
     int h;
     int stride;
     unsigned int flags;
     Ecore_Buffer_Format format;
     Eina_Bool is_imported;
};

static Ecore_Pixmap  _ecore_buffer_x11_dri3_pixmap_get(Ecore_Buffer_Module_Data bmdata, Ecore_Buffer_Data bdata);

static int
_buf_get_num_planes(Ecore_Buffer_Format format)
{
   int num_planes = 0;

   switch (format)
     {
      case ECORE_BUFFER_FORMAT_C8:
      case ECORE_BUFFER_FORMAT_RGB332:
      case ECORE_BUFFER_FORMAT_BGR233:
      case ECORE_BUFFER_FORMAT_XRGB4444:
      case ECORE_BUFFER_FORMAT_XBGR4444:
      case ECORE_BUFFER_FORMAT_RGBX4444:
      case ECORE_BUFFER_FORMAT_BGRX4444:
      case ECORE_BUFFER_FORMAT_ARGB4444:
      case ECORE_BUFFER_FORMAT_ABGR4444:
      case ECORE_BUFFER_FORMAT_RGBA4444:
      case ECORE_BUFFER_FORMAT_BGRA4444:
      case ECORE_BUFFER_FORMAT_XRGB1555:
      case ECORE_BUFFER_FORMAT_XBGR1555:
      case ECORE_BUFFER_FORMAT_RGBX5551:
      case ECORE_BUFFER_FORMAT_BGRX5551:
      case ECORE_BUFFER_FORMAT_ARGB1555:
      case ECORE_BUFFER_FORMAT_ABGR1555:
      case ECORE_BUFFER_FORMAT_RGBA5551:
      case ECORE_BUFFER_FORMAT_BGRA5551:
      case ECORE_BUFFER_FORMAT_RGB565:
      case ECORE_BUFFER_FORMAT_BGR565:
      case ECORE_BUFFER_FORMAT_RGB888:
      case ECORE_BUFFER_FORMAT_BGR888:
      case ECORE_BUFFER_FORMAT_XRGB8888:
      case ECORE_BUFFER_FORMAT_XBGR8888:
      case ECORE_BUFFER_FORMAT_RGBX8888:
      case ECORE_BUFFER_FORMAT_BGRX8888:
      case ECORE_BUFFER_FORMAT_ARGB8888:
      case ECORE_BUFFER_FORMAT_ABGR8888:
      case ECORE_BUFFER_FORMAT_RGBA8888:
      case ECORE_BUFFER_FORMAT_BGRA8888:
      case ECORE_BUFFER_FORMAT_XRGB2101010:
      case ECORE_BUFFER_FORMAT_XBGR2101010:
      case ECORE_BUFFER_FORMAT_RGBX1010102:
      case ECORE_BUFFER_FORMAT_BGRX1010102:
      case ECORE_BUFFER_FORMAT_ARGB2101010:
      case ECORE_BUFFER_FORMAT_ABGR2101010:
      case ECORE_BUFFER_FORMAT_RGBA1010102:
      case ECORE_BUFFER_FORMAT_BGRA1010102:
      case ECORE_BUFFER_FORMAT_YUYV:
      case ECORE_BUFFER_FORMAT_YVYU:
      case ECORE_BUFFER_FORMAT_UYVY:
      case ECORE_BUFFER_FORMAT_VYUY:
      case ECORE_BUFFER_FORMAT_AYUV:
         num_planes = 1;
         break;
      case ECORE_BUFFER_FORMAT_NV12:
      case ECORE_BUFFER_FORMAT_NV21:
      case ECORE_BUFFER_FORMAT_NV16:
      case ECORE_BUFFER_FORMAT_NV61:
         num_planes = 2;
         break;
      case ECORE_BUFFER_FORMAT_YUV410:
      case ECORE_BUFFER_FORMAT_YVU410:
      case ECORE_BUFFER_FORMAT_YUV411:
      case ECORE_BUFFER_FORMAT_YVU411:
      case ECORE_BUFFER_FORMAT_YUV420:
      case ECORE_BUFFER_FORMAT_YVU420:
      case ECORE_BUFFER_FORMAT_YUV422:
      case ECORE_BUFFER_FORMAT_YVU422:
      case ECORE_BUFFER_FORMAT_YUV444:
      case ECORE_BUFFER_FORMAT_YVU444:
         num_planes = 3;
         break;

      default :
         break;
     }

   return num_planes;
}

static int
_buf_get_bpp(Ecore_Buffer_Format format)
{
   int bpp = 0;

   switch (format)
     {
      case ECORE_BUFFER_FORMAT_C8:
      case ECORE_BUFFER_FORMAT_RGB332:
      case ECORE_BUFFER_FORMAT_BGR233:
         bpp = 8;
         break;
      case ECORE_BUFFER_FORMAT_XRGB4444:
      case ECORE_BUFFER_FORMAT_XBGR4444:
      case ECORE_BUFFER_FORMAT_RGBX4444:
      case ECORE_BUFFER_FORMAT_BGRX4444:
      case ECORE_BUFFER_FORMAT_ARGB4444:
      case ECORE_BUFFER_FORMAT_ABGR4444:
      case ECORE_BUFFER_FORMAT_RGBA4444:
      case ECORE_BUFFER_FORMAT_BGRA4444:
      case ECORE_BUFFER_FORMAT_XRGB1555:
      case ECORE_BUFFER_FORMAT_XBGR1555:
      case ECORE_BUFFER_FORMAT_RGBX5551:
      case ECORE_BUFFER_FORMAT_BGRX5551:
      case ECORE_BUFFER_FORMAT_ARGB1555:
      case ECORE_BUFFER_FORMAT_ABGR1555:
      case ECORE_BUFFER_FORMAT_RGBA5551:
      case ECORE_BUFFER_FORMAT_BGRA5551:
      case ECORE_BUFFER_FORMAT_RGB565:
      case ECORE_BUFFER_FORMAT_BGR565:
         bpp = 16;
         break;
      case ECORE_BUFFER_FORMAT_RGB888:
      case ECORE_BUFFER_FORMAT_BGR888:
         bpp = 24;
         break;
      case ECORE_BUFFER_FORMAT_XRGB8888:
      case ECORE_BUFFER_FORMAT_XBGR8888:
      case ECORE_BUFFER_FORMAT_RGBX8888:
      case ECORE_BUFFER_FORMAT_BGRX8888:
      case ECORE_BUFFER_FORMAT_ARGB8888:
      case ECORE_BUFFER_FORMAT_ABGR8888:
      case ECORE_BUFFER_FORMAT_RGBA8888:
      case ECORE_BUFFER_FORMAT_BGRA8888:
      case ECORE_BUFFER_FORMAT_XRGB2101010:
      case ECORE_BUFFER_FORMAT_XBGR2101010:
      case ECORE_BUFFER_FORMAT_RGBX1010102:
      case ECORE_BUFFER_FORMAT_BGRX1010102:
      case ECORE_BUFFER_FORMAT_ARGB2101010:
      case ECORE_BUFFER_FORMAT_ABGR2101010:
      case ECORE_BUFFER_FORMAT_RGBA1010102:
      case ECORE_BUFFER_FORMAT_BGRA1010102:
      case ECORE_BUFFER_FORMAT_YUYV:
      case ECORE_BUFFER_FORMAT_YVYU:
      case ECORE_BUFFER_FORMAT_UYVY:
      case ECORE_BUFFER_FORMAT_VYUY:
      case ECORE_BUFFER_FORMAT_AYUV:
         bpp = 32;
         break;
      case ECORE_BUFFER_FORMAT_NV12:
      case ECORE_BUFFER_FORMAT_NV21:
         bpp = 12;
         break;
      case ECORE_BUFFER_FORMAT_NV16:
      case ECORE_BUFFER_FORMAT_NV61:
         bpp = 16;
         break;
      case ECORE_BUFFER_FORMAT_YUV410:
      case ECORE_BUFFER_FORMAT_YVU410:
         bpp = 9;
         break;
      case ECORE_BUFFER_FORMAT_YUV411:
      case ECORE_BUFFER_FORMAT_YVU411:
      case ECORE_BUFFER_FORMAT_YUV420:
      case ECORE_BUFFER_FORMAT_YVU420:
         bpp = 12;
         break;
      case ECORE_BUFFER_FORMAT_YUV422:
      case ECORE_BUFFER_FORMAT_YVU422:
         bpp = 16;
         break;
      case ECORE_BUFFER_FORMAT_YUV444:
      case ECORE_BUFFER_FORMAT_YVU444:
         bpp = 24;
         break;
      default :
         break;
     }

   return bpp;
}

static int
_buf_get_depth(Ecore_Buffer_Format format)
{
   int depth = 0;

   switch (format)
     {
      case ECORE_BUFFER_FORMAT_C8:
      case ECORE_BUFFER_FORMAT_RGB332:
      case ECORE_BUFFER_FORMAT_BGR233:
         depth = 8;
         break;
      case ECORE_BUFFER_FORMAT_XRGB4444:
      case ECORE_BUFFER_FORMAT_XBGR4444:
      case ECORE_BUFFER_FORMAT_RGBX4444:
      case ECORE_BUFFER_FORMAT_BGRX4444:
         depth = 12;
         break;
      case ECORE_BUFFER_FORMAT_ARGB4444:
      case ECORE_BUFFER_FORMAT_ABGR4444:
      case ECORE_BUFFER_FORMAT_RGBA4444:
      case ECORE_BUFFER_FORMAT_BGRA4444:
         depth = 16;
         break;
      case ECORE_BUFFER_FORMAT_XRGB1555:
      case ECORE_BUFFER_FORMAT_XBGR1555:
      case ECORE_BUFFER_FORMAT_RGBX5551:
      case ECORE_BUFFER_FORMAT_BGRX5551:
         depth = 15;
         break;
      case ECORE_BUFFER_FORMAT_ARGB1555:
      case ECORE_BUFFER_FORMAT_ABGR1555:
      case ECORE_BUFFER_FORMAT_RGBA5551:
      case ECORE_BUFFER_FORMAT_BGRA5551:
      case ECORE_BUFFER_FORMAT_RGB565:
      case ECORE_BUFFER_FORMAT_BGR565:
         depth = 16;
         break;
      case ECORE_BUFFER_FORMAT_RGB888:
      case ECORE_BUFFER_FORMAT_BGR888:
         depth = 24;
         break;
      case ECORE_BUFFER_FORMAT_XRGB8888:
      case ECORE_BUFFER_FORMAT_XBGR8888:
      case ECORE_BUFFER_FORMAT_RGBX8888:
      case ECORE_BUFFER_FORMAT_BGRX8888:
         depth = 24;
         break;
      case ECORE_BUFFER_FORMAT_ARGB8888:
      case ECORE_BUFFER_FORMAT_ABGR8888:
      case ECORE_BUFFER_FORMAT_RGBA8888:
      case ECORE_BUFFER_FORMAT_BGRA8888:
         depth = 32;
         break;
      case ECORE_BUFFER_FORMAT_XRGB2101010:
      case ECORE_BUFFER_FORMAT_XBGR2101010:
      case ECORE_BUFFER_FORMAT_RGBX1010102:
      case ECORE_BUFFER_FORMAT_BGRX1010102:
         depth = 30;
         break;
      case ECORE_BUFFER_FORMAT_ARGB2101010:
      case ECORE_BUFFER_FORMAT_ABGR2101010:
      case ECORE_BUFFER_FORMAT_RGBA1010102:
      case ECORE_BUFFER_FORMAT_BGRA1010102:
         depth = 32;
         break;
      case ECORE_BUFFER_FORMAT_YUYV:
      case ECORE_BUFFER_FORMAT_YVYU:
      case ECORE_BUFFER_FORMAT_UYVY:
      case ECORE_BUFFER_FORMAT_VYUY:
      case ECORE_BUFFER_FORMAT_AYUV:
      case ECORE_BUFFER_FORMAT_NV12:
      case ECORE_BUFFER_FORMAT_NV21:
      case ECORE_BUFFER_FORMAT_NV16:
      case ECORE_BUFFER_FORMAT_NV61:
      case ECORE_BUFFER_FORMAT_YUV410:
      case ECORE_BUFFER_FORMAT_YVU410:
      case ECORE_BUFFER_FORMAT_YUV411:
      case ECORE_BUFFER_FORMAT_YVU411:
      case ECORE_BUFFER_FORMAT_YUV420:
      case ECORE_BUFFER_FORMAT_YVU420:
      case ECORE_BUFFER_FORMAT_YUV422:
      case ECORE_BUFFER_FORMAT_YVU422:
      case ECORE_BUFFER_FORMAT_YUV444:
      case ECORE_BUFFER_FORMAT_YVU444:
      default :
         depth = 0; //unknown in X
         break;
     }

   return depth;
}

static int
_dri3_open(Ecore_X_Display *dpy, Ecore_X_Window root, unsigned provider)
{
   xcb_connection_t *c = XGetXCBConnection(dpy);
   xcb_dri3_open_cookie_t cookie;
   xcb_dri3_open_reply_t *reply;

   cookie = xcb_dri3_open(c, root, provider);
   reply = xcb_dri3_open_reply(c, cookie, NULL);
   if ((!reply) || (reply->nfd != 1))
     return -1;

   return xcb_dri3_open_reply_fds(c, reply)[0];
}

static Ecore_X_Pixmap
_dri3_pixmap_from_fd(Ecore_X_Display *dpy, Ecore_X_Drawable draw, int width, int height, int depth, int fd, int bpp, int stride, int size)
{
   xcb_connection_t *c = XGetXCBConnection(dpy);
   Ecore_X_Pixmap pixmap = xcb_generate_id(c);

   if (!dpy)
     return 0;

   c = XGetXCBConnection(dpy);
   if (!c)
     return 0;

   pixmap = xcb_generate_id(c);
   if (!pixmap)
     return 0;

   xcb_dri3_pixmap_from_buffer(c, pixmap, draw, size, width, height, stride, depth, bpp, fd);

   return pixmap;
}

static Ecore_Buffer_Module_Data
_ecore_buffer_x11_dri3_init(const char *context EINA_UNUSED, const char *options EINA_UNUSED)
{
   Ecore_X_Display *xdpy;
   Ecore_X_Window root;
   Ecore_Buffer_Module_X11_Dri3_Data *mdata = NULL;
   int fd = 0;

   if (!ecore_x_init(NULL))
     return NULL;

   xdpy = ecore_x_display_get();
   if (!xdpy)
     goto on_error;

   root = ecore_x_window_root_first_get();
   if (!root)
     goto on_error;

   mdata = calloc(1, sizeof(Ecore_Buffer_Module_X11_Dri3_Data));
   if (!mdata)
     goto on_error;

   //Init DRI3 and TBM
   fd = _dri3_open(xdpy, root, 0);
   if (fd < 0)
     goto on_error;

   mdata->tbm_mgr = tbm_bufmgr_init(fd);
   if (!mdata->tbm_mgr)
     goto on_error;

   close(fd);

   return mdata;

on_error:
   if (fd > 0) close(fd);
   if (mdata) free(mdata);
   ecore_x_shutdown();

   return NULL;
}

static void
_ecore_buffer_x11_dri3_shutdown(Ecore_Buffer_Module_Data bmdata)
{
   Ecore_Buffer_Module_X11_Dri3_Data *bm = bmdata;

   if (!bm)
     return;

   if (bm->tbm_mgr)
     tbm_bufmgr_deinit(bm->tbm_mgr);

   ecore_x_shutdown();
}

static Ecore_Buffer_Data
_ecore_buffer_x11_dri3_buffer_alloc(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, int width, int height, Ecore_Buffer_Format format, unsigned int flags)
{
   Ecore_Buffer_X11_Dri3_Data *buf;

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri3_Data));
   if (!buf)
     return NULL;

   buf->w = width;
   buf->h = height;
   buf->format = format;
   buf->flags = flags;
   buf->is_imported = EINA_FALSE;
   buf->tbm_surface = tbm_surface_create(width,height,(tbm_format)format);
   if (!buf->tbm_surface)
     {
        free(buf);
        return NULL;
     }

   return buf;
}

static Ecore_Buffer_Data
_ecore_buffer_x11_dri3_buffer_alloc_with_tbm_surface(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, void *tbm_surface, int *ret_w, int *ret_h, Ecore_Buffer_Format *ret_format, unsigned int flags)
{
   Ecore_Buffer_X11_Dri3_Data *buf;

   EINA_SAFETY_ON_NULL_RETURN_VAL(tbm_surface, NULL);

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri3_Data));
   if (!buf)
     return NULL;

   buf->w = tbm_surface_get_width(tbm_surface);
   buf->h = tbm_surface_get_height(tbm_surface);
   buf->format = tbm_surface_get_format(tbm_surface);
   buf->flags = flags;
   buf->is_imported = EINA_FALSE;

   tbm_surface_internal_ref(tbm_surface);
   buf->tbm_surface = tbm_surface;

   if (ret_w) *ret_w = buf->w;
   if (ret_h) *ret_h = buf->h;
   if (ret_format) *ret_format = buf->format;

   return buf;
}

static Eina_Bool
_ecore_buffer_x11_dri3_buffer_info_get(Ecore_Buffer_Module_Data bmdata, Ecore_Buffer_Data bdata, Ecore_Buffer_Info *info)
{
   Ecore_Buffer_X11_Dri3_Data *buf = bdata;
   tbm_surface_info_s tinfo;
   int i, res;

   if (!buf->tbm_surface)
     return EINA_FALSE;

   res = tbm_surface_get_info(buf->tbm_surface, &tinfo);
   if (res != TBM_SURFACE_ERROR_NONE)
     return EINA_FALSE;

   if (info)
     {
        info->width = tinfo.width;
        info->height = tinfo.height;
        info->format = tinfo.format;
        info->bpp = tinfo.bpp;
        info->size = tinfo.size;
        info->num_planes = tinfo.num_planes;
        info->pixmap = _ecore_buffer_x11_dri3_pixmap_get(bmdata, bdata);

        for (i = 0; i < tinfo.num_planes; i++)
          {
             info->planes[i].size = tinfo.planes[i].size;
             info->planes[i].offset = tinfo.planes[i].offset;
             info->planes[i].stride = tinfo.planes[i].stride;
          }
     }

   return EINA_TRUE;
}

static void
_ecore_buffer_x11_dri3_buffer_free(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri3_Data *buf = bdata;

   if (!buf)
     return;

   if (buf->pixmap)
     ecore_x_pixmap_free(buf->pixmap);

   if (buf->tbm_surface)
     tbm_surface_internal_unref(buf->tbm_surface);

   free(buf);
}

static Ecore_Export_Type
_ecore_buffer_x11_dri3_buffer_export(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata, int *id)
{
   Ecore_Buffer_X11_Dri3_Data *buf = bdata;
   tbm_bo bo;

   if (_buf_get_num_planes(buf->format) != 1)
     return EXPORT_TYPE_INVALID;

   bo = tbm_surface_internal_get_bo(buf->tbm_surface, 0);
   if (!bo)
     return EXPORT_TYPE_INVALID;

   if (id) *id = tbm_bo_export_fd(bo);

   return EXPORT_TYPE_FD;
}

static Ecore_Buffer_Data
_ecore_buffer_x11_dri3_buffer_import(Ecore_Buffer_Module_Data bmdata, Ecore_Buffer_Info *einfo, Ecore_Export_Type type, int export_id, unsigned int flags)
{
   Ecore_Buffer_Module_X11_Dri3_Data *bm = bmdata;
   Ecore_Buffer_X11_Dri3_Data *buf;
   tbm_bo bo;
   tbm_surface_info_s tinfo;
   int i;

   if (!bm)
     return NULL;

   if (type != EXPORT_TYPE_FD)
     return NULL;

   if (export_id < 1)
     return NULL;

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri3_Data));
   if (!buf)
     return NULL;

   buf->w = einfo->width;
   buf->h = einfo->height;
   buf->format = einfo->format;
   buf->flags = flags;
   buf->is_imported = EINA_TRUE;

   //Import tbm_surface
   bo = tbm_bo_import_fd(bm->tbm_mgr, export_id);
   if (!bo)
     {
        free(buf);
        return NULL;
     }

   tinfo.width = buf->w = einfo->width;
   tinfo.height = buf->h = einfo->height;
   tinfo.format = buf->format = einfo->format;
   tinfo.bpp = _buf_get_bpp(einfo->format);
   tinfo.size = einfo->height * einfo->planes[0].stride;
   tinfo.num_planes = _buf_get_num_planes(einfo->format);
   for (i = 0; i < tinfo.num_planes; i++)
   {
      tinfo.planes[i].size = einfo->height * einfo->planes[i].stride;
      tinfo.planes[i].stride = einfo->planes[i].stride;
      tinfo.planes[i].offset = einfo->planes[i].offset;
   }

   buf->tbm_surface = tbm_surface_internal_create_with_bos(&tinfo, &bo, 1);
   if (!buf->tbm_surface)
     {
        tbm_bo_unref(bo);
        free(buf);
        return NULL;
     }

   tbm_bo_unref(bo);

   return buf;
}

static Ecore_Pixmap
_ecore_buffer_x11_dri3_pixmap_get(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri3_Data *buf = bdata;
   Ecore_X_Display *xdpy;
   Ecore_X_Window root;
   tbm_surface_info_s info;
   tbm_bo bo;
   int ret;

   if (!buf)
     return 0;

   if (buf->pixmap)
     return buf->pixmap;

   ret = tbm_surface_get_info(buf->tbm_surface, &info);
   if (ret != 0)
     return 0;

   if (info.num_planes != 1)
     return 0;

   bo = tbm_surface_internal_get_bo(buf->tbm_surface, 0);
   if (!bo)
     return 0;

   xdpy = ecore_x_display_get();
   root = ecore_x_window_root_first_get();
   buf->pixmap = _dri3_pixmap_from_fd(xdpy, root,
                                      buf->w, buf->h,
                                      _buf_get_depth(buf->format),
                                      tbm_bo_export_fd(bo),
                                      _buf_get_bpp(buf->format),
                                      info.planes[0].stride,
                                      info.planes[0].size);

   return buf->pixmap;
}

static void *
_ecore_buffer_x11_dri3_tbm_bo_get(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri3_Data *buf = bdata;

   if (!buf)
     return NULL;

   return buf->tbm_surface;
}

static Ecore_Buffer_Backend _ecore_buffer_x11_dri3_backend = {
     "x11_dri3",
     &_ecore_buffer_x11_dri3_init,
     &_ecore_buffer_x11_dri3_shutdown,
     &_ecore_buffer_x11_dri3_buffer_alloc,
     &_ecore_buffer_x11_dri3_buffer_alloc_with_tbm_surface,
     &_ecore_buffer_x11_dri3_buffer_info_get,
     &_ecore_buffer_x11_dri3_buffer_free,
     &_ecore_buffer_x11_dri3_buffer_export,
     &_ecore_buffer_x11_dri3_buffer_import,
     &_ecore_buffer_x11_dri3_pixmap_get,
     &_ecore_buffer_x11_dri3_tbm_bo_get,
};

Eina_Bool x11_dri3_init(void)
{
   return ecore_buffer_register(&_ecore_buffer_x11_dri3_backend);
}

void x11_dri3_shutdown(void)
{
   ecore_buffer_unregister(&_ecore_buffer_x11_dri3_backend);
}

EINA_MODULE_INIT(x11_dri3_init);
EINA_MODULE_SHUTDOWN(x11_dri3_shutdown);
