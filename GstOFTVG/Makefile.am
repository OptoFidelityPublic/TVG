oftvg_LTLIBRARIES = libgstoftvg.la

libgstoftvg_la_SOURCES = \
	gstoftvg.cc gstoftvg_impl.cc gstoftvg_layout.cc \
	gstoftvg_pixbuf.cc

libgstoftvg_la_CFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS)
libgstoftvg_la_CXXFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS)
libgstoftvg_la_LIBADD = $(GST_LIBS) $(GDK_LIBS)
libgstoftvg_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# Building a static version of a Gst plugin is not useful
libgstoftvg_la_LIBTOOLFLAGS = --tag=disable-static


