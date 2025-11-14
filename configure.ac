AC_PREREQ(2.52)

dnl please read gstreamer/docs/random/autotools before changing this file

dnl initialize autoconf
dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease
dnl releases only do -Wall, cvs and prerelease does -Werror too
AC_INIT(GStreamer Fluendo MP3 Decoder, 0.10.32,,
    gst-fluendo-mp3)

AC_CONFIG_MACRO_DIR([common/m4])

AG_GST_INIT

dnl initialize automake
AM_INIT_AUTOMAKE

dnl define PACKAGE_VERSION_* variables
AS_VERSION

dnl check if this is a release version
AS_NANO(GST_CVS="no", GST_CVS="yes")

dnl define the output header for config
AC_CONFIG_HEADER([config.h])

dnl AM_MAINTAINER_MODE only provides the option to configure to enable it
AM_MAINTAINER_MODE

dnl sets host_* variables
AC_CANONICAL_HOST

dnl make aclocal work in maintainer mode
AC_SUBST(ACLOCAL_AMFLAGS, "-I common/m4")

dnl *** check for arguments to configure ***

AG_GST_ARG_DEBUG
AG_GST_ARG_PROFILING
AG_GST_ARG_VALGRIND
AG_GST_ARG_GCOV
AG_GST_ARG_STATIC_PLUGIN

AG_GST_ARG_EXAMPLES

AG_GST_ARG_WITH_PKG_CONFIG_PATH
AG_GST_ARG_WITH_PACKAGE_NAME([Fluendo MP3 Plug-in])
AG_GST_ARG_WITH_PACKAGE_ORIGIN

dnl Header file checks
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h string.h fcntl.h dirent.h sys/ndir.h \
    ndir.h alloca.h locale.h )

dnl *** checks for platform ***

dnl * hardware/architecture *

dnl check CPU type
AG_GST_ARCH

dnl *** checks for programs ***

AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AM_PROG_AS

AC_PATH_PROG(VALGRIND_PATH, valgrind, no)
AM_CONDITIONAL(HAVE_VALGRIND, test ! "x$VALGRIND_PATH" = "xno")

dnl define an ERROR_CFLAGS Makefile variable
AG_GST_SET_ERROR_CFLAGS($GST_CVS)

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer

AG_GST_DETECT_VERSION([1.0.0], [0.10.3])
AG_GST_CHECK_GST($GST_MAJORMINOR, [$GST_REQ])
AG_GST_CHECK_GST_AUDIO($GST_MAJORMINOR, [$GST_REQ])

dnl define CPU_TUNE_CFLAGS and CPU_TUNE_CCASFLAGS
AG_GST_CPU_TUNE

AG_GST_SET_PLUGINDIR

if test "x$USE_PROFILING" = "xtrue"; then
  CFLAGS="$CFLAGS -pg"
  LDFLAGS="$LDFLAGS -pg"
fi

dnl Add unroll-loops to the default CFLAGS 
CFLAGS="$CFLAGS -funroll-loops"

AC_SUBST(CFLAGS)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_OUTPUT([
Makefile
src/Makefile
common/Makefile
common/m4/Makefile
util/Makefile
gst-fluendo-mp3.spec
win32/Makefile
win32/vs9/Makefile
win32/vs9/config.h
win32/oa/Makefile
win32/oa/config.h
win32/vs10/Makefile
win32/include/Makefile
win32/include/config-orig.h
])

echo -n "
$PACKAGE-$VERSION

        prefix:                         ${prefix}
        compiler:                       ${CC}
        Building for GStreamer-${GST_MAJORMINOR}

        Use Intel Performance Primitives: ${HAVE_IPP}
        IPP CFLAGS:  ${IPP_INCLUDES}
        IPP LDFLAGS: ${IPP_LIBS}

        CPU_TUNE_CFLAGS:  ${CPU_TUNE_CFLAGS}
        CPU_TUNE_LDFLAGS: ${CPU_TUNE_LDFLAGS}

"
