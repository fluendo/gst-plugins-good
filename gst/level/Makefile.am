plugin_LTLIBRARIES = libgstlevel.la

libgstlevel_la_SOURCES = gstlevel.c
libgstlevel_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstlevel_la_LIBADD = $(GST_BASE_LIBS) $(LIBM)
libgstlevel_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstlevel_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstlevel.h


Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstlevel -:SHARED libgstlevel \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstlevel_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstlevel_la_CFLAGS) \
	 -:LDFLAGS $(libgstlevel_la_LDFLAGS) \
	           $(libgstlevel_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
