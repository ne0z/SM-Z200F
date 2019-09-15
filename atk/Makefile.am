# Process this file with automake to create Makefile.in.
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS=atk tests docs build

EXTRA_DIST = 			\
	MAINTAINERS		\
	atk-uninstalled.pc.in	\
	atk.pc.in		\
	atk.spec.in		\
	atk.spec		\
	atk-zip.sh.in   \
	config.h.win32

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = atk.pc

DISTCHECK_CONFIGURE_FLAGS =

DISTCLEANFILES =		\
	atk-uninstalled.pc	\
	atk.pc			\
	atk.pc.spec		\
	atk-zip.sh
