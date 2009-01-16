lib_LTLIBRARIES = libomxffmpegnonfree.la

libomxffmpegnonfree_la_SOURCES = omx_amr_audiodec_component.c \
                                 omx_amr_audiodec_component.h \
                                 omx_amr_audioenc_component.c \
                                 omx_amr_audioenc_component.h \
                                 library_entry_point.c

libomxffmpegnonfree_la_LIBADD  = -lomxil-bellagio
libomxffmpegnonfree_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(FFMPEG_LIBS)
libomxffmpegnonfree_la_CFLAGS  = $(FFMPEG_CFLAGS)

