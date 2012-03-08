plugin_LTLIBRARIES = libgstaasink.la

libgstaasink_la_SOURCES = gstaasink.c
libgstaasink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(AALIB_CFLAGS)
libgstaasink_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(AALIB_LIBS)
libgstaasink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstaasink_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstaasink.h
