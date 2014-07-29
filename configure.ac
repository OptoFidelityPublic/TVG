#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
AC_INIT([oftvg], [0.9], NULL, oftvg)
AC_CONFIG_SRCDIR([GstOFTVG/gstoftvg.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10])

LT_PREREQ([2.2.6])
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

# Checks for libraries.
GST_REQ=1.0.0
PKG_CHECK_MODULES([GST],
    [gstreamer-1.0 >= $GST_REQ
    gstreamer-base-1.0 >= $GST_REQ
    gstreamer-plugins-base-1.0 >= $GST_REQ
    gstreamer-pbutils-1.0 >= $GST_REQ
    gstreamer-video-1.0 >= $GST_REQ],
    [AC_SUBST(GST_CFLAGS) AC_SUBST(GST_LIBS)]
)

PKG_CHECK_MODULES([GDK],
    [gdk-pixbuf-2.0 >= 2.0.0],
    [AC_SUBST(GDK_CFLAGS) AC_SUBST(GDK_LIBS)]
)

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.


# Checks for library functions.

# Installation path for GStreamer plugins
oftvgdir="\$(libdir)/gstreamer-1.0"
AC_SUBST(oftvgdir)

# LDFLAGS for GStreamer plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).* -no-undefined'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES([Makefile GstOFTVG/Makefile Analyzer/Makefile])
AC_OUTPUT
