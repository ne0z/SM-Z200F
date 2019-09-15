#include "evas_common_private.h"
#include "evas_xlib_image.h"
#include "evas_private.h"

#include "Evas_Engine_Software_X11.h"
#include "evas_engine.h"

#define EVAS_ROUND_UP_4(num) (((num)+3) & ~3)
#define EVAS_ROUND_UP_8(num) (((num)+7) & ~7)

static void
_evas_video_yv12(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;
   unsigned int stride_y, stride_uv;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   stride_y = EVAS_ROUND_UP_4(w);
   stride_uv = EVAS_ROUND_UP_8(w) / 2;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * stride_y];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y +
                            (rh / 2) * stride_uv +
                            j * stride_uv];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y + j * stride_uv];
}

static void
_evas_video_i420(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;
   unsigned int stride_y, stride_uv;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   stride_y = w;
   stride_uv = w / 2;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * stride_y];

   for (j = 0; j < ((rh + 1) / 2); j++, i++)
     rows[i] = &source_data[h * stride_y + j * stride_uv];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y +
                            ((rh + 1) / 2) * stride_uv +
                            j * stride_uv];
}

static void
_evas_video_nv12(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h EINA_UNUSED, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * w];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[rh * w + j * w];
}

_native_bind_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   Native *n;
   tbm_surface_info_s info;

   if (!im) return;

   n = im->native.data;

   if (!n) return;

   sym_tbm_surface_map(n->ns.data.tbm.buffer, TBM_SURF_OPTION_READ, &info);
}

static void
_native_unbind_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   Native *n;

   if (!im) return;

   n = im->native.data;

   if (!n) return;

   sym_tbm_surface_unmap(n->ns.data.tbm.buffer);
}

static void
_native_free_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   if (!im) return;
   Native *n = im->native.data;

   im->native.data        = NULL;
   im->native.func.bind   = NULL;
   im->native.func.unbind = NULL;
   im->native.func.free   = NULL;
   im->native.func.data   = NULL;

   free(n);
}

int
evas_native_tbm_stride_get(void *data EINA_UNUSED, void *native)
 {
   Evas_Native_Surface *ns = native;
   tbm_surface_info_s info;
   int stride;

   if (!ns) return -1;

   if (sym_tbm_surface_get_info(ns->data.tbm.buffer, &info))
     return -1;

   stride = info.planes[0].stride;
   return stride;
 }

void *
evas_native_tbm_image_set(void *data EINA_UNUSED, void *image, void *native)
{
   Evas_Native_Surface *ns = native;
   RGBA_Image *im = image;

   if (!im) return NULL;
   if ((ns) && (ns->type == EVAS_NATIVE_SURFACE_TBM))
     {
        void *pixels_data;
        int w, h, stride;
        tbm_format format;
        tbm_surface_info_s info;
        Native *n;
/*
        if (!tbm_init())
          {
             ERR("Could not initialize TBM!");
             return NULL;
          }
*/
        n = calloc(1, sizeof(Native));
        if (!n) return NULL;

        if (sym_tbm_surface_map(ns->data.tbm.buffer, TBM_SURF_OPTION_READ|TBM_SURF_OPTION_WRITE, &info))
          {
             free(n);
             return im;
          }

        w = info.width;
        h = info.height;
        stride = info.planes[0].stride;
        format = info.format;
        pixels_data = info.planes[0].ptr;
        im->cache_entry.w = stride;
        im->cache_entry.h = h;

        // Handle all possible format here :"(
        switch (format)
          {
           case TBM_FORMAT_RGBA8888:
           case TBM_FORMAT_RGBX8888:
           case TBM_FORMAT_BGRA8888:
              im->cache_entry.w = stride / 4;
              evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_ARGB8888);
              im->cache_entry.flags.alpha = (format == TBM_FORMAT_RGBX8888 ? 0 : 1);
              im->image.data = pixels_data;
              im->image.no_free = 1;
              break;
              /* borrowing code from emotion here */
           case TBM_FORMAT_YVU420: /* EVAS_COLORSPACE_YCBCR422P601_PL */
              evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR422P601_PL);
              _evas_video_yv12(im->cs.data, pixels_data, stride, h, h);
              evas_common_image_colorspace_dirty(im);
              break;
           case TBM_FORMAT_YUV420: /* EVAS_COLORSPACE_YCBCR422P601_PL */
              evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR422P601_PL);
              _evas_video_i420(im->cs.data, pixels_data, stride, h, h);
              evas_common_image_colorspace_dirty(im);
              break;
           case TBM_FORMAT_NV12: /* EVAS_COLORSPACE_YCBCR420NV12601_PL */
              evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR420NV12601_PL);
              _evas_video_nv12(im->cs.data, pixels_data, stride, h, h);
              evas_common_image_colorspace_dirty(im);
              break;
              /* Not planning to handle those in software */
           default:
              sym_tbm_surface_unmap(ns->data.tbm.buffer);
              free(n);
              return im;
          }

        memcpy(n, ns, sizeof(Evas_Native_Surface));
        im->native.data = n;
        im->native.func.bind   = _native_bind_cb;
        im->native.func.unbind = _native_unbind_cb;
        im->native.func.free   = _native_free_cb;

        sym_tbm_surface_unmap(ns->data.tbm.buffer);
     }
   return im;
}

