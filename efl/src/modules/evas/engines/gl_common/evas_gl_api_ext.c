
#include "evas_gl_api_ext.h"

#include <dlfcn.h>

#define EVGL_FUNC_BEGIN() if (UNLIKELY(_need_context_restore)) _context_restore()

#define MAX_EXTENSION_STRING_BUFFER 10240

// list of exts like "discard_framebuffer GL_EXT_discard_framebuffer multi_draw_arrays GL_EXT_multi_draw_arrays"
char _gl_ext_string[MAX_EXTENSION_STRING_BUFFER] = { 0 };
// list of exts by official name only like "GL_EXT_discard_framebuffer GL_EXT_multi_draw_arrays"
char _gl_ext_string_official[MAX_EXTENSION_STRING_BUFFER] = { 0 };
// list of gles 1.1 exts by official name
static char *_gles1_ext_string = NULL;
// list of gles 3.1 exts by official name
static char *_gles3_ext_string = NULL;

typedef void (*_getproc_fn) (void);
typedef _getproc_fn (*fp_getproc)(const char *);

#ifndef EGL_NATIVE_PIXMAP_KHR
# define EGL_NATIVE_PIXMAP_KHR 0x30b0
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Extension HEADER
/////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name)
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
   ret (*gl_ext_sym_##name) param1 = NULL; \
   ret (*gles1_ext_sym_##name) param1 = NULL; \
   ret (*gles3_ext_sym_##name) param1 = NULL;
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Extension HEADER
/////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   int _gl_ext_support_##name = 0; \
   int _gles1_ext_support_##name = 0; \
   int _gles3_ext_support_##name = 0;
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   int _gl_ext_support_func_##name = 0; \
   int _gles1_ext_support_func_##name = 0; \
   int _gles3_ext_support_func_##name = 0;
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
/////////////////////////////////////////////////////////////////////////////////////////////////////


// Evas extensions from EGL extensions
#ifdef GL_GLES
#define EGLDISPLAY_GET(a) _evgl_egl_display_get(__FUNCTION__, a)

// this struct defines an EvasGLImage when using EGL
typedef struct _EvasGLImage {
   EGLDisplay  dpy;
   EGLImageKHR img;
} EvasGLImage_EGL;

static EGLDisplay
_evgl_egl_display_get(const char *function, Evas_GL *evgl)
{
   EGLDisplay dpy = EGL_NO_DISPLAY;
   EVGL_Resource *rsc;

   if (!evgl_engine || !evgl_engine->funcs || !evgl_engine->funcs->display_get)
     {
        ERR("%s: Invalid Engine... (Can't acccess EGL Display)\n", function);
        evas_gl_common_error_set(NULL, EVAS_GL_BAD_DISPLAY);
        return EGL_NO_DISPLAY;
     }

   if (!(rsc=_evgl_tls_resource_get()))
     {
        if (evgl) goto fallback;
        ERR("%s: Unable to execute GL command. Error retrieving tls", function);
        evas_gl_common_error_set(NULL, EVAS_GL_NOT_INITIALIZED);
        return EGL_NO_DISPLAY;
     }

   if (!rsc->current_eng)
     {
        if (evgl) goto fallback;
        ERR("%s: no current engine set; ensure you've called evas_gl_make_current()", function);
        evas_gl_common_error_set(NULL, EVAS_GL_NOT_INITIALIZED);
        return EGL_NO_DISPLAY;
     }

   dpy = (EGLDisplay) evgl_engine->funcs->display_get(rsc->current_eng);
   return dpy;

fallback:
   dpy = (EGLDisplay) evgl_engine->funcs->display_get(_evgl_engine_data_get(evgl));
   return dpy;
}

static void *
_evgl_eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx,
                        int target, void* buffer, const int *attrib_list)
{
   EvasGLImage_EGL *img;
   int *attribs = NULL;
   void *eglimg;

   /* Convert 0 terminator into a EGL_NONE terminator */
   if (attrib_list)
     {
        int cnt = 0;
        int *a;

        for (a = (int *) attrib_list; (*a) && (*a != EGL_NONE); a += 2)
          {
             /* TODO: Verify supported attributes */
             cnt += 2;
          }

        attribs = alloca(sizeof(int) * (cnt + 1));
        for (a = attribs; (*attrib_list) && (*attrib_list != EGL_NONE);
             a += 2, attrib_list += 2)
          {
             a[0] = attrib_list[0];
             a[1] = attrib_list[1];
          }
        *a = EGL_NONE;
     }

   eglimg = EXT_FUNC(eglCreateImage)(dpy, ctx, target, buffer, attribs);
   if (!eglimg) return NULL;

   img = calloc(1, sizeof(EvasGLImage_EGL));
   img->dpy = dpy;
   img->img = eglimg;
   return img;
}

static void *
_evgl_evasglCreateImage(int target, void* buffer, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET(NULL);
   EGLContext ctx = EGL_NO_CONTEXT;

   if (!dpy)
     {
        WRN("No display found, use evasglCreateImageForContext instead.");
        return NULL;
     }

   /* EGL_NO_CONTEXT will always fail for TEXTURE_2D */
   if (target == EVAS_GL_TEXTURE_2D)
     {
        ctx = eglGetCurrentContext();
        DBG("Creating EGL image based on the current context: %p", ctx);
     }

   return _evgl_eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

static void *
_evgl_evasglCreateImageForContext(Evas_GL *evasgl, Evas_GL_Context *evasctx,
                                  int target, void* buffer, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evasgl);
   EGLContext ctx = EGL_NO_CONTEXT;

   if (!dpy || !evasgl)
     {
        ERR("Evas_GL can not be NULL here.");
        evas_gl_common_error_set(NULL, EVAS_GL_BAD_DISPLAY);
        return NULL;
     }

   ctx = _evgl_native_context_get(evasctx);
   return _evgl_eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

static void
_evgl_evasglDestroyImage(EvasGLImage image)
{
   EvasGLImage_EGL *img = image;

   EXT_FUNC(eglDestroyImage)(img->dpy, img->img);
   free(img);
}

static void
_evgl_glEvasGLImageTargetTexture2D(GLenum target, EvasGLImage image)
{
   EvasGLImage_EGL *img = image;

   EXT_FUNC(glEGLImageTargetTexture2DOES)(target, img->img);
}

static void
_evgl_glEvasGLImageTargetRenderbufferStorage(GLenum target, EvasGLImage image)
{
   EvasGLImage_EGL *img = image;

   EXT_FUNC(glEGLImageTargetRenderbufferStorageOES)(target, img->img);
}

static EvasGLSync
_evgl_evasglCreateSync(Evas_GL *evas_gl,
                       unsigned int type, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return NULL;
   return EXT_FUNC(eglCreateSyncKHR)(dpy, type, attrib_list);
}

static Eina_Bool
_evgl_evasglDestroySync(Evas_GL *evas_gl, EvasGLSync sync)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglDestroySyncKHR)(dpy, sync);
}

static int
_evgl_evasglClientWaitSync(Evas_GL *evas_gl,
                           EvasGLSync sync, int flags, EvasGLTime timeout)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglClientWaitSyncKHR)(dpy, sync, flags, timeout);
}

