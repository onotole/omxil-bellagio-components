noinst_LTLIBRARIES = libomxaudio_writer.la

libomxaudio_writer_la_SOURCES = audio_writer.c audio_writer.h

libomxaudio_writer_la_LDFLAGS = $(OMXIL_LIBS)
libomxaudio_writer_la_CFLAGS = $(OMXIL_CFLAGS)

bin_PROGRAMS = audio_writer_test

audio_writer_test_SOURCES = audio_writer_test.c audio_writer_test.h
audio_writer_test_LDADD = -lpthread $(OMXIL_LIBS)
audio_writer_test_CFLAGS = $(OMXIL_CFLAGS)
