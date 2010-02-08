noinst_LTLIBRARIES = libomxaudio_decoder.la

libomxaudio_decoder_la_SOURCES = audio_decoder.c audio_decoder.h

libomxaudio_decoder_la_LDFLAGS = $(OMXIL_LIBS)
libomxaudio_decoder_la_CFLAGS = $(OMXIL_CFLAGS)

bin_PROGRAMS = audio_decoder_test

audio_decoder_test_SOURCES = audio_decoder_test.c audio_decoder_test.h
audio_decoder_test_LDADD = -lpthread $(OMXIL_LIBS)
audio_decoder_test_CFLAGS = $(OMXIL_CFLAGS)