static Eina_Bool
_evgl_evasglSignalSync(Evas_GL *evas_gl,
                       EvasGLSync sync, unsigned mode)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglSignalSyncKHR)(dpy, sync, mode);
}

static Eina_Bool
_evgl_evasglGetSyncAttrib(Evas_GL *evas_gl,
                          EvasGLSync sync, int attribute, int *value)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglGetSyncAttribKHR)(dpy, sync, attribute, value);
}

static int
_evgl_evasglWaitSync(Evas_GL *evas_gl,
                     EvasGLSync sync, int flags)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglWaitSyncKHR)(dpy, sync, flags);
}

/*
static Eina_Bool
_evgl_evasglBindWaylandDisplay(Evas_GL *evas_gl,
                               void *wl_display)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglBindWaylandDisplayWL)(dpy, wl_display);
}

static Eina_Bool
_evgl_evasglUnbindWaylandDisplay(Evas_GL *evas_gl,
                                 void *wl_display)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglUnbindWaylandDisplayWL)(dpy, wl_display);
}

static Eina_Bool
_evgl_evasglQueryWaylandBuffer(Evas_GL *evas_gl,
                               void *buffer, int attribute, int *value)
{
   EGLDisplay dpy = EGLDISPLAY_GET(evas_gl);
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglQueryWaylandBufferWL)(dpy, buffer, attribute, value);
}
*/

