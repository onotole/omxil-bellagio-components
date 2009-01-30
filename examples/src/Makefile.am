bin_PROGRAMS = omxaudiodectest omxvoicexchange omxaudioenctest

bellagio_LDADD = -lomxil-bellagio
common_CFLAGS = -I$(top_srcdir)/test/components/common -I$(includedir)

omxaudiodectest_SOURCES = omxaudiodectest.c omxaudiodectest.h
omxaudiodectest_LDADD = $(bellagio_LDADD) -lpthread
omxaudiodectest_CFLAGS = $(common_CFLAGS)

omxvoicexchange_SOURCES = omxvoicexchange.c omxvoicexchange.h 
omxvoicexchange_LDADD =  $(bellagio_LDADD) -lpthread 
omxvoicexchange_CFLAGS= $(common_CFLAGS)

omxaudioenctest_SOURCES = omxaudioenctest.c omxaudioenctest.h
omxaudioenctest_LDADD = $(bellagio_LDADD) -lpthread
omxaudioenctest_CFLAGS = $(common_CFLAGS)

