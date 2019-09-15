#include "evas_engine.h"

typedef struct _DRI3_Native DRI3_Native;
typedef struct _DRI3_Info DRI3_Info;

struct _DRI3_Native
{
    Evas_Native_Surface ns;
    Pixmap              pixmap;
    Visual             *visual;
    Display            *d;
    DRI3_Info           *info;
};

struct _DRI3_Info {
    void                *buffer;
};

void *evas_xcb_image_dri3_native_set(void *data, void *image, void *native);
Eina_Bool evas_xcb_image_get_buffers(RGBA_Image *im);
