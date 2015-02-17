#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([waveform], [0.9.2], [rzurad@gmail.com])
AC_CONFIG_SRCDIR([src/main.c])
#AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([avcodec], [avcodec_set_dimensions], [], [
    echo "FFmpeg appears to be missing or broken"
    exit -1])
AC_CHECK_LIB([avformat], [av_format_get_audio_codec], [], [
    echo "FFmpeg appears to be missing or broken"
    exit -1])
AC_CHECK_LIB([avutil], [avutil_version], [], [
    echo "FFmpeg appears to be missing or broken"
    exit -1])
AC_CHECK_LIB([png], [png_get_pixels_per_inch], [], [
    echo "libpng appears to be missing or broken"
    exit -1])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([floor pow])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
