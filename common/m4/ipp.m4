dnl Detection of Intel Integrated Performance Primitives IPP
dnl Usage: AG_CHEC_IPP([FUNCTION LIST PATH])
dnl FUNCTION LIST PATH is an optional argument
AC_DEFUN([AG_CHECK_IPP],
[
  BUILD_IN_MACOS=false
  BUILD_IN_UNIX=false
  BUILD_IN_WINDOWS=false
  BUILD_IPP_MERGED=false
  case "$host_os" in
    *darwin*)
      BUILD_IN_MACOS=true
      ;;
    mingw*)
      BUILD_IN_WINDOWS=true
      ;;
    linux* | solaris*)
      BUILD_IN_UNIX=true
      BUILD_IPP_MERGED=true
      ;;
  esac
  AM_CONDITIONAL(BUILD_IN_MACOS, test "x$BUILD_IN_MACOS" = "xtrue")
  AM_CONDITIONAL(BUILD_IN_UNIX, test "x$BUILD_IN_UNIX" = "xtrue")
  AM_CONDITIONAL(BUILD_IN_WINDOWS, test "x$BUILD_IN_WINDOWS" = "xtrue")

  dnl Setup for finding IPP libraries. Attempt to detect by default.
  CHECK_FOR_IPP=true
  AC_MSG_CHECKING([for Intel Performance Primitives library])

  AC_ARG_WITH(ipp,
      AC_HELP_STRING([--with-ipp],
          [Turn on/off use of Intel Programming Primitives (default=yes)]),
          [if test "x$withval" = "xno"; then CHECK_FOR_IPP=false; fi])

  AC_ARG_WITH(ipp-path,
     AC_HELP_STRING([--with-ipp-path],
         [manually set location of IPP files]),
         [USE_IPP_PATH="${withval}"])

  AC_ARG_WITH(ipp-arch,
    AC_HELP_STRING([--with-ipp-arch],
         [to include only one ipp implementation/architecture, valid values are: 0=all,1=px,2=a6,3=w7,4=t7,5=v8,6=p8,7=mx,8=m7,9=u8,10=y8,11=s8(lp32)]),
    [IPP_ARCH="${withval}"],
    [IPP_ARCH="0"])

  AC_DEFINE_UNQUOTED(USE_SINGLE_IPP_ARCH, $IPP_ARCH, [Specify one IPP implementation])  

  if test -n "$USE_IPP_PATH"; then
    IPP_PREFIX="$USE_IPP_PATH"
    if test -f "$IPP_PREFIX/include/ipp.h"; then
      HAVE_IPP=true
      CHECK_FOR_IPP=false
    else
      IPP_AVAIL="."  
    fi
  else
    if test "x$BUILD_IN_MACOS" = "xtrue"; then
      IPP_PREFIX="/Developer/opt/intel/ipp/"
    fi
    if test "x$BUILD_IN_WINDOWS" = "xtrue"; then
      IPP_PREFIX="/c/Intel/IPP"
    fi
    if test "x$BUILD_IN_UNIX" = "xtrue"; then
      IPP_PREFIX="/opt/intel/ipp"
    fi
    dnl Assumes that the latest directory created is the one with the correct
    dnl version to use.
    IPP_AVAIL="`ls -vrd $IPP_PREFIX/* | sed 's|.*/||' 2>/dev/null`"
    HAVE_IPP=false
  fi

  dnl IPP is only valid for x86 based archs
  case "x${host_cpu}" in
    xi?86)
      if test "x$IPP_ARCH" = "x11" ; then
        IPP_CPU="lp32"
        IPP_SUFFIX=""
      else
        IPP_CPU="ia32"
        IPP_SUFFIX=""
      fi
    ;;
    xi?86_64 | xx86_64)
      IPP_CPU="em64t"
      IPP_SUFFIX="em64t"
    ;;
    *)
      CHECK_FOR_IPP=false
      HAVE_IPP=false
    ;;
  esac

  dnl For OSX there is no suffix nor cpu
  if test "x$BUILD_IN_MACOS" = "xtrue" ; then
    IPP_SUFFIX=""
    IPP_CPU=""
  fi

  if test "x$CHECK_FOR_IPP" = "xtrue"; then
    HAVE_IPP=false
    # Loop over IPP versions
    for ver in $IPP_AVAIL; do
      if test -f "${IPP_PREFIX}/${ver}/${IPP_CPU}/include/ipp.h"; then
        IPP_PREFIX="${IPP_PREFIX}/${ver}/${IPP_CPU}"
        HAVE_IPP=true
        break
      fi
    done
  fi

  AM_CONDITIONAL(USE_IPP, test "x$HAVE_IPP" = "xtrue")

  if test "x$HAVE_IPP" = "xtrue"; then
    AC_DEFINE(USE_IPP, TRUE, [Define whether IPP is available])
    if test -z $1; then
      IPP_FUNC_PATH="src"
    else
      IPP_FUNC_PATH=$1
    fi
    AC_SUBST(IPP_FUNC_PATH)

    IPP_PATH="${IPP_PREFIX}"
    IPP_INCLUDES="-I${IPP_PATH}/include"

    if test "x$BUILD_IPP_MERGED" = "xtrue" ; then
      AC_DEFINE(USE_IPP_MERGED, TRUE, [Define whether USE_IPP_MERGED could be used])
    fi
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no]) 
  fi

  dnl define missing __int64 type used in the ipp headers
  if test "x$BUILD_IN_WINDOWS" = "xtrue"; then
    AC_CHECK_TYPE([__int64],,
      [AC_DEFINE_UNQUOTED([__int64], [long long],
        [Define to `long long' if <sys/types.h> does not define.])])
  fi
  dnl ippmerged needs to know wheter to link or not when building a static or dynamic library
  AM_CONDITIONAL(BUILD_STATIC, test "x$enable_static" = "xyes" -a "x$enable_shared" = "xno")

])

dnl Given a list of ipp libraries to link with, set the needed variables for building
dnl Usage: AG_NEED_IPP([IPP LIBS], [FUNCTION LIST PATH])
dnl FUNCTION LIST PATH is an optional argument
AC_DEFUN([AG_NEED_IPP],
[
  HAVE_IPP=false
  AG_CHECK_IPP($2)
  if test "x$BUILD_IN_WINDOWS" = "xtrue" ; then
    AR_EXT=.lib
    AR_PRE=
  else
    AR_EXT=.a
    AR_PRE=lib
  fi
  if test "$HAVE_IPP" = "false"; then
    AC_MSG_WARN([Intel Performance Primitives not found in $IPP_PREFIX])
  else
    NEED_LIST=$1
    IPP_TRAMPOLINE_LIST=""
    IPP_LIST=""
    if test "x$BUILD_IN_MACOS" = "xtrue" ; then
      IPP_LIST+=${NEED_LIST}
      IPP_SUFFIX+="_l"
    else
      for lib in ${NEED_LIST}; do
        IPP_LIST+="${lib}merged "
        IPP_TRAMPOLINE_LIST+="${lib}emerged "
      done
    fi


    if test "x$BUILD_IN_WINDOWS" = "xtrue" ; then
      IPP_CORE="ippcorel"
    else
      IPP_CORE="ippcore"
    fi

    dnl put ippcore at the end, when linking the symbols are not resolved recursively
    IPP_LIST+=" ${IPP_CORE}"
    IPP_TRAMPOLINE_LIST+=" ${IPP_CORE}"

    IPP_LIBS=""
    IPP_ARCHIVES=""
    IPP_LIBDIR="${IPP_PREFIX}/lib"

    if test "x$THREAD_SAFE" = "xyes"; then
      IPP_SUFFIX+="_t"
    fi

    for lib in ${IPP_LIST}; do
      ARCHIVE="${AR_PRE}${lib}${IPP_SUFFIX}${AR_EXT}"
      IPP_LIBS+=" -l${lib}${IPP_SUFFIX}"
      IPP_ARCHIVES+=" ${ARCHIVE}"
    done
    for lib in ${IPP_TRAMPOLINE_LIST}; do
      ARCHIVE="${AR_PRE}${lib}${IPP_SUFFIX}${AR_EXT}"
      IPP_TRAMPOLINE_LIBS+=" -l${lib}${IPP_SUFFIX}"
      if test "x$BUILD_IN_WINDOWS" = "xtrue" ; then
        IPP_LIBS=" -l${lib}${IPP_SUFFIX} ${IPP_LIBS}"
        IPP_ARCHIVES+=" ${ARCHIVE}"
      fi
    done

    if test "x$THREAD_SAFE" = "xyes"; then 
      IPP_LIBS+=" -lirc -liomp5" 
    fi  

    IPP_LIBS+=" -L${IPP_LIBDIR}"
  fi

  dnl IPP libs are compiled with the buffer security check leading to
  dnl an unresolved symbol __security_check_cookie, which is provided
  dnl by bufferoverflowU.lib. This library can be found in the Windows
  dnl Driver Kit. Nevertheless we add msvcrt first to make sure we don't
  dnl start pulling tons of symbols provided by msvcrt from ntdll which could
  dnl seriously break our build like sscanf.
  if test "x$BUILD_IN_WINDOWS" = "xtrue" ; then
    IPP_LIBS+=" -lbufferoverflowU -lmsvcrt -lntdll"
  fi

  AC_SUBST(IPP_PATH)      dnl source directory
  AC_SUBST(IPP_INCLUDES)    dnl cflags
  AC_SUBST(IPP_LIBS)      dnl ldflags
  AC_SUBST(IPP_TRAMPOLINE_LIBS)      dnl ldflags
  AC_SUBST(IPP_ARCHIVES)  dnl to iterate
  AM_CONDITIONAL(USE_IPP, test "x$HAVE_IPP" = "xtrue")

  dnl Permit patching IPP library to avoid reallocation problems
  ipp_patch_realloc=false
  AC_ARG_WITH(ipp-patch-realloc,
    AC_HELP_STRING([--with-ipp-patch-realloc],
        [Turn on/off patching of IPP for reallocatable code (default=no)]),
        [if test "x$withval" = "xyes"; then ipp_patch_realloc=true; fi])
  dnl Disable patching on 64 bits
  if test "x$HAVE_CPU_X86_64" = "xyes" ; then
    ipp_patch_realloc=false
  fi  
  AM_CONDITIONAL(IPP_PATCH_REALLOC, ${ipp_patch_realloc})
])

