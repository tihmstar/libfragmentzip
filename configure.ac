AC_PREREQ([2.69])
AC_INIT([libfragmentzip], m4_esyscmd([git rev-list --count HEAD | tr -d '\n']), [tihmstar@gmail.com])

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST
AM_PROG_LIBTOOL

AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_DEFINE([VERSION_COMMIT_COUNT], "m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])", [Git commit count])
AC_DEFINE([VERSION_COMMIT_SHA], "m4_esyscmd([git rev-parse HEAD | tr -d '\n'])", [Git commit sha])
AC_SUBST([VERSION_COMMIT_COUNT], ["m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_COMMIT_SHA], ["m4_esyscmd([git rev-parse HEAD | tr -d '\n'])"])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

CFLAGS+=" -std=c11"

# Checks the platform.
AC_MSG_CHECKING([whether to enable WIN32 specific flags])
case "$host_os" in
*mingw*|*msys*)
    win32=true
    AC_MSG_RESULT([yes])
    AC_CHECK_TOOL(WINDRES, windres)
  ;;
*)
    win32=false
    AC_MSG_RESULT([no])
  ;;
esac
AM_CONDITIONAL(WIN32, test x$win32 = xtrue)

AC_ARG_ENABLE([debug],
            [AS_HELP_STRING([--enable-debug],
            [enable debug build(default is no)])],
            [debug_build=true],
            [debug_build=false])


LIBCURL_REQUIRES_STR="libcurl >= 1.0"
LIBZ_REQUIRES_STR="zlib >= 1.0"
LIBGENERAL_REQUIRES_STR="libgeneral >= 80"

# check for large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

# Checks for libraries.
PKG_CHECK_MODULES(curl, $LIBCURL_REQUIRES_STR)
PKG_CHECK_MODULES(zlib, $LIBZ_REQUIRES_STR)
PKG_CHECK_MODULES(libgeneral, $LIBGENERAL_REQUIRES_STR)

AC_SUBST([libcurl_requires], [$LIBCURL_REQUIRES_STR])
AC_SUBST([libz_requires], [$LIBZ_REQUIRES_STR])

if test "$debug_build" = true; then
  echo "*** Note: debug build requested ***"
  CFLAGS+=" -g -O0 -DDEBUG=1"
fi

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h stdio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([strdup])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libfragmentzip/Makefile
                 libfragmentzip.pc])
AC_OUTPUT

echo "
Configuration for $PACKAGE-$VERSION:
-------------------------------------------

  install prefix ..........: $prefix"

echo "  compiler ................: ${CC}

  Now type 'make' to build $PACKAGE-$VERSION,
  and then 'make install' for installation.
"