#else
#endif

/* GL_EXT_discard_framebuffer (1.x, 2.0) */
static void
_evgl_glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum* attachments)
{
   EVGL_Resource *rsc;
   EVGL_Context *ctx;
   Eina_Bool target_is_fbo = EINA_FALSE;

   if (!(rsc=_evgl_tls_resource_get()))
     {
        ERR("Unable to execute GL command. Error retrieving tls");
        return;
     }

   if (!rsc->current_eng)
     {
        ERR("Unable to retrive Current Engine");
        return;
     }

   ctx = rsc->current_ctx;
   if (!ctx)
     {
        ERR("Unable to retrive Current Context");
        return;
     }

   if (_evgl_direct_enabled())
     {
        if (ctx->map_tex)
          target_is_fbo = EINA_TRUE;
     }
   else
     {
        if (ctx->current_fbo == 0)
          target_is_fbo = EINA_TRUE;
     }

   if (target_is_fbo && numAttachments)
     {
        GLenum *att;
        int i = 0;
        att = (GLenum *)calloc(1, numAttachments * sizeof(GLenum));
        if (!att)
          return;

        memcpy(att, attachments, numAttachments * sizeof(GLenum));
        while (i < numAttachments)
          {
             if (att[i] == GL_COLOR_EXT)
               att[i] = GL_COLOR_ATTACHMENT0;
             else if (att[i] == GL_DEPTH_EXT)
               att[i] = GL_DEPTH_ATTACHMENT;
             else if (att[i] == GL_STENCIL_EXT)
               att[i] = GL_STENCIL_ATTACHMENT;
             i++;
          }
        EXT_FUNC(glDiscardFramebuffer)(target, numAttachments, att);
        free(att);
     }
   else
     {
        EXT_FUNC(glDiscardFramebuffer)(target, numAttachments, attachments);
     }
}
//2.0 ext bodies
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name)
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
    static ret evgl_##name param1 { EVGL_FUNC_BEGIN(); return EXT_FUNC(name) param2; }
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

//1.1 ext bodies
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name)
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
    static ret evgl_gles1_##name param1 { EVGL_FUNC_BEGIN(); return EXT_FUNC_GLES1(name) param2; }
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

//  0: not initialized,
//  1: GLESv2 initialized,
//  3: GLESv1 and GLESv2 initialized,
//  5: GLESv3 and GLESv2 initialized,
//  7: GLESv3 + GLESv2  + GLESv1 all initialized.
static int _evgl_api_ext_status = 0;

Eina_Bool
evgl_api_ext_init(void *getproc, const char *glueexts)
{
   const char *glexts;
   fp_getproc gp = (fp_getproc)getproc;
   int _curext_supported = 0;

   memset(_gl_ext_string, 0, MAX_EXTENSION_STRING_BUFFER);
   memset(_gl_ext_string_official, 0, MAX_EXTENSION_STRING_BUFFER);

#ifndef GL_GLES
   /* Add some extension strings that are always working on desktop GL */
   static const char *desktop_exts =
         "GL_EXT_read_format_bgra "
         "GL_EXT_texture_format_BGRA8888 "
         "GL_EXT_texture_type_2_10_10_10_REV ";
   strncpy(_gl_ext_string, desktop_exts, MAX_EXTENSION_STRING_BUFFER);
   strncpy(_gl_ext_string_official, desktop_exts, MAX_EXTENSION_STRING_BUFFER);
#endif

   // GLES Extensions
   glexts = (const char*)glGetString(GL_EXTENSIONS);
   if (!glexts)
     {
        ERR("glGetString returned NULL! Something is very wrong...");
        return EINA_FALSE;
     }

   /*
   // GLUE Extensions
#ifdef GL_GLES
getproc = &eglGetProcAddress;
glueexts = eglQueryString(re->win->egl_disp, EGL_EXTENSIONS);
#else
getproc = &glXGetProcAddress;
glueexts = glXQueryExtensionsString(re->info->info.display,
re->info->info.screen);
#endif
    */

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define GETPROCADDR(sym) \
   (((!(*drvfunc)) && (gp)) ? (__typeof__((*drvfunc)))gp(sym) : (__typeof__((*drvfunc)))dlsym(RTLD_DEFAULT, sym))

#define _EVASGL_EXT_BEGIN(name) \
     { \
        int *ext_support = &_gl_ext_support_##name; \
        *ext_support = 0;

#define _EVASGL_EXT_END() \
     }

#define _EVASGL_EXT_CHECK_SUPPORT(name) \
   (strstr(glexts, name) != NULL || strstr(glueexts, name) != NULL)

#define _EVASGL_EXT_DISCARD_SUPPORT() \
   *ext_support = 0;

#define _EVASGL_EXT_DRVNAME(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) *ext_support = 1;

#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) { *ext_support = 1; _gl_ext_support_func_##name = 1; }

#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname) \
   if (_EVASGL_EXT_CHECK_SUPPORT(deskname)) *ext_support = 1;

#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
     { \
        ret (**drvfunc)param1 = &gl_ext_sym_##name;

#define _EVASGL_EXT_FUNCTION_END() \
        if ((*drvfunc) == NULL) _EVASGL_EXT_DISCARD_SUPPORT(); \
     }

#define _EVASGL_EXT_FUNCTION_DRVFUNC(name) \
   if ((*drvfunc) == NULL) *drvfunc = name;

// This adds all the function names to the "safe" list but only one pointer
// will be stored in the hash table.
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name) \
   if ((*drvfunc) == NULL) \
     { \
        *drvfunc = GETPROCADDR(name); \
        evgl_safe_extension_add(name, (void *) (*drvfunc)); \
     } \
   else evgl_safe_extension_add(name, NULL);

