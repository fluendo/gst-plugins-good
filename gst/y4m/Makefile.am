
plugin_LTLIBRARIES = libgsty4menc.la

libgsty4menc_la_SOURCES = gsty4mencode.c
libgsty4menc_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
libgsty4menc_la_LIBADD = $(GST_LIBS)
libgsty4menc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgsty4menc_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gsty4mencode.h

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgsty4menc -:SHARED libgsty4menc \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgsty4menc_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgsty4menc_la_CFLAGS) \
	 -:LDFLAGS $(libgsty4menc_la_LDFLAGS) \
	           $(libgsty4menc_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
