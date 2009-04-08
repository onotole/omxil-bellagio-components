LIBSOURCES= jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c \
        jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c \
        jcparam.c jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c \
        jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
        jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdpostct.c \
        jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c \
        jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c \
        jutils.c jmemmgr.c jaricom.c jcarith.c jdarith.c
SYSDEPSOURCES= jmemansi.c 
APPSOURCES= cjpeg.c djpeg.c jpegtran.c rdjpgcom.c wrjpgcom.c cdjpeg.c \
        rdcolmap.c rdswitch.c transupp.c rdppm.c wrppm.c rdgif.c wrgif.c \
        rdtarga.c wrtarga.c rdbmp.c wrbmp.c rdrle.c wrrle.c

noinst_LTLIBRARIES = libjpeg.la

libjpeg_la_SOURCES = $(LIBSOURCES) $(SYSDEPSOURCES)

include_extradir = $(includedir)
