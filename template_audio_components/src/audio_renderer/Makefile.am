noinst_LTLIBRARIES = libomxaudio_renderer.la

libomxaudio_renderer_la_SOURCES = audio_renderer.c audio_renderer.h

libomxaudio_renderer_la_LDFLAGS = $(OMXIL_LIBS)
libomxaudio_renderer_la_CFLAGS = $(OMXIL_CFLAGS)

bin_PROGRAMS = audio_renderer_test

audio_renderer_test_SOURCES = audio_renderer_test.c audio_renderer_test.h
audio_renderer_test_LDADD = -lpthread $(OMXIL_LIBS)
audio_renderer_test_CFLAGS = $(OMXIL_CFLAGS)
