omxaudio_readerrdir = $(plugindir)

noinst_LTLIBRARIES = libomxaudio_reader.la

libomxaudio_reader_la_SOURCES = audio_reader.c audio_reader.h

libomxaudio_reader_la_LDFLAGS = $(PLUGIN_LDFLAGS) -lomxil-bellagio

bin_PROGRAMS = audio_reader_test

audio_reader_test_SOURCES = audio_reader_test.c audio_reader_test.h
audio_reader_test_LDADD = -lpthread -lomxil-bellagio
