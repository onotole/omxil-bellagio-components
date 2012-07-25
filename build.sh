#!/bin/bash

export OMX_CFLAGS="-g -O0 -I/home/kamil/praca/w1-party/kernel/headers/include"
export OMX_LDFLAGS="-L/home/kamil/praca/debian-env/libdrm/.libs"

export TOOLCHAIN_PATH=/usr/arm-linux-gnueabi
export OMX_INSTALL_PATH=/home/kamil/praca/openmax/sprc/dst7
export OMXCORE_INCLUDE_PATH=$OMX_INSTALL_PATH/usr/local/include
export OMX_PKG_CONFIG_PATH=$OMX_INSTALL_PATH/usr/local/lib/pkgconfig

if cd camera; then ./build.sh; cd ..; fi

if cd ffmpeg; then ./build.sh ; cd ..; fi

if cd colorconv; then ./build.sh; cd ..; fi

if cd fbvideo; then ./build.sh; cd ..; fi

if cd drmvideo; then ./build.sh; cd ..; fi

if cd ffmpeg-dist; then ./build.sh ; cd ..; fi

if cd mfc; then ./build.sh ; cd ..; fi

if cd examples; then ./build.sh ; cd ..; fi

