lib_LTLIBRARIES = libomxaudioeffects.la

libomxaudioeffects_la_SOURCES = omx_audio_effect.c omx_audio_effect.h \
                                 library_entry_point.c

libomxaudioeffects_la_LDFLAGS = $(PLUGIN_LDFLAGS) -lomxil-bellagio

bin_PROGRAMS = omxvolcontroltest

omxvolcontroltest_SOURCES = omxvolcontroltest.c omxvolcontroltest.h
omxvolcontroltest_LDADD = -lpthread -lomxil-bellagio
