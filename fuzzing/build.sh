#!/bin/bash
# WARNING: this script is used by https://github.com/google/oss-fuzz/blob/master/projects/mapserver/Dockerfile
# and should not be renamed or moved without updating the above

set -e

if [ "$SRC" == "" ]; then
    echo "SRC env var not defined"
    exit 1
fi

if [ "$OUT" == "" ]; then
    echo "OUT env var not defined"
    exit 1
fi

if [ "$CXX" == "" ]; then
    echo "CXX env var not defined"
    exit 1
fi

if [ "$LIB_FUZZING_ENGINE" = "" ]; then
    export LIB_FUZZING_ENGINE=-lFuzzingEngine
fi

# Build mapserver
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$SRC/install \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_STATIC=ON \
      -DWITH_CLIENT_WMS=1 -DWITH_CLIENT_WFS=1 -DWITH_KML=1 -DWITH_SOS=1 \
      -DWITH_THREAD_SAFETY=1 \
      -DWITH_FCGI=0 -DWITH_EXEMPI=1 \
      -DWITH_RSVG=1 -DWITH_CURL=1 -DWITH_FRIBIDI=1 -DWITH_HARFBUZZ=1 \
      -DCMAKE_C_FLAGS="$CFLAGS -DACCEPT_USE_OF_DEPRECATED_PROJ_API_H" \
      -DCMAKE_CXX_FLAGS="$CXXFLAGS -DACCEPT_USE_OF_DEPRECATED_PROJ_API_H" \
      ..
make clean -s
make -j$(nproc) -s
make install
cd ..

# Make sure to copy all .so dependencies
ldd $SRC/install/lib/libmapserver.so
mkdir -p $OUT/lib
for i in `ldd $SRC/install/lib/libmapserver.so | grep -v linux-vdso.so.1 | grep -v ld-linux-x86-64 | awk '{print $3}'`; do
    cp $i $OUT/lib;
done

# Build fuzzer
$CXX $CXXFLAGS -std=c++11 fuzzing/query_string_fuzzer.cpp -o $OUT/query_string_fuzzer \
            -I$SRC/install/include/mapserver -I/usr/include/gdal \
            $LIB_FUZZING_ENGINE $SRC/install/lib/libmapserver_static.a \
            -Wl,-rpath,'$ORIGIN/lib' \
            -lgdal -lxml2 -lpq -lproj \
            -lcairo -lfreetype -lharfbuzz -lfribidi \
            -lpng -ljpeg -lgif -lprotobuf-c -lgeos_c -lrsvg-2 -lcurl -lexempi \
            -lgobject-2.0

ldd $OUT/query_string_fuzzer

# Copy resources
cp /usr/share/gdal/2.2/* $OUT/
cp /usr/share/proj/* $OUT/
cp -r msautotest/wxs/data $OUT/
cp fuzzing/query_string_fuzzer.map $OUT/
