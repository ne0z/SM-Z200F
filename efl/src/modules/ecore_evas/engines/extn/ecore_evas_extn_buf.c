#include "ecore_evas_extn_engine.h"

////////////////////////////////////
// drm stuff (libdrm.so.2)
static void *drm_lib = NULL;
typedef unsigned int drm_magic_t;
static int (*sym_drmGetMagic)(int fd, drm_magic_t *magic) = NULL;

////////////////////////////////////
// dri2 stuff (libdri2.so.0)
static void *dri_lib = NULL;
#define DRI2BufferFrontLeft 0

typedef struct
{
   unsigned int attachment;
   unsigned int name;
   unsigned int pitch;
   unsigned int cpp;
   unsigned int flags;
} DRI2Buffer;

static Eina_Bool (*sym_DRI2QueryExtension)(Display *display, int *eventBase, int *errorBase) = NULL;
static Eina_Bool (*sym_DRI2QueryVersion)(Display *display, int *major, int *minor) = NULL;
static Eina_Bool (*sym_DRI2Connect)(Display *display, XID window, char **driverName, char **deviceName) = NULL;
static Eina_Bool (*sym_DRI2Authenticate)(Display *display, XID window, drm_magic_t magic) = NULL;
static void (*sym_DRI2CreateDrawable) (Display *display, XID drawable) = NULL;
static void (*sym_DRI2DestroyDrawable) (Display *display, XID handle) = NULL;
static DRI2Buffer *(*sym_DRI2GetBuffers) (Display *display, XID drawable, int *width, int *height, unsigned int *attachments, int count, int *outCount) = NULL;

////////////////////////////////////
// tbm stuff (libtbm.so.1)
static void *tbm_lib = NULL;
typedef struct _tbm_bufmgr *tbm_bufmgr;
typedef struct _tbm_bo *tbm_bo;
typedef union _tbm_bo_handle
{
   void     *ptr;
   int32_t  s32;
   uint32_t u32;
   int64_t  s64;
   uint64_t u64;
} tbm_bo_handle;

/* TBM_DEVICE_TYPE */
#define TBM_DEVICE_DEFAULT   0  /**< device type to get the default handle    */
#define TBM_DEVICE_CPU       1  /**< device type to get the virtual memory    */
#define TBM_DEVICE_2D        2  /**< device type to get the 2D memory handle  */
#define TBM_DEVICE_3D        3  /**< device type to get the 3D memory handle  */
#define TBM_DEVICE_MM        4  /**< device type to get the multimedia handle */

/* TBM_OPTION */
#define TBM_OPTION_READ      (1 << 0) /**< access option to read  */
#define TBM_OPTION_WRITE     (1 << 1) /**< access option to write */
#define TBM_OPTION_VENDOR    (0xffff0000) /**< vendor specific option: it depends on the backend */

static tbm_bufmgr (*sym_tbm_bufmgr_init) (int fd) = NULL;
static void (*sym_tbm_bufmgr_deinit) (tbm_bufmgr bufmgr) = NULL;
static tbm_bo_handle (*sym_tbm_bo_map) (tbm_bo bo, int device, int opt) = NULL;
static int (*sym_tbm_bo_unmap) (tbm_bo bo) = NULL;
static tbm_bo_handle (*sym_tbm_bo_get_handle) (tbm_bo bo, int device) = NULL;
static tbm_bo (*sym_tbm_bo_import) (tbm_bufmgr bufmgr, unsigned int key) = NULL;
static void (*sym_tbm_bo_unref) (tbm_bo bo) = NULL;

typedef struct _Drm_Pixmap
{
   int fd;
   tbm_bufmgr bufmgr;
   tbm_bo bo;
} Drm_Pixmap;

static Drm_Pixmap *_dri_drm_init(Ecore_X_Display *display, Ecore_X_Window window);
static void _dri_drm_shutdown(Drm_Pixmap *dp);
static void _extnbuf_data_get_from_pixmap(Extnbuf *b);

