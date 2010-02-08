noinst_LTLIBRARIES = libomxaudio_capturer.la

libomxaudio_capturer_la_SOURCES = audio_capturer.c audio_capturer.h
                                 
libomxaudio_capturer_la_LDFLAGS = $(OMXIL_LIBS)
libomxaudio_capturer_la_CFLAGS = $(OMXIL_CFLAGS)

bin_PROGRAMS = audio_capturer_test

audio_capturer_test_SOURCES = audio_capturer_test.c audio_capturer_test.h
audio_capturer_test_LDADD = -lpthread $(OMXIL_LIBS)
audio_capturer_test_CFLAGS = $(OMXIL_CFLAGS)
