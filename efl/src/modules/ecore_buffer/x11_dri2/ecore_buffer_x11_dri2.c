#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include <tbm_bufmgr.h>
#include <tbm_surface.h>
#include <tbm_surface_internal.h>

#include <xf86drm.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>

#include "Ecore_Buffer.h"
#include "ecore_buffer_private.h"

typedef struct _Ecore_Buffer_Module_X11_Dri2_Data Ecore_Buffer_Module_X11_Dri2_Data;
typedef struct _Ecore_Buffer_X11_Dri2_Data Ecore_Buffer_X11_Dri2_Data;

struct _Ecore_Buffer_Module_X11_Dri2_Data {
     tbm_bufmgr tbm_mgr;
};

struct _Ecore_Buffer_X11_Dri2_Data {
     Ecore_X_Pixmap pixmap;
     int w;
     int h;
     int stride;
     Ecore_Buffer_Format format;
     Eina_Bool is_imported;

     struct
     {
        void *surface;
        Eina_Bool owned;
     } tbm;
};

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

static Ecore_Buffer_Module_Data
_ecore_buffer_x11_dri2_init(const char *context EINA_UNUSED, const char *options EINA_UNUSED)
{
   Ecore_X_Display *xdpy;
   Ecore_X_Window root;
   int eb, ee;
   int major, minor;
   char *driver_name = NULL;
   char *device_name = NULL;
   int fd = 0;
   drm_magic_t magic;
   Ecore_Buffer_Module_X11_Dri2_Data *mdata = NULL;

   if (!ecore_x_init(NULL))
     return NULL;

   xdpy = ecore_x_display_get();
   if (!xdpy)
     goto on_error;

   root = ecore_x_window_root_first_get();
   if (!root)
     goto on_error;

   mdata = calloc(1, sizeof(Ecore_Buffer_Module_X11_Dri2_Data));
   if (!mdata)
     goto on_error;

   //Init DRI2 and TBM
   DRI2QueryExtension(xdpy, &eb, &ee);
   DRI2QueryVersion(xdpy, &major, &minor);
   DRI2Connect(xdpy, root, &driver_name, &device_name);

   fd = open (device_name, O_RDWR);
   if (fd < 0)
     goto on_error;

   if (drmGetMagic(fd, &magic) < 0)
     goto on_error;

   if (!(DRI2Authenticate(xdpy, root, magic)))
     goto on_error;

   mdata->tbm_mgr = tbm_bufmgr_init(fd);
   if (!mdata->tbm_mgr)
     goto on_error;

   free(driver_name);
   free(device_name);
   close(fd);

   return mdata;

on_error:
   if (fd > 0) close(fd);
   if (driver_name) free(driver_name);
   if (device_name) free(device_name);
   if (mdata) free(mdata);
   ecore_x_shutdown();

   return NULL;
}

static void
_ecore_buffer_x11_dri2_shutdown(Ecore_Buffer_Module_Data bmdata)
{
   Ecore_Buffer_Module_X11_Dri2_Data *bm = bmdata;

   if (bm->tbm_mgr)
     tbm_bufmgr_deinit(bm->tbm_mgr);

   ecore_x_shutdown();
}

static Ecore_Buffer_Data
_ecore_buffer_x11_dri2_buffer_alloc(Ecore_Buffer_Module_Data bmdata, int width, int height, Ecore_Buffer_Format format, unsigned int flags EINA_UNUSED)
{
   Ecore_X_Display *xdpy;
   Ecore_X_Pixmap pixmap;
   Ecore_Buffer_X11_Dri2_Data *buf;
   Ecore_Buffer_Module_X11_Dri2_Data *bm = bmdata;
   DRI2Buffer *bufs = NULL;
   tbm_bo bo = NULL;
   int bpp;
   int num_plane;
   int rw, rh, rcount;
   unsigned int attachment = DRI2BufferFrontLeft;
   tbm_surface_info_s info;
   int i;

   bpp = _buf_get_bpp(format);
   if (bpp != 32)
     return NULL;

   num_plane = _buf_get_num_planes(format);
   if (num_plane != 1)
     return NULL;

   xdpy = ecore_x_display_get();
   pixmap = ecore_x_pixmap_new(0, width, height, bpp);
   if (!pixmap)
     return NULL;

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri2_Data));
   if (!buf)
     {
        ecore_x_pixmap_free(pixmap);
        return NULL;
     }

   buf->w = width;
   buf->h = height;
   buf->format = format;
   buf->pixmap = pixmap;
   buf->is_imported = EINA_FALSE;

   //Get DRI2Buffer
   DRI2CreateDrawable(xdpy, buf->pixmap);
   bufs = DRI2GetBuffers(xdpy, buf->pixmap, &rw, &rh, &attachment, 1, &rcount);
   if (!(bufs) || (buf->w != rw) || (buf->h != rh))
     goto on_error;

   buf->stride = bufs->pitch;

   //Import tbm_surface
   bo = tbm_bo_import(bm->tbm_mgr, bufs->name);
   if (!bo)
     goto on_error;

   info.width = width;
   info.height =  height;
   info.format = format;
   info.bpp = bpp;
   info.size = height * bufs->pitch;
   info.num_planes = num_plane;
   for ( i = 0 ; i < num_plane ; i++)
   {
      info.planes[i].size = height * bufs->pitch;
      info.planes[i].stride = bufs->pitch;
      info.planes[i].offset = 0;
   }

   buf->tbm.surface = tbm_surface_internal_create_with_bos(&info, &bo, 1);
   if (!buf->tbm.surface)
     goto on_error;

   buf->tbm.owned = EINA_TRUE;
   tbm_bo_unref(bo);
   free(bufs);

   return buf;

