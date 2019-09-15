##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_define([v_maj], [1])
m4_define([v_min], [0])
m4_define([v_mic], [0])
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], [m4_define([v_ver], [v_maj.v_min.v_mic.v_rev])], [m4_define([v_ver], [v_maj.v_min.v_mic])])
m4_define([lt_rev], m4_eval(v_maj + v_min))
m4_define([lt_cur], v_mic)
m4_define([lt_age], v_min)
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##

AC_INIT([edje-multisense-plugin], [v_ver])
AC_PREREQ([2.52])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
AM_CONFIG_HEADER([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_LIBTOOL_WIN32_DLL
define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
AC_PROG_LIBTOOL

##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
m4_ifdef([v_rev], , [m4_define([v_rev], [0])])
m4_ifdef([v_rel], , [m4_define([v_rel], [])])
AC_DEFINE_UNQUOTED(VMAJ, [v_maj], [Major version])
AC_DEFINE_UNQUOTED(VMIN, [v_min], [Minor version])
AC_DEFINE_UNQUOTED(VMIC, [v_mic], [Micro version])
AC_DEFINE_UNQUOTED(VREV, [v_rev], [Revison])
version_info="lt_rev:lt_cur:lt_age"
release_info="v_rel"
AC_SUBST(version_info)
AC_SUBST(release_info)
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##--##
VMAJ=v_maj
AC_SUBST(VMAJ)

AC_DEFINE_UNQUOTED([SHARED_LIB_SUFFIX], ["$shrext_cmds"], [Suffix for shared objects])

EFL_CHECK_PATH_MAX

### Default options with respect to host

case "$host_os" in
   mingw32ce* | cegcc*)
      MODULE_ARCH="$host_os-$host_cpu"
      install_vim="no"
      have_edje_cc="no"
      ;;
   *)
      MODULE_ARCH="$host_os-$host_cpu-v_maj.v_min.v_mic"
      install_vim="yes"
      have_edje_cc="yes"
      ;;
esac

requirement_edje_multisense_plugin=""

AC_SUBST(MODULE_ARCH)
AC_DEFINE_UNQUOTED(MODULE_ARCH, "${MODULE_ARCH}", "Module architecture")

### Checks for programs
AC_PROG_CC

# pkg-config
PKG_PROG_PKG_CONFIG

# Check whether pkg-config supports Requires.private

if $PKG_CONFIG --atleast-pkgconfig-version 0.22; then
   pkgconfig_requires_private="Requires.private"
else
   pkgconfig_requires_private="Requires"
fi
AC_SUBST(pkgconfig_requires_private)

AC_ARG_ENABLE(gcc-werror,
   AC_HELP_STRING([--enable-gcc-werror], [enable -Werror in all Makefiles]))

dnl Use -Wall if we have gcc.

if test "x$ac_cv_prog_gcc" = xyes ; then
  CFLAGS="$CFLAGS -Wall"

  if test x$enable_gcc_werror = "xyes" ; then
    CFLAGS="$CFLAGS -Werror"
   fi
fi

# Dependencies for the library
PKG_CHECK_MODULES([MINIMAL],
   [
    eina >= 1.6.99
    eet >= 1.6.99
   ])

PKG_CHECK_MODULES([MULTISENSE_PLUGIN],
  [
    ecore >= 1.7.99
    mm-sound >= 1.0 
    remix >= 0.2.4
  ])

requirement_edje_multisense_plugin="ecore >= 1.7.99 mm-sound >= 1.0 ${requirement_edje_multisense_plugin}"

AC_DEFINE(EAPI, __attribute__ ((visibility("default"))), "Public API Visibility")

##libremix library
PKG_CHECK_MODULES(LIBREMIX, remix >= 0.2.4,
                  HAVE_LIB_REMIX="yes", HAVE_LIB_REMIX="no")

if test "$HAVE_LIB_REMIX" = "yes" ; then
  AC_DEFINE([HAVE_LIB_REMIX], [], [Define if libremix version found])
  requirement_edje_multisense_plugin="remix >= 0.2.4 ${requirement_edje_multisense_plugin}"
  PACKAGE_PLUGIN_DIR=`pkg-config --variable=libdir remix`"/remix"
  AC_SUBST(PACKAGE_PLUGIN_DIR)
  AC_DEFINE_UNQUOTED(PACKAGE_PLUGIN_DIR, "$PACKAGE_PLUGIN_DIR", [Set the plugin directory for remix])
  AC_DEFINE(__REMIX_PLUGIN__, 1, "Set to REMIX Plugin type")
else
  PACKAGE_PLUGIN_DIR=""
  fi
AM_CONDITIONAL(HAVE_LIB_REMIX, test "x${HAVE_LIB_REMIX}" = xyes)

MULTISENSE_PLUGIN_CFLAGS="${MULTISENSE_PLUGIN_CFLAGS}"
MULTISENSE_PLUGIN_LIBS="${MULTISENSE_PLUGIN_LIBS}"

### Checks for header files
AC_CHECK_HEADERS([locale.h])

### Checks for compiler characteristics
AM_PROG_CC_C_O
AC_C_CONST
AC_PROG_CC_STDC
AC_HEADER_STDC

case "$host_os" in
   mingw32ce*)
      MULTISENSE_PLUGIN_CFLAGS="${MULTISENSE_PLUGIN_CFLAGS} -D_WIN32_WCE=0x0420"
      ;;
   cegcc*)
      MULTISENSE_PLUGIN_CFLAGS="${MULTISENSE_PLUGIN_CFLAGS} -mwin32 -D_WIN32_WCE=0x0420"
      ;;
esac


### Checks for linker characteristics
lt_enable_auto_import=""
case "$host_os" in
   mingw* | cegcc*)
      lt_enable_auto_import="-Wl,--enable-auto-import"
      ;;
esac
AC_SUBST(lt_enable_auto_import)


### Checks for library functions
AC_FUNC_ALLOCA

case "$host_os" in
   mingw* | cegcc*)
      AC_DEFINE([HAVE_REALPATH], [1], [Define to 1 if you have the `realpath' function.])
      ;;
   *)
      AC_CHECK_FUNCS([realpath])
      ;;
esac

AC_SUBST(requirement_edje_multisense_plugin)
AC_OUTPUT([
Makefile
src/Makefile
src/tizen_sound_player/Makefile
])

#####################################################################
## Info

echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE $VERSION"
echo "------------------------------------------------------------------------"
echo
echo "Configuration Options Summary:"
echo
echo "Compilation............: make (or gmake)"
echo "  CPPFLAGS.............: $CPPFLAGS"
echo "  CFLAGS...............: $CFLAGS"
echo "  LDFLAGS..............: $LDFLAGS"
echo
echo "Installation...........: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...............: $prefix"
echo
