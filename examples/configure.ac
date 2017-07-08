AC_INIT([OpenMAX application examples], [0.1], [], [omxBellagioExamples])
AM_INIT_AUTOMAKE([tar-ustar])

# Prerequisite autoconf version
AC_PREREQ([2.59])

PKG_CHECK_MODULES(OMXIL, libomxil-bellagio >= 0.9 )
AC_SUBST(OMXIL_CFLAGS)
AC_SUBST(OMXIL_LIBS)

AC_CONFIG_HEADERS([config.h])
CFLAGS="${CFLAGS}"

################################################################################
# Set the shared versioning info, according to section 6.3 of the libtool info #
# pages. CURRENT:REVISION:AGE must be updated immediately before each release: #
#                                                                              #
#   * If the library source code has changed at all since the last             #
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').             #
#                                                                              #
#   * If any interfaces have been added, removed, or changed since the         #
#     last update, increment CURRENT, and set REVISION to 0.                   #
#                                                                              #
#   * If any interfaces have been added since the last public release,         #
#     then increment AGE.                                                      #
#                                                                              #
#   * If any interfaces have been removed since the last public release,       #
#     then set AGE to 0.                                                       #
#                                                                              #
################################################################################
SHARED_VERSION_INFO="0:0:0"
AC_SUBST(SHARED_VERSION_INFO)

# Set to 'm4' the directory where the extra autoconf macros are stored
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])

AC_ARG_ENABLE(
    [debug],
    [AC_HELP_STRING(
        [--enable-debug],
        [whether to enable an x debug level])],
    [with_debug_level=$enableval],
    [with_debug_level=no])

################################################################################
# Check for programs                                                           #
################################################################################

# Check for a working C compiler
AC_PROG_CC
AM_PROG_CC_C_O

# Check for libtool
AM_PROG_LIBTOOL

################################################################################
# Check for libraries                                                          #
################################################################################
CFG_DEBUG_LEVEL=0
# Define debug messages level
if test "x$with_debug_level" = "xno"; then
 CFG_DEBUG_LEVEL=0
fi
if test "x$with_debug_level" = "xyes"; then
 CFG_DEBUG_LEVEL=255
fi
if test "x$with_debug_level" = "x1"; then
 CFG_DEBUG_LEVEL=2
fi
if test "x$with_debug_level" = "x2"; then
 CFG_DEBUG_LEVEL=4
fi
if test "x$with_debug_level" = "x3"; then
 CFG_DEBUG_LEVEL=8
fi
if test "x$with_debug_level" = "x4"; then
 CFG_DEBUG_LEVEL=16
fi
if test "x$with_debug_level" = "x5"; then
 CFG_DEBUG_LEVEL=32
fi
if test "x$with_debug_level" = "x6"; then
 CFG_DEBUG_LEVEL=255
fi

AC_SUBST(CFG_DEBUG_LEVEL)
CFLAGS="${CFLAGS} -DCONFIG_DEBUG_LEVEL=$CFG_DEBUG_LEVEL"

################################################################################
# Check for header files                                                       #
################################################################################

################################################################################
# Check for pkg-config modules                                                 #
################################################################################

################################################################################
# Check for types                                                              #
################################################################################

# unused

################################################################################
# Check for structures                                                         #
################################################################################

# unused

################################################################################
# Check for compiler characteristics                                           #
################################################################################

# unused

################################################################################
# Check for library functions                                                  #
################################################################################

# unused

################################################################################
# Check for system services                                                    #
################################################################################

# unused

################################################################################
# Conditionals and file output                                                 #
################################################################################

AC_OUTPUT
