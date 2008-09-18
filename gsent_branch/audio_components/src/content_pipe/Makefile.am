omx_content_pipedir = $(plugindir)

noinst_LTLIBRARIES = libomx_content_pipe.la

libomx_content_pipe_la_SOURCES = content_pipe_core.c  content_pipe_file.c \
                              content_pipe_file.h  content_pipe_inet.c \
                              content_pipe_inet.h

libomx_content_pipe_la_LDFLAGS = $(PLUGIN_LDFLAGS) -lomxil-bellagio

bin_PROGRAMS = content_pipe_test

content_pipe_test_SOURCES = content_pipe_test.c
content_pipe_test_LDADD = -lpthread -lomxil-bellagio
