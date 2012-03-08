plugin_LTLIBRARIES = libgstmikmod.la

libgstmikmod_la_SOURCES = gstmikmod.c drv_gst.c mikmod_reader.c 
libgstmikmod_la_CFLAGS = $(GST_CFLAGS) $(MIKMOD_CFLAGS)
libgstmikmod_la_LIBADD = $(GST_LIBS) $(MIKMOD_LIBS)
libgstmikmod_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstmikmod_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstmikmod.h 

EXTRA_DIST = README
