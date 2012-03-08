plugin_LTLIBRARIES = libgstsunaudio.la

libgstsunaudio_la_SOURCES = gstsunaudio.c \
                            gstsunaudiosink.c \
                            gstsunaudiomixerctrl.c \
                            gstsunaudiomixer.c \
                            gstsunaudiomixertrack.c \
                            gstsunaudiomixeroptions.c \
                            gstsunaudiosrc.c

libgstsunaudio_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)
libgstsunaudio_la_LIBADD = \
        -lgstinterfaces-@GST_MAJORMINOR@ \
        -lgstaudio-@GST_MAJORMINOR@ \
        $(GST_PLUGINS_BASE_LIBS) \
        $(GST_LIBS)
libgstsunaudio_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
if !GST_PLUGIN_BUILD_STATIC
libgstsunaudio_la_LIBTOOLFLAGS = --tag=disable-static
endif

noinst_HEADERS = gstsunaudiosink.h \
                 gstsunaudiomixer.h \
                 gstsunaudiomixerctrl.h \
                 gstsunaudiomixertrack.h \
                 gstsunaudiomixeroptions.h \
                 gstsunaudiosrc.h

