dnl This macros is intended to define platform specific tuned CFLAGS/CCASFLAGS
AC_DEFUN([AG_GST_CPU_TUNE],
[
  CPU_TUNE_CFLAGS=""
  CPU_TUNE_CCASFLAGS=""
  CPU_TUNE_LDFLAGS=""

  dnl tune build for Atom
  AC_ARG_ENABLE(cpu-tune-atom,
    AC_HELP_STRING([--enable-cpu-tune-atom], 
      [enable CFLAGS/CCASFLAGS tuned for Intel Atom]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
    
  if test "x$TUNE" = xyes; then
    AC_MSG_NOTICE(Build will be tuned for Intel Atom)
    AS_COMPILER_FLAG(-O3, 
      CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -O3")
    AS_COMPILER_FLAG(-march=atom, [march_atom=yes], [march_atom=no])
    if test "x$march_atom" = xyes; then
      CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -march=atom"
    else    
      AS_COMPILER_FLAG(-msse3, 
        CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -msse3")
      AS_COMPILER_FLAG(-march=nocona, 
        CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -march=nocona")
    fi
    AS_COMPILER_FLAG(-mfpmath=sse, 
      CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -mfpmath=sse")
  fi
  
  dnl tune build for Nokia N800
  AC_ARG_ENABLE(cpu-tune-n800,
    AC_HELP_STRING([--enable-cpu-tune-n800], 
      [enable CFLAGS/CCASFLAGS tuned for Nokia N800]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AC_MSG_NOTICE(Build will be tuned for Nokia N800)
    AS_COMPILER_FLAG(-march=armv6j, 
      NEW_FLAGS="$NEW_FLAGS -march=armv6j")
    AS_COMPILER_FLAG(-mtune=arm1136j-s, 
      NEW_FLAGS="$NEW_FLAGS -mtune=arm1136j-s")
    dnl Some assembly code requires -fomit-frame-pointer
    AS_COMPILER_FLAG(-fomit-frame-pointer, 
      NEW_FLAGS="$NEW_FLAGS -fomit-frame-pointer")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARMV6_SIMD, TRUE, [Build with ARM v6 optimizations])
  fi

  dnl tune build for Raspberry PI
  AC_ARG_ENABLE(cpu-tune-rpi,
    AC_HELP_STRING([--enable-cpu-tune-rpi], 
      [enable CFLAGS/CCASFLAGS tuned for Raspberry PI]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AC_MSG_NOTICE(Build will be tuned for Raspberry PI)
    AS_COMPILER_FLAG(-march=armv6j, 
      NEW_FLAGS="$NEW_FLAGS -march=armv6j")
    AS_COMPILER_FLAG(-mtune=arm1176jzf-s, 
      NEW_FLAGS="$NEW_FLAGS -mtune=arm1176jzf-s")

    dnl Some assembly code requires -fomit-frame-pointer
    AS_COMPILER_FLAG(-fomit-frame-pointer, 
      NEW_FLAGS="$NEW_FLAGS -fomit-frame-pointer")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARMV6_SIMD, TRUE, [Build with ARM v6 optimizations])
  fi

  dnl tune build for Marvell Dove
  AC_ARG_ENABLE(cpu-tune-dove,
    AC_HELP_STRING([--enable-cpu-tune-dove], 
      [enable CFLAGS/CCASFLAGS tuned for Marvell Dove]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value

  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AC_MSG_NOTICE(Build will be tuned for Marvell Dove)

    AS_COMPILER_FLAG(-march=armv7-a,
      NEW_FLAGS="$NEW_FLAGS -march=armv7-a")

    AS_COMPILER_FLAG(-mtune=marvell-pj4, [mtune_pj4=yes], [mtune_pj4=no])
    if test "x$mtune_pj4" = xyes; then
      dnl FIXME: we should use -mcpu=marvell-pj4 too instead of -march 
      dnl but doesn't work in Ubuntu gcc 4.8 from ppa:ubuntu-toolchain-r
      NEW_FLAGS="$NEW_FLAGS -mtune=marvell-pj4"
    else
      AS_COMPILER_FLAG(-mtune=cortex-a9,
        NEW_FLAGS="$NEW_FLAGS -mtune=cortex-a9")
    fi

    AS_COMPILER_FLAG(-mthumb, 
      NEW_FLAGS="$NEW_FLAGS -mthumb")
    AS_COMPILER_FLAG(-mfloat-abi=softfp, 
      NEW_FLAGS="$NEW_FLAGS -mfloat-abi=softfp")
    AS_COMPILER_FLAG(-mfpu=vfpv3-d16, 
      NEW_FLAGS="$NEW_FLAGS -mfpu=vfpv3-d16")
    dnl Some assembly code requires -fomit-frame-pointer
    AS_COMPILER_FLAG(-fomit-frame-pointer, 
      NEW_FLAGS="$NEW_FLAGS -fomit-frame-pointer")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARMV6_SIMD, TRUE, [Build with ARM v6 optimizations])
  fi

  dnl Build the ARM v6 code
  AC_ARG_ENABLE(armv6-code,
    AC_HELP_STRING([--enable-armv6-code], 
      [enable conditional code for armv6]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    AC_MSG_NOTICE(Build with ARM v6 assembly optimized code)
    AC_DEFINE(USE_ARMV6_SIMD, TRUE, [Build with ARM v6 optimizations])
  fi

  dnl tune build for ARM Cortex A8 cpus
  AC_ARG_ENABLE(cpu-tune-cortex-a8,
    AC_HELP_STRING([--enable-cpu-tune-cortex-a8], 
      [enable CFLAGS/CCASFLAGS tuned for ARM Cortex A8]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AC_MSG_NOTICE(Build will be tuned for ARM Cortex A8)
    AS_COMPILER_FLAG(-mcpu=cortex-a8, [mcpu_a8=yes], [mcpu_a8=no])
    if test "x$mcpu_a8" = xyes; then
      NEW_FLAGS="$NEW_FLAGS -mcpu=cortex-a8"
    else    
      AS_COMPILER_FLAG(-march=armv7-a, 
        NEW_FLAGS="$NEW_FLAGS -march=armv7-a")
      AS_COMPILER_FLAG(-mtune=cortex-a8, 
        NEW_FLAGS="$NEW_FLAGS -mtune=cortex-a8")
    fi

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
  fi

  dnl tune build for ARM Cortex A9 cpus
  AC_ARG_ENABLE(cpu-tune-cortex-a9,
    AC_HELP_STRING([--enable-cpu-tune-cortex-a9], 
      [enable CFLAGS/CCASFLAGS tuned for ARM Cortex A9]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AC_MSG_NOTICE(Build will be tuned for ARM Cortex A9)
    AS_COMPILER_FLAG(-mcpu=cortex-a9, [mcpu_a9=yes], [mcpu_a9=no])
    if test "x$mcpu_a9" = xyes; then
      NEW_FLAGS="$NEW_FLAGS -mcpu=cortex-a9"
    else    
      AS_COMPILER_FLAG(-march=armv7-a, 
        NEW_FLAGS="$NEW_FLAGS -march=armv7-a")
      AS_COMPILER_FLAG(-mtune=cortex-a9, 
        NEW_FLAGS="$NEW_FLAGS -mtune=cortex-a9")
    fi

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
  fi
  
  dnl tune build using vfp+softfp
  AC_ARG_ENABLE(cpu-tune-vfp,
    AC_HELP_STRING([--enable-cpu-tune-vfp], 
      [enable build of ARM vfp optimizations]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AS_COMPILER_FLAG(-marm, 
      NEW_FLAGS="$NEW_FLAGS -marm")
    AS_COMPILER_FLAG(-mfloat-abi=softfp, 
      NEW_FLAGS="$NEW_FLAGS -mfloat-abi=softfp")
    AS_COMPILER_FLAG(-mfpu=vfp, 
      NEW_FLAGS="$NEW_FLAGS -mfpu=vfp")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARM_VFP, TRUE, [Build with ARM vfp optimizations])
  fi
  AM_CONDITIONAL(USE_ARM_VFP, test "x$TUNE" = "xyes")

  dnl tune build using vfp+hardfp
  AC_ARG_ENABLE(cpu-tune-vfp-hf,
    AC_HELP_STRING([--enable-cpu-tune-vfp-hf], 
      [enable build of ARM vfp optimizations]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AS_COMPILER_FLAG(-marm, 
      NEW_FLAGS="$NEW_FLAGS -marm")
    AS_COMPILER_FLAG(-mfloat-abi=hard, 
      NEW_FLAGS="$NEW_FLAGS -mfloat-abi=hard")
    AS_COMPILER_FLAG(-mfpu=vfp, 
      NEW_FLAGS="$NEW_FLAGS -mfpu=vfp")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARM_VFP, TRUE, [Build with ARM vfp optimizations])
  fi
  AM_CONDITIONAL(USE_ARM_VFP, test "x$TUNE" = "xyes")

  dnl tune build using neon+softfp
  AC_ARG_ENABLE(cpu-tune-neon,
    AC_HELP_STRING([--enable-cpu-tune-neon], 
      [enable build of with ARM neon optimizations]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AS_COMPILER_FLAG(-marm, 
      NEW_FLAGS="$NEW_FLAGS -marm")
    AS_COMPILER_FLAG(-mfloat-abi=softfp, 
      NEW_FLAGS="$NEW_FLAGS -mfloat-abi=softfp")
    AS_COMPILER_FLAG(-mfpu=neon, 
      NEW_FLAGS="$NEW_FLAGS -mfpu=neon")
    AS_COMPILER_FLAG(-ffast-math, 
      NEW_FLAGS="$NEW_FLAGS -ffast-math")
    AS_COMPILER_FLAG(-fsingle-precision-constant, 
      NEW_FLAGS="$NEW_FLAGS -fsingle-precision-constant")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARM_NEON, TRUE, [Build with ARM neon optimizations])
  fi

  dnl tune build using neon+hardfp
  AC_ARG_ENABLE(cpu-tune-neon-hf,
    AC_HELP_STRING([--enable-cpu-tune-neon-hf], 
      [enable build of with ARM neon optimizations]),
    [TUNE=yes],
    [TUNE=no]) dnl Default value
     
  if test "x$TUNE" = xyes; then
    NEW_FLAGS=""
    AS_COMPILER_FLAG(-marm, 
      NEW_FLAGS="$NEW_FLAGS -marm")
    AS_COMPILER_FLAG(-mfloat-abi=hard, 
      NEW_FLAGS="$NEW_FLAGS -mfloat-abi=hard")
    AS_COMPILER_FLAG(-mfpu=neon, 
      NEW_FLAGS="$NEW_FLAGS -mfpu=neon")
    AS_COMPILER_FLAG(-ffast-math, 
      NEW_FLAGS="$NEW_FLAGS -ffast-math")
    AS_COMPILER_FLAG(-fsingle-precision-constant, 
      NEW_FLAGS="$NEW_FLAGS -fsingle-precision-constant")

    CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS $NEW_FLAGS"
    CPU_TUNE_CCASFLAGS="$CPU_TUNE_CCASFLAGS $NEW_FLAGS"
    AC_DEFINE(USE_ARM_NEON, TRUE, [Build with ARM neon optimizations])
  fi

  AM_CONDITIONAL(USE_ARM_NEON, test "x$TUNE" = "xyes")

  dnl tune build on Solaris with Sun Forte CC
  AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])
  if test "x$SUNCC" == "xyes"; then
    AS_COMPILER_FLAG([-xO5],
      CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -xO5")
    AS_COMPILER_FLAG([-xspace],
      CPU_TUNE_CFLAGS="$CPU_TUNE_CFLAGS -xspace")
  fi

  dnl No execstack depends on the platform
  case "$host" in
    *darwin*)
      ;;
    *mingw*)
      ;;
    *-sun-* | *pc-solaris* )
      AC_CHECK_FILE([/usr/lib/ld/map.noexstk],
        [CPU_TUNE_LDFLAGS="${CPU_TUNE_LDFLAGS} -Wl,-M/usr/lib/ld/map.noexstk -static-libgcc"])
      ;;
    *)
      dnl FIXME: this is a test for compiler flags but we are testing a linker flag
      AS_COMPILER_FLAG([-Wl,-znoexecstack], 
        [CPU_TUNE_LDFLAGS="$CPU_TUNE_LDFLAGS -Wl,-znoexecstack"])
      ;;
  esac

  dnl libtool requires -no-udefined to build a DLL in windows
  case "$host" in
    *mingw*)
      CPU_TUNE_LDFLAGS="$CPU_TUNE_LDFLAGS -no-undefined"
      ;;
  esac

  AC_SUBST(CPU_TUNE_CFLAGS)
  AC_SUBST(CPU_TUNE_CCASFLAGS)
  AC_SUBST(CPU_TUNE_LDFLAGS)

  if test "x$CPU_TUNE_CFLAGS" != "x"; then  
    AC_MSG_NOTICE(CPU_TUNE_CFLAGS   : $CPU_TUNE_CFLAGS)
    AC_MSG_NOTICE(CPU_TUNE_CCASFLAGS: $CPU_TUNE_CCASFLAGS)
    AC_MSG_NOTICE(CPU_TUNE_LDFLAGS  : $CPU_TUNE_LDFLAGS)
  fi  
])

