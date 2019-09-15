#ifndef EVAS_ENGINE_H
# define EVAS_ENGINE_H

# include <sys/ipc.h>
# include <sys/shm.h>

# ifdef BUILD_ENGINE_SOFTWARE_XLIB
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#  include <X11/Xatom.h>
#  include <X11/extensions/XShm.h>
#  include <X11/Xresource.h> // xres - dpi
# endif

# ifdef BUILD_ENGINE_SOFTWARE_XCB
#  include <xcb/xcb.h>
#  include <xcb/shm.h>
#  include <xcb/xcb_image.h>
# endif

#include "../software_generic/Evas_Engine_Software_Generic.h"

extern int _evas_engine_soft_x11_log_dom;

# ifdef ERR
#  undef ERR
# endif
# define ERR(...) EINA_LOG_DOM_ERR(_evas_engine_soft_x11_log_dom, __VA_ARGS__)

# ifdef DBG
#  undef DBG
# endif
# define DBG(...) EINA_LOG_DOM_DBG(_evas_engine_soft_x11_log_dom, __VA_ARGS__)

# ifdef INF
#  undef INF
# endif
# define INF(...) EINA_LOG_DOM_INFO(_evas_engine_soft_x11_log_dom, __VA_ARGS__)

# ifdef WRN
#  undef WRN
# endif
# define WRN(...) EINA_LOG_DOM_WARN(_evas_engine_soft_x11_log_dom, __VA_ARGS__)

# ifdef CRI
#  undef CRI
# endif
# define CRI(...) \
   EINA_LOG_DOM_CRIT(_evas_engine_soft_x11_log_dom, __VA_ARGS__)

struct _Outbuf
{
   Outbuf_Depth depth;
   int w, h;
   int rot;
   int onebuf;

   struct 
     {
        Convert_Pal *pal;
        union 
          {
# ifdef BUILD_ENGINE_SOFTWARE_XLIB
             struct 
               {
                  Display *disp;
                  Drawable win;
                  Pixmap mask;
                  Visual *vis;
                  Colormap cmap;
                  int depth, imdepth, shm;
                  GC gc, gcm;
                  unsigned char swap : 1;
                  unsigned char bit_swap : 1;
               } xlib;
# endif
# ifdef BUILD_ENGINE_SOFTWARE_XCB
             struct 
               {
                  xcb_connection_t *conn;
                  xcb_screen_t *screen;
                  xcb_window_t win;
                  xcb_pixmap_t mask;
                  xcb_visualtype_t *visual;
                  xcb_colormap_t cmap;
                  int depth, imdepth, shm;
                  xcb_gcontext_t gc, gcm;
                  unsigned char swap : 1;
                  unsigned char bit_swap : 1;
               } xcb;
# endif
          } x11;
        struct 
          {
             DATA32 r, g, b;
          } mask;

        /* 1 big buffer for updates - flush on idle_flush */
        RGBA_Image *onebuf;
        Eina_Array  onebuf_regions;
        
        void *swapper;

        /* a list of pending regions to write to the target */
        Eina_List *pending_writes;

        /* a list of previous frame pending regions to write to the target */
        Eina_List *prev_pending_writes;

        unsigned char mask_dither : 1;
        unsigned char destination_alpha : 1;
        unsigned char debug : 1;
        unsigned char synced : 1;
     } priv;
};

////////////////////////////////////
// libtbm.so

#define TBM_SURF_PLANE_MAX 4 /**< maximum number of the planes  */

#define TBM_DEVICE_CPU 1

#define TBM_BO_DEFAULT 0

#define TBM_OPTION_READ     (1 << 0)
#define TBM_OPTION_WRITE    (1 << 1)

   /* option to map the tbm_surface */
#define TBM_SURF_OPTION_READ      (1 << 0) /**< access option to read  */
#define TBM_SURF_OPTION_WRITE     (1 << 1) /**< access option to write */

