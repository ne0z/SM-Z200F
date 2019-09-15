#include "evas_engine.h"


typedef struct _X_DRI3_Swapper X_DRI3_Swapper;

X_DRI3_Swapper *evas_xcb_swapper_new(Display *disp, Drawable draw, Visual *vis,
                                 int depth, int w, int h);
void evas_xcb_swapper_free(X_DRI3_Swapper *swp);
void *evas_xcb_swapper_buffer_map(X_DRI3_Swapper *swp, int *bpl, int *w, int *h);
void evas_xcb_swapper_buffer_unmap(X_DRI3_Swapper *swp);
void evas_xcb_swapper_swap(X_DRI3_Swapper *swp, Eina_Rectangle *rects, int nrects);
Render_Engine_Swap_Mode evas_xcb_swapper_buffer_state_get(X_DRI3_Swapper *swp);
int evas_xcb_swapper_depth_get(X_DRI3_Swapper *swp);
int evas_xcb_swapper_byte_order_get(X_DRI3_Swapper *swp);
int evas_xcb_swapper_bit_order_get(X_DRI3_Swapper *swp);
// TIZEN_ONLY [[
void evas_xcb_swapper_buffer_size_get(X_DRI3_Swapper *swp, int *w, int *h);
// ]]
