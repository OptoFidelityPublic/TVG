#GST_LOCATION=C:\gstreamer\1.0\x86_64
GST_LOCATION=C:\gstreamer-sdk\0.10\x86_64

CPPFLAGS=-I$(GST_LOCATION)\include\gstreamer-0.10 -I$(GST_LOCATION)\include\glib-2.0 -I$(GST_LOCATION)\lib\glib-2.0\include -I$(GST_LOCATION)\include\libxml2 -I$(GST_LOCATION)\include\gdk-pixbuf-2.0
#CPPFLAGS=-I$(GST_LOCATION)\include\gstreamer-1.0 -I$(GST_LOCATION)\include\glib-2.0 -I$(GST_LOCATION)\lib\glib-2.0\include -I$(GST_LOCATION)\include\libxml2 -I$(GST_LOCATION)\include\gdk-pixbuf-2.0
CXXFLAGS=-fpermissive -fno-exceptions -std=c++11

HEADERS=timemeasure.h gstoftvg.hh gstoftvg_impl.hh gstoftvg_layout.hh gstoftvg_pixbuf.hh
OBJS=gstoftvg.o gstoftvg_impl.o gstoftvg_layout.o gstoftvg_pixbuf.o

LDFLAGS=-Wl,--exclude-all-symbols --shared -L$(GST_LOCATION)\lib
LDLIBS=-lgstreamer-0.10 -lgstbase-0.10 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lxml2 -lpthread -lz -lm -lglib-2.0 -lgstvideo-0.10 -lgdk-win32-2.0 -lgdk_pixbuf-2.0

.PHONY:  default all clean install

default: all

all:	liboftvg-0.10.dll

liboftvg-0.10.dll:	$(OBJS) 
	$(CXX) -o $@ GstOFTVG.def $(OBJS) $(LDFLAGS) $(LDLIBS)

%.o:	%.cc $(HEADERS)
	$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

clean:	
	rm $(OBJS)

install: liboftvg-0.10.dll
	cp liboftvg-0.10.dll $(GST_LOCATION)\lib\gstreamer-0.10\