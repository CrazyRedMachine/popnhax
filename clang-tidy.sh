#!/bin/bash
# Run clang-tidy on a source file
TOOLCHAIN_PREFIX=i686-w64-mingw32

CLANG_TIDY=clang-tidy
SYSROOT=$($TOOLCHAIN_PREFIX-gcc -print-sysroot)/mingw

if [ ! -f $SYSROOT/include/windows.h ]; then
    SYSROOT=/usr/i686-w64-mingw32
    if [ ! -f $SYSROOT/include/windows.h ]; then
        SYSROOT=/usr/local/i686-w64-mingw32
    fi
fi

CPP_HEADERS="/usr/share/mingw-w64/include/"

# echo "System root: $SYSROOT"
# echo "C++ headers: $CPP_HEADERS"

$CLANG_TIDY $1 \
    -- \
    -target i686-w64-mingw32 \
    -std=$2 \
    --sysroot $SYSROOT \
    -isysroot $SYSROOT \
    -I $CPP_HEADERS \
    -I. \
    -Iinclude \
    -Dssize_t=int \
    -DAVS_VERSION=1700 \
    -D_CRT_SECURE_NO_WARNINGS=1 \
    -Di386=1 \
    -D__i386=1 \
    -D__i386__=1 \
    -D_WIN32=1 \
    -DCOBJMACROS