#define __tbm_fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define TBM_FORMAT_RGBX8888	__tbm_fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define TBM_FORMAT_RGBA8888	__tbm_fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define TBM_FORMAT_BGRA8888	__tbm_fourcc_code('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */
#define TBM_FORMAT_NV12		__tbm_fourcc_code('N', 'V', '1', '2') /* 2x2 subsampled Cr:Cb plane */
#define TBM_FORMAT_YUV420	__tbm_fourcc_code('Y', 'U', '1', '2') /* 2x2 subsampled Cb (1) and Cr (2) planes */
#define TBM_FORMAT_YVU420	__tbm_fourcc_code('Y', 'V', '1', '2') /* 2x2 subsampled Cr (1) and Cb (2) planes */

typedef struct _tbm_surface * tbm_surface_h;
typedef struct _tbm_bufmgr *tbm_bufmgr;
typedef struct _tbm_bo *tbm_bo;
typedef uint32_t tbm_format;

typedef struct _tbm_surface_plane
{
    unsigned char *ptr;   /**< Plane pointer */
    uint32_t size;        /**< Plane size */
    uint32_t offset;      /**< Plane offset */
    uint32_t stride;      /**< Plane stride */

    void *reserved1;      /**< Reserved pointer1 */
    void *reserved2;      /**< Reserved pointer2 */
    void *reserved3;      /**< Reserved pointer3 */
} tbm_surface_plane_s;

typedef struct _tbm_surface_info
{
    uint32_t width;      /**< TBM surface width */
    uint32_t height;     /**< TBM surface height */
    tbm_format format;   /**< TBM surface format*/
    uint32_t bpp;        /**< TBM surface bbp */
    uint32_t size;       /**< TBM surface size */

    uint32_t num_planes;                            /**< The number of planes */
    tbm_surface_plane_s planes[TBM_SURF_PLANE_MAX]; /**< Array of planes */

    void *reserved4;   /**< Reserved pointer4 */
    void *reserved5;   /**< Reserved pointer5 */
    void *reserved6;   /**< Reserved pointer6 */
} tbm_surface_info_s;

typedef union _tbm_bo_handle
{
   void     *ptr;
   int32_t  s32;
   uint32_t u32;
   int64_t  s64;
   uint64_t u64;
} tbm_bo_handle;

/* returns 0 on success */
extern int (*sym_tbm_surface_map) (tbm_surface_h surface, int opt, tbm_surface_info_s *info);
extern int (*sym_tbm_surface_unmap) (tbm_surface_h surface);
extern int (*sym_tbm_surface_get_info) (tbm_surface_h surface, tbm_surface_info_s *info);

extern tbm_bo (*sym_tbm_bo_import) (tbm_bufmgr bufmgr, unsigned int key);
extern tbm_bo_handle (*sym_tbm_bo_map) (tbm_bo bo, int device, int opt);
extern int (*sym_tbm_bo_unmap)  (tbm_bo bo);
extern void (*sym_tbm_bo_unref) (tbm_bo bo);

extern tbm_bo (*sym_tbm_bo_import_fd) (tbm_bufmgr bufmgr, unsigned int fd);
extern tbm_bo (*sym_tbm_bo_alloc) (tbm_bufmgr bufmgr, int size, int flags);
extern int (*sym_tbm_bo_export_fd) (tbm_bo bo);
extern tbm_bo_handle (*sym_tbm_bo_get_handle) (tbm_bo bo, int device);

extern tbm_bufmgr (*sym_tbm_bufmgr_init) (int fd);
extern void (*sym_tbm_bufmgr_deinit) (tbm_bufmgr bufmgr);

////////////////////////////////////

void evas_software_xlib_x_init(void);
void evas_software_xcb_init(void);
void *evas_native_buffer_image_set(void *data, void *image, void *native);
void *evas_native_tbm_image_set(void *data, void *image, void *native);
int evas_native_tbm_stride_get(void *data, void *native);

#endif
