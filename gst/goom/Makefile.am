
plugin_LTLIBRARIES = libgstgoom.la

PPC_FILES=ppc_zoom_ultimate.s ppc_drawings.s ppc_drawings.h ppc_zoom_ultimate.h
MMX_FILES=mmx.c xmmx.c mmx.h xmmx.h

if HAVE_CPU_PPC
# disable until someone figures out how to build these correctly on ppc
#ARCH_FILES = $(PPC_FILES)
ARCH_FILES =
endif
if HAVE_CPU_PPC64
# disable until someone figures out how to build these correctly on ppc
#ARCH_FILES = $(PPC_FILES)
ARCH_FILES =
endif
if HAVE_CPU_I386
ARCH_FILES = $(MMX_FILES)
ARCH_CFLAGS = -DBUILD_MMX
endif
				
libgstgoom_la_SOURCES =						\
	gstgoom.c gstgoom.h					\
	drawmethods.c drawmethods.h				\
	sound_tester.c sound_tester.h				\
	mathtools.c mathtools.h					\
	lines.c lines.h ifs.c ifs.h surf3d.c surf3d.h		\
	tentacle3d.c tentacle3d.h v3d.c v3d.h			\
	convolve_fx.c flying_stars_fx.c				\
	goom_fx.h goom_visual_fx.h				\
	motif_goom1.h motif_goom2.h				\
	plugin_info.c goom_plugin_info.h			\
	goom_tools.c						\
	config_param.c filters.c goom_core.c graphic.c		\
	goom.h goom_typedefs.h goom_graphic.h			\
	goom_config_param.h goom_visual_fx.h goom_filters.h	\
	goom_tools.h goom_tools.h goom_config.h			\
	$(ARCH_FILES)

libgstgoom_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(GOOM_FILTER_CFLAGS) $(ARCH_CFLAGS) $(ORC_CFLAGS)
libgstgoom_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(LIBM) $(ORC_LIBS)
libgstgoom_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstgoom_la_LIBTOOLFLAGS = --tag=disable-static
endif

EXTRA_DIST = $(PPC_FILES) $(MMX_FILES)

.NOTPARALLEL:

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstgoom -:SHARED libgstgoom \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstgoom_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstgoom_la_CFLAGS) \
	 -:LDFLAGS $(libgstgoom_la_LDFLAGS) \
	           $(libgstgoom_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
