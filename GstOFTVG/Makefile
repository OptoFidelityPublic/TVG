GST_LOCATION=C:\Work\Linux\1.2\windows_x86_64
#GST_LOCATION=C:\gstreamer\1.0\x86_64

#CPPFLAGS=-I$(GST_LOCATION)\include\gstreamer-0.10 -I$(GST_LOCATION)\include\glib-2.0 -I$(GST_LOCATION)\lib\glib-2.0\include -I$(GST_LOCATION)\include\libxml2 -I$(GST_LOCATION)\include\gdk-pixbuf-2.0
CPPFLAGS=-I$(GST_LOCATION)\include\gstreamer-1.0 -I$(GST_LOCATION)\include\glib-2.0 -I$(GST_LOCATION)\lib\glib-2.0\include -I$(GST_LOCATION)\include\libxml2 -I$(GST_LOCATION)\include\gdk-pixbuf-2.0 -I/home/jlemm/cerbero/mingw/w64/include -v
CXXFLAGS=-fpermissive -fno-exceptions -std=c++11

HEADERS=timemeasure.h gstoftvg.hh gstoftvg_impl.hh gstoftvg_layout.hh gstoftvg_pixbuf.hh
OBJS=gstoftvg.o gstoftvg_impl.o gstoftvg_layout.o gstoftvg_pixbuf.o

LDFLAGS=-Wl,--exclude-all-symbols --shared -L$(GST_LOCATION)\lib
LDLIBS=-lgstreamer-1.0 -lgstbase-1.0 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lxml2 -lpthread -lz -lm -lglib-2.0 -lgstvideo-1.0 -lgdk_pixbuf-2.0

.PHONY:  default all clean install

default: all

all:	liboftvg-1.0.dll

liboftvg-1.0.dll:	$(OBJS) 
	$(CXX) -o $@ GstOFTVG.def $(OBJS) $(LDFLAGS) $(LDLIBS)

%.o:	%.cc $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

clean:	
	rm -f $(OBJS) liboftvg-1.0.dll

install: liboftvg-1.0.dll
	cp liboftvg-1.0.dll $(GST_LOCATION)\lib\gstreamer-1.0\