////////////////////////////////////

struct _Extnbuf
{
   const char *file, *lock;
   void *addr;
   int fd, lockfd;
   int w, h, stride, size;
   Ecore_X_Pixmap pixmap;
   Drm_Pixmap *dp;
   Eina_Bool have_lock : 1;
   Eina_Bool am_owner : 1;
   int type : 4;
};

// "owner" creates/frees the bufs, clients just open existing ones
Extnbuf *
_extnbuf_new(const char *base, int id, Eina_Bool sys, int num,
             int w, int h, Eina_Bool owner, int type)
{
   Extnbuf *b;
   char file[PATH_MAX];
   mode_t mode = S_IRUSR;
   int prot = PROT_READ;
   int page_size;

   page_size = eina_cpu_page_size();

   b = calloc(1, sizeof(Extnbuf));
   b->fd = -1;
   b->lockfd = -1;
   b->addr = MAP_FAILED;
   b->w = w;
   b->h = h;
   b->stride = w * 4;
   b->size = page_size * (((b->stride * b->h) + (page_size - 1)) / page_size);
   b->pixmap = 0;
   b->dp = NULL;
   b->am_owner = owner;
   b->type = type;

   if (b->am_owner)
     {
        const char *s = NULL;

        // Tizen only: smack issue. It should be written in labeled folder.
        s = "/run/.ecore";
        snprintf(file, sizeof(file), "%s/ee-lock-XXXXXX", s);
        b->lockfd = mkstemp(file);
        if (b->lockfd < 0) goto err;
        b->lock = eina_stringshare_add(file);
        if (!b->lock) goto err;
     }

   if (b->type == BUFFER_TYPE_SHM)
     {
        snprintf(file, sizeof(file), "/%s-%i.%i", base, id, num);
        b->file = eina_stringshare_add(file);
        if (!b->file) goto err;

        if (sys) mode |= S_IRGRP | S_IROTH;

        if (owner)
          {
             mode |= S_IWUSR;
             prot |= PROT_WRITE;
          }

        if (b->am_owner)
          {
             //TIZEN ONLY (150702): support indicator_shm in efl-extension
             //b->fd = shm_open(b->file, O_RDWR | O_CREAT | O_EXCL, mode);
             b->fd = shm_open(b->file, O_RDWR | O_CREAT, mode);
             if (b->fd < 0) goto err;
             if (ftruncate(b->fd, b->size) < 0) goto err;
          }
        else
          {
             b->fd = shm_open(b->file, O_RDONLY, mode);
             if (b->fd < 0) goto err;
          }
        b->addr = mmap(NULL, b->size, prot, MAP_SHARED, b->fd, 0);
        if (b->addr == MAP_FAILED) goto err;
     }
   else if (b->type == BUFFER_TYPE_DRI2_PIXMAP)
     {
        if (b->am_owner)
          {
             unsigned int foreground = 0;
             Ecore_X_GC gc;
             b->pixmap = ecore_x_pixmap_new(0, b->w, b->h, 32);
             gc = ecore_x_gc_new(b->pixmap, ECORE_X_GC_VALUE_MASK_FOREGROUND, &foreground);
             ecore_x_drawable_rectangle_fill(b->pixmap, gc, 0, 0, b->w, b->h);
             ecore_x_gc_free(gc);
             _extnbuf_data_get_from_pixmap(b);
          }
     }
   else if (b->type == BUFFER_TYPE_GL_PIXMAP)
     {
        b->addr = NULL;
     }
   return b;
err:
   _extnbuf_free(b);
   return NULL;
}

