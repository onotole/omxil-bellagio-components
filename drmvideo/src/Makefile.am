lib_LTLIBRARIES = libomxdrmdev.la

libomxdrmdev_la_SOURCES = omx_drmdev_sink_component.c omx_drmdev_sink_component.h \
			 library_entry_point.c drm.c

libomxdrmdev_la_LIBADD  = $(OMXIL_LIBS) -L/home/kamil/praca/debian-env/libdrm/.libs -ldrm
libomxdrmdev_la_LDFLAGS = 
libomxdrmdev_la_CFLAGS  = $(OMXIL_CFLAGS)
