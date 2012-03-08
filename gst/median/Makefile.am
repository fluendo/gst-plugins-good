plugin_LTLIBRARIES = libgstmedian.la

libgstmedian_la_SOURCES = gstmedian.c

libgstmedian_la_CFLAGS = $(GST_CFLAGS)
libgstmedian_la_LIBADD = $(GST_LIBS)
libgstmedian_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstmedian_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstmedian.h
