
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "evas_common_private.h"
#include "evas_macros.h"
#include "evas_xcb_swapper.h"
#include "dri3/evas_xcb_dri3.h"

#ifdef HAVE_DLSYM

struct _X_DRI3_Swapper
{
   Display        *disp;
   dri3_drawable  *dri3_draw;
   Drawable       draw;
   Visual         *vis;
   int            w, h, depth;
   int            last_count;
   dri3_buffer    *dri3_buf;
   void           *dri3_buf_data;
   Eina_Bool      mapped: 1;
};

static int inits = 0;
static int swap_debug = 0;

X_DRI3_Swapper *
evas_xcb_swapper_new(Display *disp, Drawable draw, Visual *vis,
                     int depth, int w, int h)
{
   X_DRI3_Swapper *swp;

   if (inits <= 0)
      {
         if (!dri3_init_dri3(disp)) return NULL;
         if (!dri3_query_dri3()) return NULL;
         if (!dri3_query_present()) return NULL;
         if (!dri3_XFixes_query(disp)) return NULL;
      }
   inits++;

   swp = calloc(1, sizeof(X_DRI3_Swapper));
   if (!swp) return NULL;
   swp->disp = disp;
   swp->draw = draw;
   swp->vis = vis;
   swp->depth = depth;
   swp->w = w;
   swp->h = h;
   swp->last_count = -1;
   if (swp->depth == 24) swp->depth = 32;

   swp->dri3_draw = dri3_create_drawable(draw, w, h, swp->depth);
   if (!swp->dri3_draw)
      {
         inits--;
         free(swp);
         if (inits == 0) dri3_deinit_dri3();
         return NULL;
      }

   if (swap_debug) DBG("Swapper allocated OK");
   return swp;
}

void
evas_xcb_swapper_free(X_DRI3_Swapper *swp)
{
   if (swap_debug) DBG("Swapper free");
   if (swp->mapped) evas_xcb_swapper_buffer_unmap(swp);
   dri3_destroy_drawable(swp->dri3_draw);
   free(swp);
   inits--;
   if (inits == 0) dri3_deinit_dri3();
}

void *
evas_xcb_swapper_buffer_map(X_DRI3_Swapper *swp, int *bpl, int *w, int *h)
{
   if (swp->mapped)
      {
         if (bpl)
            {
               if ((swp->dri3_buf) && (swp->dri3_buf->stride > 0)) *bpl = swp->dri3_buf->stride;
               else *bpl = swp->w * 4;
            }
         if (w) *w = swp->w;
         if (h) *h = swp->h;
         return swp->dri3_buf_data;
      }

   swp->dri3_buf = dri3_get_backbuffers(swp->dri3_draw, 0);
   if (!swp->dri3_buf) return NULL;
   swp->dri3_buf_data = dri3_tbm_bo_handle_map(swp->dri3_buf);
   if (!swp->dri3_buf_data)
      {
         ERR("Buffer map failed");
         return NULL;
      }

   if (bpl) *bpl = swp->dri3_buf->stride;

   swp->mapped = EINA_TRUE;
   if ((swp->w != swp->dri3_buf->w) || (swp->h != swp->dri3_buf->h))
      {
         DBG("Evas software DRI swapper buffer size mismatch");
      }
   swp->w = swp->dri3_buf->w;
   swp->h = swp->dri3_buf->h;
   if (w) *w = swp->w;
   if (h) *h = swp->h;

   return swp->dri3_buf_data;
}

void
evas_xcb_swapper_buffer_unmap(X_DRI3_Swapper *swp)
{
   if (!swp->mapped) return;
   if (swap_debug) DBG("Unmap buffer");
   dri3_tbm_bo_handle_unmap(swp->dri3_buf);
   swp->mapped = EINA_FALSE;
}

void
evas_xcb_swapper_swap(X_DRI3_Swapper *swp, Eina_Rectangle *rects, int nrects)
{
   XRectangle *xrects = alloca(nrects * sizeof(XRectangle));
   XID region = 0;
   int i;
   if (swap_debug) DBG("Swap buffers");
   for (i = 0; i < nrects; i++)
      {
         xrects[i].x = rects[i].x; xrects[i].y = rects[i].y;
         xrects[i].width = rects[i].w; xrects[i].height = rects[i].h;
      }
   region = dri3_XFixes_create_region(swp->disp, xrects, nrects);
   dri3_swap_buffers(swp->dri3_draw, 0, 0, 0, region);
   dri3_XFixes_destory_region(swp->disp, region);
}

Render_Engine_Swap_Mode
evas_xcb_swapper_buffer_state_get(X_DRI3_Swapper *swp)
{
   if (!swp->mapped) evas_xcb_swapper_buffer_map(swp, NULL, NULL, NULL);
   if (!swp->mapped) return MODE_FULL;
   int buffer_age = dri3_get_buffer_age(swp->dri3_draw);
   if (buffer_age != swp->last_count)
      {
         swp->last_count = buffer_age;
         if (swap_debug) DBG("Reuse changed - force FULL");
         return MODE_FULL;
      }
   if (swap_debug) DBG("Swap state buffer_age = %i (0=FULL, 1=COPY, 2=DOUBLE, 3=TRIPLE, 4=QUAD)", buffer_age);
   switch (buffer_age)
   {
      case 0:
      case 1:
         return MODE_FULL;
      case 2:
         return MODE_DOUBLE;
      case 3:
         return MODE_TRIPLE;
      case 4:
         return MODE_QUADRUPLE;
      default :
         return MODE_FULL;
   }

   return MODE_FULL;
}

int
evas_xcb_swapper_depth_get(X_DRI3_Swapper *swp)
{
   return swp->depth;
}

int
evas_xcb_swapper_byte_order_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return LSBFirst;
}

int
evas_xcb_swapper_bit_order_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return LSBFirst;
}

// TIZEN_ONLY [[
void
evas_xcb_swapper_buffer_size_get(X_DRI3_Swapper *swp, int *w, int *h)
{
   if (!swp || !swp->dri3_buf) return;

   *w = swp->dri3_buf->w;
   *h = swp->dri3_buf->h;

   return;
}
// ]]

#else

X_DRI3_Swapper *
evas_xcb_swapper_new(Display *disp EINA_UNUSED, Drawable draw EINA_UNUSED,
                     Visual *vis EINA_UNUSED, int depth EINA_UNUSED,
                     int w EINA_UNUSED, int h EINA_UNUSED)
{
   return NULL;
}

void
evas_xcb_swapper_free(X_DRI3_Swapper *swp EINA_UNUSED)
{
}

void *
evas_xcb_swapper_buffer_map(X_DRI3_Swapper *swp EINA_UNUSED, int *bpl EINA_UNUSED, int *w EINA_UNUSED, int *h EINA_UNUSED)
{
   return NULL;
}

void
evas_xcb_swapper_buffer_unmap(X_DRI3_Swapper *swp EINA_UNUSED)
{
}

void
evas_xcb_swapper_swap(X_DRI3_Swapper *swp EINA_UNUSED, Eina_Rectangle *rects EINA_UNUSED, int nrects EINA_UNUSED)
{
}

Render_Engine_Swap_Mode
evas_xcb_swapper_buffer_state_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return MODE_FULL;
}

int
evas_xcb_swapper_depth_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return 0;
}

int
evas_xcb_swapper_byte_order_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return 0;
}

int
evas_xcb_swapper_bit_order_get(X_DRI3_Swapper *swp EINA_UNUSED)
{
   return 0;
}
#endif