on_error:
   if (bo) tbm_bo_unref(bo);
   if (bufs) free(bufs);
   ecore_x_pixmap_free(buf->pixmap);
   DRI2DestroyDrawable(xdpy, buf->pixmap);
   free(buf);

   return NULL;
}

static Ecore_Buffer_Data
_ecore_buffer_x11_dri2_buffer_alloc_with_tbm_surface(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, void *tbm_surface, int *ret_w, int *ret_h, Ecore_Buffer_Format *ret_format, unsigned int flags EINA_UNUSED)
{
   Ecore_Buffer_X11_Dri2_Data *buf;

   EINA_SAFETY_ON_NULL_RETURN_VAL(tbm_surface, NULL);

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri2_Data));
   if (!buf)
     return NULL;

   buf->w = tbm_surface_get_width(tbm_surface);
   buf->h = tbm_surface_get_height(tbm_surface);
   buf->format = tbm_surface_get_format(tbm_surface);
   buf->pixmap = 0;
   buf->tbm.owned = EINA_FALSE;
   buf->is_imported = EINA_FALSE;

   tbm_surface_internal_ref(tbm_surface);
   buf->tbm.surface = tbm_surface;

   if (ret_w) *ret_w = buf->w;
   if (ret_h) *ret_h = buf->h;
   if (ret_format) *ret_format = buf->format;

   return buf;
}

