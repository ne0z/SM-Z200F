## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
-I. \
-I$(top_builddir) \
-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
@MULTISENSE_PLUGIN_CFLAGS@

libdir = $(PACKAGE_PLUGIN_DIR)

REMIXDIR = $(libdir)/../
REMIX_LIBS = $(REMIXDIR)/libremix.la

lib_LTLIBRARIES = libtizen_sound_player.la

libtizen_sound_player_la_SOURCES = tizen_sound_player.c
libtizen_sound_player_la_LDFLAGS = -module -avoid-version
libtizen_sound_player_la_LIBADD = $(MULTISENSE_PLUGIN_LIBS) $(REMIX_LIBS)
libtizen_sound_player_la_LIBTOOLFLAGS = --tag=disable-static
