oftvg_LTLIBRARIES = libgstoftvg.la

# Plugin toplevel files
libgstoftvg_la_SOURCES = plugin_main.c gstoftvg.cc

# Video filter and helper classes
libgstoftvg_la_SOURCES += gstoftvg_video.cc
libgstoftvg_la_SOURCES += gstoftvg_video_process.cc gstoftvg_layout.cc gstoftvg_pixbuf.cc

# Audio source
libgstoftvg_la_SOURCES += gstoftvg_audio.cc

WFLAGS = -Wall -Wextra -Wno-unused-parameter -O0 -ggdb
libgstoftvg_la_CFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(WFLAGS)
libgstoftvg_la_CXXFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(WFLAGS)
libgstoftvg_la_LIBADD = $(GST_LIBS) $(GDK_LIBS)
libgstoftvg_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# Building a static version of a Gst plugin is not useful
libgstoftvg_la_LIBTOOLFLAGS = --tag=disable-static