void
_extnbuf_free(Extnbuf *b)
{
   if (b->have_lock) _extnbuf_unlock(b);

   if (b->am_owner)
     {
        if (b->file) shm_unlink(b->file);
        if (b->lock) unlink(b->lock);
        if (b->dp) _dri_drm_shutdown(b->dp);
        if (b->pixmap && b->type == BUFFER_TYPE_DRI2_PIXMAP) ecore_x_pixmap_free(b->pixmap);
     }

   if (b->addr != MAP_FAILED) munmap(b->addr, b->size);
   if (b->fd >= 0) close(b->fd);
   if (b->lockfd >= 0) close(b->lockfd);
   eina_stringshare_del(b->file);
   eina_stringshare_del(b->lock);
   b->file = NULL;
   b->lock = NULL;
   b->addr = MAP_FAILED;
   b->fd = 1;
   b->lockfd = 1;
   b->am_owner = EINA_FALSE;
   b->have_lock = EINA_FALSE;
   b->w = 0;
   b->h = 0;
   b->stride = 0;
   b->size = 0;
   b->pixmap = 0;
   b->dp = NULL;
   b->type = -1;
   free(b);
}

// both ends can lock or unlock a buf
void *
_extnbuf_data_get(Extnbuf *b, int *w, int *h, int *stride)
{
   if (w) *w = b->w;
   if (h) *h = b->h;
   if (stride) *stride = b->stride;
   return b->addr;
}

void *
_extnbuf_lock(Extnbuf *b, int *w, int *h, int *stride)
{
   struct flock filelock;
   if (!b->have_lock)
     {
        if (b->lockfd >= 0)
          {
             filelock.l_type = b->am_owner ? F_WRLCK : F_RDLCK;
             filelock.l_whence = SEEK_SET;
             filelock.l_start = 0;
             filelock.l_len = 0;
             if (fcntl(b->lockfd, F_SETLKW, &filelock) == -1)
               {
                  ERR("lock take fail");
                  return NULL;
               }
          }
        b->have_lock = EINA_TRUE;
     }
   if (b->am_owner && b->dp && b->dp->bo)
     {
        // lock on pixmap(tbm) since the cpu will write into the buffer on the socket side
        sym_tbm_bo_map(b->dp->bo, TBM_DEVICE_CPU, TBM_OPTION_READ|TBM_OPTION_WRITE);
     }
   return _extnbuf_data_get(b, w, h, stride);
}

void
_extnbuf_unlock(Extnbuf *b)
{
   struct flock filelock;
   if (!b->have_lock) return;
   if (b->lockfd >= 0)
     {
        filelock.l_type = F_UNLCK;
        filelock.l_whence = SEEK_SET;
        filelock.l_start = 0;
        filelock.l_len = 0;
        if (fcntl(b->lockfd, F_SETLKW, &filelock) == -1)
          {
             ERR("lock release fail");
             return;
          }
     }
   if (b->am_owner && b->dp && b->dp->bo)
     {
        sym_tbm_bo_unmap(b->dp->bo);
     }
   b->have_lock = EINA_FALSE;
}

const char *
_extnbuf_lock_file_get(const Extnbuf *b)
{
   return b->lock;
}

Eina_Bool
_extnbuf_lock_file_set(Extnbuf *b, const char *file)
{
   if (b->am_owner) return EINA_FALSE;
   if (b->lock) eina_stringshare_del(b->lock);
   if (b->lockfd >= 0) close(b->lockfd);
   b->lockfd = -1;
   if (!file)
     {
        b->lock = NULL;
        b->lockfd = -1;
        return EINA_TRUE;
     }
   b->lock = eina_stringshare_add(file);
   if (!b->lock) goto err;
   b->lockfd = open(b->lock, O_RDWR);
   if (b->lockfd >= 0) return EINA_TRUE;
err:
   if (b->lock) eina_stringshare_del(b->lock);
   if (b->lockfd >= 0) close(b->lockfd);
   b->lockfd = -1;
   b->lock = NULL;
   return EINA_FALSE;
}

Eina_Bool
_extnbuf_lock_get(const Extnbuf *b)
{
   return b->have_lock;
}

Ecore_X_Pixmap
_extnbuf_pixmap_get(const Extnbuf *b)
{
   return b->pixmap;
}

