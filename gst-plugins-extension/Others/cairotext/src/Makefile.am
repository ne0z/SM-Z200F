# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstcairotext.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstcairotext_la_SOURCES = gstcairotext.c gstcairotext.h

# compiler and linker flags used to compile this myfilter, set in configure.ac
libgstcairotext_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(CAIRO_CFLAGS)
libgstcairotext_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)  $(GST_BASE_LIBS) $(CAIRO_LIBS)
libgstcairotext_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcairotext_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcairotext.h
