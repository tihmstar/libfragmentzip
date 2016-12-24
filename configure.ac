AC_PREREQ([2.69])
AC_INIT([libfragmentzip], [1.0], [tihmstar@gmail.com])
AC_CONFIG_HEADERS([config.h])
AM_PROG_LIBTOOL

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(libfragmentzip, 1.0)

# Checks for programs.
AC_PROG_CC
AC_LANG_C

AC_CONFIG_MACRO_DIRS([m4])

# Checks for libraries.
PKG_CHECK_MODULES(curl, libcurl >= 1.0)
PKG_CHECK_MODULES(zlib, zlib >= 1.0)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero strdup])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 libfragmentzip/Makefile
		 libfragmentzip.pc])
AC_OUTPUT