void
_extnbuf_pixmap_set(Extnbuf *b, Ecore_X_Pixmap pixmap)
{
   b->pixmap = pixmap;
}

////////////////////////////////////
static int
_tbm_link(void)
{
   const char *tbm_libs[] =
   {
      "libtbm.so.1",
      "libtbm.so.0",
      NULL,
   };
   int i, fail;
#define SYM(lib, xx)                            \
  do {                                          \
       sym_ ## xx = dlsym(lib, #xx);            \
       if (!(sym_ ## xx)) {                     \
            ERR("%s", dlerror());               \
            fail = 1;                           \
         }                                      \
    } while (0)

   if (tbm_lib) return 1;
   for (i = 0; tbm_libs[i]; i++)
     {
        tbm_lib = dlopen(tbm_libs[i], RTLD_LOCAL | RTLD_LAZY);
        if (tbm_lib)
          {
             fail = 0;
             SYM(tbm_lib, tbm_bufmgr_init);
             SYM(tbm_lib, tbm_bufmgr_deinit);
             SYM(tbm_lib, tbm_bo_map);
             SYM(tbm_lib, tbm_bo_unmap);
             SYM(tbm_lib, tbm_bo_get_handle);
             SYM(tbm_lib, tbm_bo_import);
             SYM(tbm_lib, tbm_bo_unref);
             if (fail)
               {
                  dlclose(tbm_lib);
                  tbm_lib = NULL;
               }
             else break;
          }
     }
   if (!tbm_lib) return 0;
   return 1;
#undef SYM
}

static void
_tbm_unlink(void)
{
   if (tbm_lib)
     {
        dlclose(tbm_lib);
        tbm_lib = NULL;
     }
}

static int
_dri_drm_link(void)
{
   const char *drm_libs[] =
   {
      "libdrm.so.2",
      "libdrm.so.1",
      "libdrm.so.0",
      "libdrm.so",
      NULL,
   };
   const char *dri_libs[] =
   {
      "libdri2.so.2",
      "libdri2.so.1",
      "libdri2.so.0",
      "libdri2.so",
      NULL,
   };
   int i, fail;
#define SYM(lib, xx)                            \
   do {                                         \
       sym_ ## xx = dlsym(lib, #xx);            \
       if (!(sym_ ## xx)) {                     \
            ERR("%s", dlerror());               \
            fail = 1;                           \
         }                                      \
    } while (0)

   if (dri_lib) return 1;
   for (i = 0; drm_libs[i]; i++)
     {
        drm_lib = dlopen(drm_libs[i], RTLD_LOCAL | RTLD_LAZY);
        if (drm_lib)
          {
             fail = 0;
             SYM(drm_lib, drmGetMagic);
             if (fail)
               {
                  dlclose(drm_lib);
                  drm_lib = NULL;
               }
             else break;
          }
     }
   if (!drm_lib) return 0;
   for (i = 0; dri_libs[i]; i++)
     {
        dri_lib = dlopen(dri_libs[i], RTLD_LOCAL | RTLD_LAZY);
        if (dri_lib)
          {
             fail = 0;
             SYM(dri_lib, DRI2QueryExtension);
             SYM(dri_lib, DRI2QueryVersion);
             SYM(dri_lib, DRI2Connect);
             SYM(dri_lib, DRI2Authenticate);
             SYM(dri_lib, DRI2CreateDrawable);
             SYM(dri_lib, DRI2DestroyDrawable);
             SYM(dri_lib, DRI2GetBuffers);
             if (fail)
               {
                  dlclose(dri_lib);
                  dri_lib = NULL;
               }
             else break;
          }
     }
   if (!dri_lib)
     {
        dlclose(drm_lib);
        drm_lib = NULL;
        return 0;
     }

   return 1;
#undef SYM
}

static void
_dri_drm_unlink(void)
{
   if (dri_lib)
     {
        dlclose(dri_lib);
        dri_lib = NULL;
     }

   if (drm_lib)
     {
        dlclose(drm_lib);
        drm_lib = NULL;
     }
}

static Drm_Pixmap *
_dri_drm_init(Ecore_X_Display *display, Ecore_X_Window window)
{
   int dri2_ev_base, dri2_err_base, dri2_major, dri2_minor;
   char *drv_name, *dev_name;
   drm_magic_t magic;
   Drm_Pixmap *dp = NULL;

   if (!display) return NULL;
   if (!window) return NULL;
   if (!_dri_drm_link()) goto error;
   if (!_tbm_link()) goto error;

   dp = (Drm_Pixmap *)calloc(1, sizeof(Drm_Pixmap));
   if (!dp) goto error;

   if (!sym_DRI2QueryExtension(display, &dri2_ev_base, &dri2_err_base))
     {
        ERR("Failed to get dri2 extension");
        goto error;
     }

   if (!sym_DRI2QueryVersion(display, &dri2_major, &dri2_minor))
     {
        ERR("Failed to get dri2 version");
        goto error;
     }

   if (!sym_DRI2Connect(display, window, &drv_name, &dev_name))
     {
        ERR("Failed to get dri2 version");
        goto error;
     }

   dp->fd = open(dev_name, O_RDWR);
   if (dp->fd < 0)
     {
        ERR("Cannot open fd");
        free(drv_name);
        free(dev_name);
        goto error;
     }

   free(drv_name);
   free(dev_name);

   if (sym_drmGetMagic(dp->fd, &magic))
     {
        ERR("Cannot get magic in drmGetMagic");
        goto error;
     }

   if (!sym_DRI2Authenticate(display, window, (unsigned int)magic))
     {
        ERR("DRI2 authentication failed");
        goto error;
     }

   dp->bufmgr = sym_tbm_bufmgr_init(dp->fd);
   if (!dp->bufmgr)
     {
        ERR("tbm_bufmgr init failed");
        goto error;
     }

   return dp;
error:
   _dri_drm_shutdown(dp);
   return NULL;
}

static void
_dri_drm_shutdown(Drm_Pixmap *dp)
{
   if (dp)
     {
        if (dp->bo) sym_tbm_bo_unref(dp->bo);
        if (dp->bufmgr) sym_tbm_bufmgr_deinit(dp->bufmgr);
        if (dp->fd) close(dp->fd);
        if (dp) free(dp);
     }

   _tbm_unlink();
   _dri_drm_unlink();
}

static void
_extnbuf_data_get_from_pixmap(Extnbuf *b)
{
   DRI2Buffer *buf = NULL;
   unsigned int attach[1] = { DRI2BufferFrontLeft };
   Ecore_X_Display *display = ecore_x_display_get();
   Ecore_X_Drawable drawable;
   int num, width, height;
   tbm_bo_handle handle;
   handle.ptr = NULL;

   if (!b) return;
   if (!display) return;

   if (!b->dp)
     b->dp = _dri_drm_init(ecore_x_display_get(), DefaultRootWindow(ecore_x_display_get()));
   if (!b->dp) return;

   XSync(display, 0);

   drawable = (Ecore_X_Drawable)b->pixmap;
   sym_DRI2CreateDrawable(display, drawable);
   buf = sym_DRI2GetBuffers(display, drawable, &width, &height, attach, 1, &num);
   if (!buf) goto error;
   if (!buf->name) goto error;

   if (!b->dp->bufmgr) goto error;
   b->dp->bo = sym_tbm_bo_import(b->dp->bufmgr, buf->name);
   if (!b->dp->bo) goto error;

   b->stride = buf->pitch;
   handle = sym_tbm_bo_get_handle(b->dp->bo, TBM_DEVICE_CPU);
error:
   if (buf) XFree(buf);
   sym_DRI2DestroyDrawable(display, drawable);
   b->addr = handle.ptr;
}

