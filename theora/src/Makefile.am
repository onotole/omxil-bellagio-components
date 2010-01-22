lib_LTLIBRARIES = libomxtheora.la

libomxtheora_la_SOURCES = omx_theoradec_component.c \
						  omx_theoradec_component.h \
						  library_entry_point.c

libomxtheora_la_LIBADD = -lomxil-bellagio $(THEORA_LIBS) $(THEORADEC_LIBS)
libomxtheora_la_LDFLAGS =
libomxtheora_la_CFLAGS = $(OMXIL_CFLAGS) $(THEORA_CFLAGS)

