
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "evas_common_private.h"
#include "dri3/evas_xcb_dri3.h"
#include "evas_xcb_dri3_image.h"

static int inits = 0;

DRI3_Info *
evas_xcb_image_dri_new()
{
   DRI3_Info *info;
   info = calloc(1, sizeof(DRI3_Info));
   if (!info)
      return NULL;

   return info;
}

void
evas_xcb_image_dri_free(DRI3_Info *info)
{
   if (!info) return;
   inits--;
   dri3_destroy_buffer(info->buffer);
   if (inits == 0) dri3_deinit_dri3();
   free(info);
}

Eina_Bool
evas_xcb_image_dri3_init(Display *dpy)
{
   if (inits <= 0)
      {
         if(!dri3_init_dri3(dpy)) return EINA_FALSE;
         if(!dri3_query_dri3()) return EINA_FALSE;
         if(!dri3_query_present()) return EINA_FALSE;
      }
   inits++;
   return EINA_TRUE;
}


Eina_Bool
evas_xcb_image_get_buffers(RGBA_Image *im)
{
   DRI3_Native *n = NULL;
   Display *d;
   DRI3_Info *info;

   if (im->native.data)
      n = im->native.data;
   if (!n || !n->d  || !n->info || inits <= 0) return EINA_FALSE;

   info = n->info;
   d = n->d;

   XGrabServer(d);

   dri3_destroy_buffer(info->buffer);
   info->buffer = dri3_get_pixmap_buffer(n->pixmap);
   dri3_get_data(info->buffer, im);

   XUngrabServer(d);
   XSync(d, 0);

   if (!im->image.data) return EINA_FALSE;

   return EINA_TRUE;
}

static void
_native_bind_cb(void *data EINA_UNUSED, void *image,
                int x EINA_UNUSED, int y EINA_UNUSED, int w EINA_UNUSED, int h EINA_UNUSED)
{
   RGBA_Image *im = image;
   DRI3_Native *n = im->native.data;

   if ((n) && (n->ns.type == EVAS_NATIVE_SURFACE_X11))
      {
         if (evas_xcb_image_get_buffers(im))
            {
               evas_common_image_colorspace_dirty(im);
            }
      }
}

static void
_native_free_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   DRI3_Native *n = im->native.data;
   if (!n) return;
   DRI3_Info *info = n->info;
   evas_xcb_image_dri_free(info);
   n->visual = NULL;
   n->d = NULL;

   im->native.data        = NULL;
   im->native.func.data   = NULL;
   im->native.func.bind   = NULL;
   im->native.func.free   = NULL;
   im->image.data         = NULL;
   free(n);
}

void *
evas_xcb_image_dri3_native_set(void *data, void *image, void *native)
{
   Display *d = NULL;
   Visual  *vis = NULL;
   Pixmap   pm = 0;
   DRI3_Native  *n = NULL;
   RGBA_Image *im = image;

   Evas_Native_Surface *ns = native;
   Outbuf *ob = (Outbuf *)data;

   if(!ns || ns->type != EVAS_NATIVE_SURFACE_X11)
      return EINA_FALSE;

   d = ob->priv.x11.xlib.disp;
   vis = ns->data.x11.visual;
   pm = ns->data.x11.pixmap;
   if (!pm) return EINA_FALSE;


   DRI3_Info *dri_info = evas_xcb_image_dri_new();
   if (!dri_info) return EINA_FALSE;

   n = calloc(1, sizeof(DRI3_Native));
   if (!n)
      {
         free(dri_info);
         return EINA_FALSE;
      }

   memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
   n->pixmap = pm;
   n->visual = vis;
   n->d = d;
   n->info = dri_info;

   im->native.data = n;
   im->native.func.data   = NULL;
   im->native.func.bind   = _native_bind_cb;
   im->native.func.unbind = NULL;
   im->native.func.free   = _native_free_cb;

   if (evas_xcb_image_dri3_init(d)) evas_xcb_image_get_buffers(im);
   else
      {
         INF("evas_xcb_image_dri3_init failed. return false");
         return EINA_FALSE;
      }

   return im;
}

