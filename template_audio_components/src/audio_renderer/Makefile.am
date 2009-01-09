omxaudio_rendererrdir = $(plugindir)

noinst_LTLIBRARIES = libomxaudio_renderer.la

libomxaudio_renderer_la_SOURCES = audio_renderer.c audio_renderer.h

libomxaudio_renderer_la_LDFLAGS = $(PLUGIN_LDFLAGS) -lomxil-bellagio

bin_PROGRAMS = audio_renderer_test

audio_renderer_test_SOURCES = audio_renderer_test.c audio_renderer_test.h
audio_renderer_test_LDADD = -lpthread -lomxil-bellagio
