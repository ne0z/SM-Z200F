# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstvideoefx.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstvideoefx_la_SOURCES = gstvideoefx.c gstvideoefx.h videoefx_utility.h videoefx_utility.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstvideoefx_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(CAPI_IMAGE_FILTER_CFLAGS) $(MMCOMMON_CFLAGS)  $(TBM_CFLAGS) $(MM_UTIL_JPEG_CFLAGS) $(DRM_CFLAGS) $(OPENGLES20_CFLAGS) $(XFIXES_CFLAGS) $(DRI2_CFLAGS) $(XPROTO_CFLAGS) $(IMAGE_FILTER_CFLAGS)
libgstvideoefx_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(CAPI_IMAGE_FILTER_LIBS) $(MMCOMMON_LIBS) $(TBM_LIBS) $(MM_UTIL_JPEG_LIBS) $(DRM_LIBS) $(OPENGLES20_LIBS) $(XFIXES_LIBS) $(DRI2_LIBS) $(XPROTO_LIBS) $(IMAGE_FILTER_LIBS)
libgstvideoefx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
#libvideoefx_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstvideoefx.h videoefx_utility.c
