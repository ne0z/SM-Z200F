# plugindir is set in configure

##############################################################################
# library libgspscaler.la for GSP scaler plugin                              #
##############################################################################
plugin_LTLIBRARIES = libgspscaler.la

# sources used to compile this plug-in
libgspscaler_la_SOURCES = gspscaler.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgspscaler_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)$(X_CFLAGS) $(DRM_CFLAGS) $(XFIXES_CFLAGS) $(DRI2PROTO_CFLAGS) $(DRI2_CFLAGS) $(TBM_CFLAGS) $(XDAMAGE_CFLAGS) $(GSL_CFLAGS) $(MMCOMMON_CFLAGS)
libgspscaler_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) $(X_LIBS) $(XVIDEO_LIBS) $(DRM_LIBS) $(XFIXES_LIBS) $(DRI2PROTO_LIBS) $(DRI2_LIBS) $(TBM_LIBS) $(XDAMAGE_LIBS) -ldrminterface

libgspscaler_la_LDFLAGS = -L./libs

# headers we need but don't want installed
noinst_HEADERS = gspscaler.h

install-exec-hook:
	mkdir -p $(DESTDIR)$(prefix)/lib && \
	cp -P $(srcdir)/libs/* $(DESTDIR)$(prefix)/lib/