static Eina_Bool
_ecore_buffer_x11_dri2_buffer_info_get(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata, Ecore_Buffer_Info *info)
{
   Ecore_Buffer_X11_Dri2_Data *buf = bdata;
   tbm_surface_info_s tinfo;
   int i, res;

   if (!buf->tbm.surface)
     return EINA_FALSE;

   res = tbm_surface_get_info(buf->tbm.surface, &tinfo);
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
        info->pixmap = buf->pixmap;

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
_ecore_buffer_x11_dri2_buffer_free(Ecore_Buffer_Module_Data bmdata, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri2_Data *buf = bdata;
   Ecore_Buffer_Module_X11_Dri2_Data *bm = bmdata;

   if (buf->pixmap)
     {
        DRI2DestroyDrawable(ecore_x_display_get(), buf->pixmap);

        if (!buf->is_imported)
          ecore_x_pixmap_free(buf->pixmap);
     }

   if (buf->tbm.surface)
     tbm_surface_internal_unref(buf->tbm.surface);

   free(buf);

   return;
}

static Ecore_Export_Type
_ecore_buffer_x11_dri2_buffer_export(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata, int *id)
{
   Ecore_Buffer_X11_Dri2_Data *buf = bdata;
   tbm_bo bo;

   if (id)
     {
        if (buf->pixmap)
          *id = buf->pixmap;
        else
          {
             tbm_bo bo;
             /* NOTE: constraints - cannot support more than two bos */
             bo = tbm_surface_internal_get_bo(buf->tbm.surface, 0);
             *id = (int)(tbm_bo_export(bo));
          }
     }

   return EXPORT_TYPE_ID;
}

static tbm_bo
_x11_dri2_bo_get_from_pixmap(tbm_bufmgr tbm_mgr, Ecore_Pixmap pixmap, int width, int height)
{
   Ecore_X_Display *xdpy;
   DRI2Buffer *bufs = NULL;
   tbm_bo bo = NULL;
   int rw, rh, rx, ry;
   int rcount;
   unsigned int attachment = DRI2BufferFrontLeft;

   // Check valid pixmap
   ecore_x_pixmap_geometry_get(pixmap, &rx, &ry, &rw, &rh);
   if ((rw != width) || (rh != height))
     return NULL;

   xdpy = ecore_x_display_get();
   DRI2CreateDrawable(xdpy, pixmap);
   bufs = DRI2GetBuffers(xdpy, pixmap, &rw, &rh, &attachment, 1, &rcount);
   if ((!bufs) || (width != rw) || (height != rh))
     goto err;

   bo = tbm_bo_import(tbm_mgr, bufs->name);
   free(bufs);
   if (!bo)
     goto err;

   return bo;
err:
   DRI2DestroyDrawable(xdpy, pixmap);
   return NULL;
}

static void *
_ecore_buffer_x11_dri2_buffer_import(Ecore_Buffer_Module_Data bmdata, Ecore_Buffer_Info *einfo, Ecore_Export_Type type, int export_id, unsigned int flags EINA_UNUSED)
{
   Ecore_Buffer_Module_X11_Dri2_Data *bm = bmdata;
   Ecore_Buffer_X11_Dri2_Data *buf;
   tbm_bo bo = NULL;
   tbm_surface_h surface;
   tbm_surface_info_s tinfo;
   int i;

   if (type != EXPORT_TYPE_ID)
     return NULL;

   if (!einfo)
     return NULL;

   if (einfo->pixmap)
     bo = _x11_dri2_bo_get_from_pixmap(bm->tbm_mgr, einfo->pixmap, einfo->width, einfo->height);
   else
     bo = tbm_bo_import(bm->tbm_mgr, export_id);

   if (!bo)
     goto err;

   tinfo.width = einfo->width;
   tinfo.height = einfo->height;
   tinfo.format = einfo->format;
   tinfo.bpp = _buf_get_bpp(einfo->format);
   tinfo.size = einfo->height * einfo->planes[0].stride;
   tinfo.num_planes = _buf_get_num_planes(einfo->format);
   for (i = 0; i < tinfo.num_planes; i++)
     {
        tinfo.planes[i].size = einfo->height * einfo->planes[i].stride;
        tinfo.planes[i].stride = einfo->planes[i].stride;
        tinfo.planes[i].offset = einfo->planes[i].offset;
     }

   surface = tbm_surface_internal_create_with_bos(&tinfo, &bo, 1);
   tbm_bo_unref(bo);
   if (!surface)
     goto err;

   buf = calloc(1, sizeof(Ecore_Buffer_X11_Dri2_Data));
   if (!buf)
     goto err_alloc;

   buf->w = einfo->width;
   buf->h = einfo->height;
   buf->format = einfo->format;
   buf->pixmap = einfo->pixmap;
   buf->stride = einfo->planes[0].stride;
   buf->is_imported = EINA_TRUE;
   buf->tbm.surface = surface;
   buf->tbm.owned = EINA_TRUE;

   return buf;
err_alloc:
   tbm_surface_internal_unref(surface);
err:
   if ((bo) && (einfo->pixmap))
     DRI2DestroyDrawable(ecore_x_display_get(), einfo->pixmap);

   free(buf);

   return NULL;
}

static Ecore_Pixmap
_ecore_buffer_x11_dri2_pixmap_get(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri2_Data *buf = bdata;

   if (!buf)
     return 0;

   if (!buf->tbm.owned)
     return 0;

   return buf->pixmap;
}

static void *
_ecore_buffer_x11_dri2_tbm_bo_get(Ecore_Buffer_Module_Data bmdata EINA_UNUSED, Ecore_Buffer_Data bdata)
{
   Ecore_Buffer_X11_Dri2_Data *buf = bdata;

   if (!buf)
     return NULL;

   return buf->tbm.surface;
}

static Ecore_Buffer_Backend _ecore_buffer_x11_dri2_backend = {
     "x11_dri2",
     &_ecore_buffer_x11_dri2_init,
     &_ecore_buffer_x11_dri2_shutdown,
     &_ecore_buffer_x11_dri2_buffer_alloc,
     &_ecore_buffer_x11_dri2_buffer_alloc_with_tbm_surface,
     &_ecore_buffer_x11_dri2_buffer_info_get,
     &_ecore_buffer_x11_dri2_buffer_free,
     &_ecore_buffer_x11_dri2_buffer_export,
     &_ecore_buffer_x11_dri2_buffer_import,
     &_ecore_buffer_x11_dri2_pixmap_get,
     &_ecore_buffer_x11_dri2_tbm_bo_get,
};

Eina_Bool x11_dri2_init(void)
{
   return ecore_buffer_register(&_ecore_buffer_x11_dri2_backend);
}

void x11_dri2_shutdown(void)
{
   ecore_buffer_unregister(&_ecore_buffer_x11_dri2_backend);
}

EINA_MODULE_INIT(x11_dri2_init);
EINA_MODULE_SHUTDOWN(x11_dri2_shutdown);
