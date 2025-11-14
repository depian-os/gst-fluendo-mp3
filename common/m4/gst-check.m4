dnl pkg-config-based checks for GStreamer modules and dependency modules

dnl generic:
dnl AG_GST_PKG_CHECK_MODULES([PREFIX], [WHICH], [REQUIRED])
dnl sets HAVE_[$PREFIX], [$PREFIX]_*
dnl AG_GST_CHECK_MODULES([PREFIX], [MODULE], [MINVER], [NAME], [REQUIRED])
dnl sets HAVE_[$PREFIX], [$PREFIX]_*

dnl specific:
dnl AG_GST_CHECK_GST([MAJMIN], [MINVER], [REQUIRED])
dnl   also sets/ACSUBSTs GST_TOOLS_DIR and GST_PLUGINS_DIR
dnl AG_GST_CHECK_GST_BASE([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_GDP([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_CONTROLLER([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_CHECK([MAJMIN], [MINVER], [REQUIRED])
dnl AG_GST_CHECK_GST_PLUGINS_BASE([MAJMIN], [MINVER], [REQUIRED])
dnl   also sets/ACSUBSTs GSTPB_PLUGINS_DIR

AC_DEFUN([AG_GST_PKG_CHECK_MODULES],
[
  which="[$2]"
  dnl not required by default, since we use this mostly for plugin deps
  required=ifelse([$3], , "no", [$3])

  PKG_CHECK_MODULES([$1], $which,
    [
      HAVE_[$1]="yes"
    ],
    [
      HAVE_[$1]="no"
      AC_MSG_RESULT(no)
      if test "x$required" = "xyes"; then
        AC_MSG_ERROR($[$1]_PKG_ERRORS)
      else
        AC_MSG_NOTICE($[$1]_PKG_ERRORS)
      fi
    ])

  dnl AC_SUBST of CFLAGS and LIBS was not done before automake 1.7
  dnl It gets done automatically in automake >= 1.7, which we now require
])

AC_DEFUN([AG_GST_CHECK_MODULES],
[
  module=[$2]
  minver=[$3]
  name="[$4]"
  required=ifelse([$5], , "yes", [$5]) dnl required by default

  PKG_CHECK_MODULES([$1], $module >= $minver,
    [
      HAVE_[$1]="yes"
    ],
    [
      HAVE_[$1]="no"
      AC_MSG_RESULT(no)
      AC_MSG_NOTICE($[$1]_PKG_ERRORS)
      if test "x$required" = "xyes"; then
        AC_MSG_ERROR([no $module >= $minver ($name) found])
      else
        AC_MSG_NOTICE([no $module >= $minver ($name) found])
      fi
    ])

  dnl AC_SUBST of CFLAGS and LIBS was not done before automake 1.7
  dnl It gets done automatically in automake >= 1.7, which we now require
])

AC_DEFUN([AG_GST_DETECT_VERSION],
[
  AC_ARG_WITH(gstreamer-api,
     AC_HELP_STRING([--with-gstreamer-api],
         [manually set the gstreamer API version 0.10 or 1.0 are valid values]),
         [USE_GSTREAMER_API="${withval}"])

  case "$USE_GSTREAMER_API" in
    1.0)
      PKG_CHECK_MODULES(GST_VER_1_0, gstreamer-1.0 >= [$1],
        [GST_REQ=[$1]
         GST_MAJORMINOR=1.0],
         AC_MSG_ERROR([Could not find gstreamer 1.0 in the system])
      )
    ;;
    0.10)
      PKG_CHECK_MODULES(GST_VER_0_10, gstreamer-0.10 >= [$2],
        [GST_REQ=[$2]
         GST_MAJORMINOR=0.10],
         AC_MSG_ERROR([Could not find gstreamer 0.10 in the system])
      )
    ;;
    *)
      PKG_CHECK_MODULES(GST_VER_1_0, gstreamer-1.0 >= [$1],
        [GST_REQ=[$1]
         GST_MAJORMINOR=1.0],
        [PKG_CHECK_MODULES(GST_VER_0_10, gstreamer-0.10 >= [$2],
          [GST_REQ=[$2]
           GST_MAJORMINOR=0.10],
          AC_MSG_ERROR([Could not find a valid version of gstreamer in the system])
        )]
      )
    ;;
  esac

  AM_CONDITIONAL([GST_VER_1_0], [test "x$GST_MAJORMINOR" = "x1.0"])
  AM_CONDITIONAL([GST_VER_0_10], [test "x$GST_MAJORMINOR" = "x0.10"])
])

AC_DEFUN([AG_GST_CHECK_GST],
[
  AG_GST_CHECK_MODULES(GST, gstreamer-[$1], [$2], [GStreamer], [$3])
  dnl allow setting before calling this macro to override
  if test -z $GST_TOOLS_DIR; then
    GST_TOOLS_DIR=`$PKG_CONFIG --variable=toolsdir gstreamer-[$1]`
    if test -z $GST_TOOLS_DIR; then
      AC_MSG_ERROR(
        [no tools dir set in GStreamer pkg-config file, core upgrade needed.])
    fi
  fi
  AC_MSG_NOTICE([using GStreamer tools in $GST_TOOLS_DIR])
  AC_SUBST(GST_TOOLS_DIR)

  dnl check for where core plug-ins got installed
  dnl this is used for unit tests
  dnl allow setting before calling this macro to override
  if test -z $GST_PLUGINS_DIR; then
    GST_PLUGINS_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-[$1]`
    if test -z $GST_PLUGINS_DIR; then
      AC_MSG_ERROR(
        [no pluginsdir set in GStreamer pkg-config file, core upgrade needed.])
    fi
  fi
  AC_MSG_NOTICE([using GStreamer plug-ins in $GST_PLUGINS_DIR])
  AC_SUBST(GST_PLUGINS_DIR)

  if test "x$1" = "x0.10"; then
    dnl Check if gestreamer is 0.10.10 or newer, required to know for
    dnl printf segment extension.
    PKG_CHECK_MODULES(GST_10_10, gstreamer-0.10 >= 0.10.10,
      POST_10_10=1, POST_10_10=0)
    AC_DEFINE_UNQUOTED(POST_10_10, $POST_10_10,
      [GStreamer version is >= 0.10.10])

    PKG_CHECK_MODULES(GST_10_11, gstreamer-0.10 >= 0.10.11,
      POST_10_11=1, POST_10_11=0)
    AC_DEFINE_UNQUOTED(POST_10_11, $POST_10_11,	
      [GStreamer version is >= 0.10.11])

    PKG_CHECK_MODULES(GST_10_12, gstreamer-0.10 >= 0.10.12,
      POST_10_12=1, POST_10_12=0)
    AC_DEFINE_UNQUOTED(POST_10_12, $POST_10_12,	
      [GStreamer version is >= 0.10.12])

    PKG_CHECK_MODULES(GST_10_13, gstreamer-0.10 >= 0.10.13,
      POST_10_13=1, POST_10_13=0)
    AC_DEFINE_UNQUOTED(POST_10_13, $POST_10_13,	
      [GStreamer version is >= 0.10.13])

    PKG_CHECK_MODULES(GST_10_14, gstreamer-0.10 >= 0.10.14,
      POST_10_14=1, POST_10_14=0)
    AC_DEFINE_UNQUOTED(POST_10_14, $POST_10_14,	
      [GStreamer version is >= 0.10.14])

    PKG_CHECK_MODULES(GST_10_21, gstreamer-0.10 >= 0.10.21,
      POST_10_21=1, POST_10_21=0)
    AC_DEFINE_UNQUOTED(POST_10_21, $POST_10_21,
      [GStreamer version is >= 0.10.21])

    PKG_CHECK_MODULES(GST_10_25, gstreamer-0.10 >= 0.10.25,
      POST_10_25=1, POST_10_25=0)
    AC_DEFINE_UNQUOTED(POST_10_25, $POST_10_25,
      [GStreamer version is >= 0.10.25])

    PKG_CHECK_MODULES(GST_10_26, gstreamer-0.10 >= 0.10.26,
      POST_10_26=1, POST_10_26=0)
    AC_DEFINE_UNQUOTED(POST_10_26, $POST_10_26,
      [GStreamer version is >= 0.10.26])
  else
    PKG_CHECK_MODULES(GST_1_0, gstreamer-1.0 >= 1.0,
      POST_1_0_0=1, POST_1_0_0=0)
    AC_DEFINE_UNQUOTED(POST_1_0, $POST_1_0,
      [GStreamer version is >= 1.0.0])
  fi
])

AC_DEFUN([AG_GST_CHECK_GST_BASE],
[
  AG_GST_CHECK_MODULES(GST_BASE, gstreamer-base-[$1], [$2],
    [GStreamer Base Libraries], [$3])
])
  
AC_DEFUN([AG_GST_CHECK_GST_GDP],
[
  AG_GST_CHECK_MODULES(GST_GDP, gstreamer-dataprotocol-[$1], [$2],
    [GStreamer Data Protocol Library], [$3])
])

AC_DEFUN([AG_GST_CHECK_GST_CONTROLLER],
[
  AG_GST_CHECK_MODULES(GST_CONTROLLER, gstreamer-controller-[$1], [$2],
    [GStreamer Controller Library], [$3])
])  

AC_DEFUN([AG_GST_CHECK_GST_AUDIO],
[
  AG_GST_CHECK_MODULES(GST_AUDIO, gstreamer-audio-[$1], [$2],
    [GStreamer Audio Library], no)
  dnl package config files are only available since 0.10.16
  if test "x$HAVE_GST_AUDIO" = "xno"; then
    GST_AUDIO_CFLAGS=""
    GST_AUDIO_LIBS="-lgstaudio-$GST_MAJORMINOR"
    AC_SUBST(GST_AUDIO_CFLAGS)
    AC_SUBST(GST_AUDIO_LIBS)
  fi
])

AC_DEFUN([AG_GST_CHECK_GST_VIDEO],
[
  AG_GST_CHECK_MODULES(GST_VIDEO, gstreamer-video-[$1], [$2],
    [GStreamer Video Library], no)
  dnl package config files are only available since 0.10.16
  if test "x$HAVE_GST_VIDEO" = "xno"; then
    GST_VIDEO_CFLAGS=""
    GST_VIDEO_LIBS="-lgstvideo-$GST_MAJORMINOR"
    AC_SUBST(GST_VIDEO_CFLAGS)
    AC_SUBST(GST_VIDEO_LIBS)
  fi
])

AC_DEFUN([AG_GST_CHECK_GST_CHECK],
[
  AG_GST_CHECK_MODULES(GST_CHECK, gstreamer-check-[$1], [$2],
    [GStreamer Check unittest Library], [$3])
])

AC_DEFUN([AG_GST_CHECK_GST_PLUGINS_BASE],
[
  AG_GST_CHECK_MODULES(GST_PLUGINS_BASE, gstreamer-plugins-base-[$1], [$2],
    [GStreamer Base Plug-ins Library], [$3])

  dnl check for where base plug-ins got installed
  dnl this is used for unit tests
  dnl allow setting before calling this macro to override
  if test -z $GSTPB_PLUGINS_DIR; then
    GSTPB_PLUGINS_DIR=`$PKG_CONFIG --variable=pluginsdir gstreamer-plugins-base-[$1]`
    if test -z $GSTPB_PLUGINS_DIR; then
      GSTPB_PLUGINS_DIR=${libdir}/gstreamer-${GST_MAJORMINOR}
    fi
  fi
  AC_MSG_NOTICE([using GStreamer Base Plug-ins in $GSTPB_PLUGINS_DIR])
  AC_SUBST(GSTPB_PLUGINS_DIR)
])
