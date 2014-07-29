oftvg_PROGRAMS = tvg_analyzer

tvg_analyzer_SOURCES = analyzer_main.c loader.c

noinst_HEADERS = loader.h

WFLAGS = -Wall -Wextra -Wno-unused-parameter -O0 -ggdb
tvg_analyzer_CFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(WFLAGS)
tvg_analyzer_CXXFLAGS = $(GST_CFLAGS) $(GDK_CFLAGS) $(WFLAGS)
tvg_analyzer_LDADD = $(GST_LIBS) $(GDK_LIBS)
tvg_analyzer_LDFLAGS = $(GST_PLUGIN_LDFLAGS)


