plugin_LTLIBRARIES = libgstspeex.la

libgstspeex_la_SOURCES = gstspeex.c gstspeexdec.c gstspeexenc.c
libgstspeex_la_CFLAGS = -DGST_USE_UNSTABLE_API \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(SPEEX_CFLAGS)
libgstspeex_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgsttag-$(GST_MAJORMINOR) -lgstaudio-$(GST_MAJORMINOR) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(SPEEX_LIBS)
libgstspeex_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(LIBM)
if !GST_PLUGIN_BUILD_STATIC
libgstspeex_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstspeexenc.h gstspeexdec.h 
