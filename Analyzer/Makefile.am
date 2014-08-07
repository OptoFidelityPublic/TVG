bin_PROGRAMS = tvg_analyzer

tvg_analyzer_SOURCES = analyzer_main.c loader.c layout.c lipsync.c markertype.c

noinst_HEADERS = loader.h

WFLAGS = -Wall -Wextra -Wno-unused-parameter -ggdb
tvg_analyzer_CFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(ZLIB_CFLAGS) $(WFLAGS)
tvg_analyzer_CXXFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(ZLIB_CFLAGS) $(WFLAGS)
tvg_analyzer_LDADD = $(GST_LIBS) $(GDK_LIBS) $(ZLIB_LIBS) -lm
tvg_analyzer_LDFLAGS = $(GST_PLUGIN_LDFLAGS)


