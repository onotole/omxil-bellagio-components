#!/bin/sh

echo $TOOLCHAIN_PATH
echo $OMX_INSTALL_PATH
echo $OMXCORE_INCLUDE_PATH

cflags="-I$OMX_INSTALL_PATH/include -I$TOOLCHAIN_PATH/include \
	-I$OMXCORE_INCLUDE_PATH \
	-L$OMX_INSTALL_PATH/usr/local/lib"

make distclean
autoreconf -i

PKG_CONFIG_PATH=$OMX_PKG_CONFIG_PATH \
ac_cv_func_memset_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes  ac_cv_func_malloc_0_nonnull=yes CFLAGS="$cflags" ./configure \
 --host=arm-linux-gnueabi \
 --prefix=$OMX_INSTALL_PATH/usr/local \
 --enable-shared \
 --includedir=$OMX_INSTALL_PATH/usr/local/include


#ac_cv_func_memset_0_nonnull=yes ac_cv_func_realloc_0_nonnull=yes  ac_cv_func_malloc_0_nonnull=yes ./configure \
# --host=arm-linux-gnueabi \
# --prefix=/usr/local \
# --enable-shared \
#  CFLAGS="$cflags"

mkdir -p $OMX_INSTALL_PATH

make uninstall #DESTDIR=$OMX_INSTALL_PATH
make
make install #DESTDIR=$OMX_INSTALL_PATH