#ifdef _EVASGL_EXT_FUNCTION_WHITELIST
# undef _EVASGL_EXT_FUNCTION_WHITELIST
#endif
#define _EVASGL_EXT_FUNCTION_WHITELIST(name) evgl_safe_extension_add(name, NULL);

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_FUNCTION_WHITELIST
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

#undef GETPROCADDR

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   _gl_ext_string[0] = 0x00; //NULL;
   _gl_ext_string_official[0] = 0x00;
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_BEGIN(name) \
     if (_gl_ext_support_##name != 0) \
       { \
          strncat(_gl_ext_string, #name" ", MAX_EXTENSION_STRING_BUFFER); \
          _curext_supported = 1; \
       } \
     else _curext_supported = 0;

#define _EVASGL_EXT_END()
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_DRVNAME_PRINT(name) \
       { \
          strncat(_gl_ext_string, name" ", MAX_EXTENSION_STRING_BUFFER); \
          if ((strncmp(name, "GL", 2) == 0) && (strstr(_gl_ext_string_official, name) == NULL)) \
            strncat(_gl_ext_string_official, name" ", MAX_EXTENSION_STRING_BUFFER); \
       }
#define _EVASGL_EXT_DRVNAME(name) \
     if (_curext_supported) \
       _EVASGL_EXT_DRVNAME_PRINT(#name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
     if (_curext_supported && _gl_ext_support_func_##name) \
       _EVASGL_EXT_DRVNAME_PRINT(#name)

#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME_PRINT
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////

   _gl_ext_string[MAX_EXTENSION_STRING_BUFFER - 1] = '\0';
   _gl_ext_string_official[MAX_EXTENSION_STRING_BUFFER - 1] = '\0';

  _evgl_api_ext_status = 1;
   return EINA_TRUE;
}

void
evgl_api_ext_get(Evas_GL_API *gl_funcs)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return;
     }

#define ORD(f) EVAS_API_OVERRIDE(f, gl_funcs, evgl_)

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   if (_gl_ext_support_##name != 0) \
     {
#define _EVASGL_EXT_END() \
     }
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
   ORD(name);
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#undef _EVASGL_EXT_WHITELIST_ONLY
#define _EVASGL_EXT_WHITELIST_ONLY 0

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_WHITELIST_ONLY
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ORD

}

Eina_Bool
_evgl_api_gles1_ext_init(void)
{
   // Return if GLESv1 ext is already intiialised
   if (_evgl_api_ext_status & 0x2)
     return EINA_TRUE;

#ifdef GL_GLES
   int _curext_supported = 0;
   Evas_GL_API *gles1_funcs;
   const char *gles1_exts, *eglexts;
   EVGL_Resource *rsc;
   EGLint context_version;
   EGLDisplay dpy = EGLDISPLAY_GET(NULL);

   /* glGetString returns the information for the currently bound context
    * So, update gles1_exts only if GLES1 context is currently bound.
    * Check here if GLESv1 is current
    */
   if (!(rsc=_evgl_tls_resource_get()))
     {
        ERR("Unable to initialize GLES1 extensions. Error retrieving tls");
        return EINA_FALSE;
     }

   if ((dpy == EGL_NO_DISPLAY) || !rsc->current_ctx)
     {
        DBG("Unable to initialize GLES1 extensions. Engine not initialised");
        return EINA_FALSE;
     }

   if (!eglQueryContext(dpy, rsc->current_ctx->context, EGL_CONTEXT_CLIENT_VERSION, &context_version))
     {
        ERR("Unable to initialize GLES1 extensions. eglQueryContext failed 0x%x", eglGetError());
        return EINA_FALSE;
     }

   if (context_version != EVAS_GL_GLES_1_X)
     {
        DBG("GLESv1 context not bound");
        return EINA_FALSE;
     }

   gles1_funcs = _evgl_api_gles1_internal_get();
   if (!gles1_funcs || !gles1_funcs->glGetString)
     {
        ERR("Could not get address of glGetString in GLESv1 library!");
        return EINA_FALSE;
     }

   gles1_exts = (const char *) gles1_funcs->glGetString(GL_EXTENSIONS);
   if (!gles1_exts)
     {
        ERR("GLESv1:glGetString(GL_EXTENSIONS) returned NULL!");
        return EINA_FALSE;
     }

   eglexts = eglQueryString(dpy, EGL_EXTENSIONS);
   if (!eglexts)
     {
        ERR("eglQueryString(EGL_EXTENSIONS) returned NULL!");
        eglexts = "";
     }

   if (!_gles1_ext_string)
     {
        _gles1_ext_string = calloc(MAX_EXTENSION_STRING_BUFFER, 1);
        if (!_gles1_ext_string) return EINA_FALSE;
     }

   _gles1_ext_string[0] = '\0';

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning supported extensions, sets the variables
   /////////////////////////////////////////////////////////////////////////////////////////////////////

   // Preparing all the magic macros
#define GETPROCADDR(sym) \
   ((__typeof__((*drvfunc))) (eglGetProcAddress(sym)))

#define _EVASGL_EXT_BEGIN(name) \
   { \
      int *ext_support = &_gles1_ext_support_##name; \
      *ext_support = 0;

#define _EVASGL_EXT_END() \
   }

#define _EVASGL_EXT_CHECK_SUPPORT(name) \
   ((strstr(gles1_exts, name) != NULL) || (strstr(eglexts, name) != NULL))

#define _EVASGL_EXT_DISCARD_SUPPORT() \
   *ext_support = 0;

#define _EVASGL_EXT_DRVNAME(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) *ext_support = 1;

#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) { *ext_support = 1; _gles1_ext_support_func_##name = 1; }

#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname) \
   if (_EVASGL_EXT_CHECK_SUPPORT(deskname)) *ext_support = 1;

#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
     { \
        ret (**drvfunc)param1 = &gles1_ext_sym_##name;

#define _EVASGL_EXT_FUNCTION_END() \
        if ((*drvfunc) == NULL) _EVASGL_EXT_DISCARD_SUPPORT(); \
     }

#define _EVASGL_EXT_FUNCTION_DRVFUNC(name) \
   if ((*drvfunc) == NULL) *drvfunc = name;

#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name) \
   if ((*drvfunc) == NULL) \
     { \
        *drvfunc = GETPROCADDR(name); \
        evgl_safe_extension_add(name, (void *) (*drvfunc)); \
     } \
   else evgl_safe_extension_add(name, NULL);

#ifdef _EVASGL_EXT_FUNCTION_WHITELIST
# undef _EVASGL_EXT_FUNCTION_WHITELIST
#endif
#define _EVASGL_EXT_FUNCTION_WHITELIST(name) evgl_safe_extension_add(name, NULL);

#define _EVASGL_EXT_GLES1_ONLY 1

   // Okay, now we are ready to scan.
#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_GLES1_ONLY
#undef _EVASGL_EXT_FUNCTION_WHITELIST
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
#undef GETPROCADDR

#define _EVASGL_EXT_BEGIN(name) \
     _curext_supported = (_gles1_ext_support_##name != 0);


   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning again to add to the gles1 ext string list
   /////////////////////////////////////////////////////////////////////////////////////////////////////

#define _EVASGL_EXT_END()
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_DRVNAME_PRINT(name) \
     { \
        if ((strncmp(name, "GL", 2) == 0) && (strstr(_gles1_ext_string, name) == NULL)) \
          strcat(_gles1_ext_string, name" "); \
     }
#define _EVASGL_EXT_DRVNAME(name) \
   if (_curext_supported) \
      _EVASGL_EXT_DRVNAME_PRINT(#name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   if (_curext_supported && _gles1_ext_support_func_##name) \
      _EVASGL_EXT_DRVNAME_PRINT(#name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME_PRINT
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

   if (evgl_engine->api_debug_mode)
     DBG("GLES1: List of supported extensions:\n%s", _gles1_ext_string);

   // GLESv1 version has been initialized!
   _evgl_api_ext_status |= 0x2;
   return EINA_TRUE;
#else
   ERR("GLESv1 support is not implemented for GLX");
   return EINA_FALSE;
#endif
}

void
evgl_api_gles1_ext_get(Evas_GL_API *gl_funcs)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return;
     }

   if (!(_evgl_api_ext_status & 0x2))
     {
        DBG("Initializing GLESv1 extensions...");
        if (!_evgl_api_gles1_ext_init())
          {
             ERR("GLESv1 extensions initialization failed");
             return;
          }
     }

#define ORD(f) EVAS_API_OVERRIDE(f, gl_funcs, evgl_gles1_)

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   if (_gles1_ext_support_##name != 0) \
     {
#define _EVASGL_EXT_END() \
     }
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
   ORD(name);
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#undef _EVASGL_EXT_WHITELIST_ONLY
#define _EVASGL_EXT_WHITELIST_ONLY 0

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ORD

}

Eina_Bool
_evgl_api_gles3_ext_init(void)
{
   if (_evgl_api_ext_status & 0x4)
     return EINA_TRUE;

   int _curext_supported = 0;
   Evas_GL_API *gles3_funcs;
   const char *gles3_exts;
#ifdef GL_GLES
   EVGL_Resource *rsc;
   EGLint context_version;
   EGLDisplay dpy = EGLDISPLAY_GET(NULL);

   /* glGetString returns the information for the currently bound context
    * So, update gles3_exts only if GLES3 context is currently bound.
    * Check here if GLESv3 is current
    */
   if (!(rsc=_evgl_tls_resource_get()))
     {
        ERR("Unable to initialize GLES3 extensions. Error retrieving tls");
        return EINA_FALSE;
     }

   if ((dpy == EGL_NO_DISPLAY) || !rsc->current_ctx)
     {
        DBG("Unable to initialize GLES3 extensions. Engine not initialised");
        return EINA_FALSE;
     }

   if (!eglQueryContext(dpy, rsc->current_ctx->context, EGL_CONTEXT_CLIENT_VERSION, &context_version))
     {
        ERR("Unable to initialize GLES3 extensions. eglQueryContext failed 0x%x", eglGetError());
        return EINA_FALSE;
     }

   if (context_version != EVAS_GL_GLES_3_X)
     {
        DBG("GLESv3 context not bound");
        return EINA_FALSE;
     }
#endif

   gles3_funcs = _evgl_api_gles3_internal_get();
   if (!gles3_funcs || !gles3_funcs->glGetString)
     {
        ERR("Could not get address of glGetString in GLESv3 library!");
        return EINA_FALSE;
     }

   gles3_exts = (const char *) gles3_funcs->glGetString(GL_EXTENSIONS);
   if (!gles3_exts)
     {
        ERR("GLESv3:glGetString(GL_EXTENSIONS) returned NULL!");
        return EINA_FALSE;
     }

   if (!_gles3_ext_string)
     {
        _gles3_ext_string = calloc(MAX_EXTENSION_STRING_BUFFER, 1);
        if (!_gles3_ext_string) return EINA_FALSE;
     }

   _gles3_ext_string[0] = '\0';

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning supported extensions, sets the variables
   /////////////////////////////////////////////////////////////////////////////////////////////////////

   // Preparing all the magic macros
#define GETPROCADDR(sym) \
   ((__typeof__((*drvfunc))) (eglGetProcAddress(sym)))

#define _EVASGL_EXT_BEGIN(name) \
   { \
      int *ext_support = &_gles3_ext_support_##name; \
      *ext_support = 0;

#define _EVASGL_EXT_END() \
   }

#define _EVASGL_EXT_CHECK_SUPPORT(name) \
   (strstr(gles3_exts, name) != NULL)

#define _EVASGL_EXT_DISCARD_SUPPORT() \
   *ext_support = 0;

#define _EVASGL_EXT_DRVNAME(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) *ext_support = 1;

#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) { *ext_support = 1; _gles3_ext_support_func_##name = 1; }
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname) \
   if (_EVASGL_EXT_CHECK_SUPPORT(deskname)) *ext_support = 1;

#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
     { \
        ret (**drvfunc)param1 = &gles3_ext_sym_##name; \
        if (*ext_support == 1) \
          {

#define _EVASGL_EXT_FUNCTION_END() \
          } \
        if ((*drvfunc) == NULL) _EVASGL_EXT_DISCARD_SUPPORT(); \
     }
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name) \
   if ((*drvfunc) == NULL) *drvfunc = name;

#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name) \
   if ((*drvfunc) == NULL) \
     { \
        *drvfunc = GETPROCADDR(name); \
        evgl_safe_extension_add(name, (void *) (*drvfunc)); \
     } \
   else evgl_safe_extension_add(name, NULL);

#ifdef _EVASGL_EXT_FUNCTION_WHITELIST
# undef _EVASGL_EXT_FUNCTION_WHITELIST
#endif
#define _EVASGL_EXT_FUNCTION_WHITELIST(name) evgl_safe_extension_add(name, NULL);

   // Okay, now we are ready to scan.
#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_FUNCTION_WHITELIST
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
#undef GETPROCADDR

#define _EVASGL_EXT_BEGIN(name) \
     _curext_supported = (_gles3_ext_support_##name != 0);


   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning again to add to the gles3 ext string list
   /////////////////////////////////////////////////////////////////////////////////////////////////////

#define _EVASGL_EXT_END()
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_DRVNAME_PRINT(name) \
     { \
        if ((strncmp(name, "GL", 2) == 0) && (strstr(_gles3_ext_string, name) == NULL)) \
          strcat(_gles3_ext_string, name" "); \
     }
#define _EVASGL_EXT_DRVNAME(name) \
   if (_curext_supported) \
      _EVASGL_EXT_DRVNAME_PRINT(#name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name) \
   if (_curext_supported && _gles3_ext_support_func_##name) \
      _EVASGL_EXT_DRVNAME_PRINT(#name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME_PRINT
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

   if (evgl_engine->api_debug_mode)
     DBG("GLES3: List of supported extensions:\n%s", _gles3_ext_string);

   // GLESv3 version has been initialized!
   _evgl_api_ext_status |= 0x4;
   return EINA_TRUE;
}

void
evgl_api_gles3_ext_get(Evas_GL_API *gl_funcs)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return;
     }

   if (!(_evgl_api_ext_status & 0x4))
     {
        DBG("Initializing GLESv3 extensions...");
        if (!_evgl_api_gles3_ext_init())
          {
             ERR("GLESv3 extensions initialization failed");
             return;
          }
     }

#define ORD(f) EVAS_API_OVERRIDE(f, gl_funcs, gles3_ext_sym_)

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   if (_gles3_ext_support_##name != 0) \
     {
#define _EVASGL_EXT_END() \
     }
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_DRVNAME_PRIVATE(name)
#define _EVASGL_EXT_DRVNAME_DESKTOP(deskname)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param1, param2) \
   ORD(name);
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN()
#define _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param1, param2)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#undef _EVASGL_EXT_WHITELIST_ONLY
#define _EVASGL_EXT_WHITELIST_ONLY 0

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_DRVNAME_PRIVATE
#undef _EVASGL_EXT_DRVNAME_DESKTOP
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_BEGIN
#undef _EVASGL_EXT_FUNCTION_DISABLE_FOR_GLES1_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ORD

}



const char *
evgl_api_ext_string_get(Eina_Bool official, int version)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return NULL;
     }

   if (version == EVAS_GL_GLES_1_X)
     return _gles1_ext_string;

   if (version == EVAS_GL_GLES_3_X)
     return _gles3_ext_string;

   if (official)
     return _gl_ext_string_official;

   return _gl_ext_string;
}
