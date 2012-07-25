#!/bin/sh


cflags="-I$OMX_INSTALL_PATH/include -I$TOOLCHAIN_PATH/include \
	-I$OMXCORE_INCLUDE_PATH \
	-L$OMX_INSTALL_PATH/usr/local/lib \
	$OMX_CFLAGS " 

make distclean

autoreconf -i

PKG_CONFIG_PATH=$OMX_PKG_CONFIG_PATH \
ac_cv_func_memset_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes  ac_cv_func_malloc_0_nonnull=yes ./configure \
 --host=arm-linux-gnueabi \
 --prefix=/usr/local \
 --enable-shared \
 CFLAGS="$cflags"


mkdir -p $OMX_INSTALL_PATH

make uninstall DESTDIR=$OMX_INSTALL_PATH
make
make install DESTDIR=$OMX_INSTALL_PATH
