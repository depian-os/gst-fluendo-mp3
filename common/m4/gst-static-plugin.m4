dnl Disable pic build and 

AC_DEFUN([AG_GST_ARG_STATIC_PLUGIN],
[
  dnl build static plugins or not
  AC_MSG_CHECKING([whether to build static plugins or not])
  AC_ARG_ENABLE(
    static-plugin,
    AC_HELP_STRING(
      [--enable-static-plugin],
      [build static plugin @<:@default=no@:>@]),,
      [enable_static_plugin=no])
  AC_MSG_RESULT([$enable_static_plugin])
  if test "x$enable_static_plugin" = xyes; then
    AC_ENABLE_STATIC(yes) dnl --enable-static
    AC_LIBTOOL_PICMODE(default) dnl --without-pic
    AC_DEFINE(GST_PLUGIN_BUILD_STATIC, 1,
      [Define if static plugins should be built])
  fi
  AM_CONDITIONAL(GST_PLUGIN_BUILD_STATIC, test "x$enable_static_plugin" = "xyes")
])

AC_DEFUN([AG_GST_ARG_STATIC_PLUGINS],
[
  dnl to avoid break in some revisions
  AG_GST_ARG_STATIC_PLUGIN
])
