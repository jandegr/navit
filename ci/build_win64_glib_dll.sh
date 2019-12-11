#!/bin/bash
set -e

# mingw-w64-tools is there to provide pkg-config
apt-get update && apt-get install -y mingw-w64 mingw-w64-tools default-jre nsis libsaxonb-java xz-utils ninja-build

# ubuntu version of mesa is too old at this time to compile latest glib
apt-get install -y python3-pip
pip3 install meson


export ARCH=x86_64-w64-mingw32
export PREFIX=/usr/x86_64-w64-mingw32
export PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/lib/pkgconfig

cp Toolchain/cross_file_mingw64.txt /tmp # for meson, used to compile glib

pushd .

# mkdir /tmp

# zlib
# https://wiki.openttd.org/Cross-compiling_for_Windows#zlib
cd /tmp
wget -c http://zlib.net/zlib-1.2.11.tar.gz
tar xzf zlib-1.2.11.tar.gz
cd zlib-1.2.11
sed -e s/"PREFIX ="/"PREFIX = x86_64-w64-mingw32-"/ -i win32/Makefile.gcc
make -f win32/Makefile.gcc
BINARY_PATH=/usr/x86_64-w64-mingw32/bin \
    INCLUDE_PATH=/usr/x86_64-w64-mingw32/include \
    LIBRARY_PATH=/usr/x86_64-w64-mingw32/lib \
    make -f win32/Makefile.gcc install
cd ..

#iconv
cd /tmp
wget ftp.gnu.org/pub/gnu/libiconv/libiconv-1.16.tar.gz
tar xzf libiconv-1.16.tar.gz
cd libiconv-1.16
./configure --host=$ARCH --prefix=$PREFIX
make
make install-strip
cd ..

#gettext
cd /tmp
wget ftp.gnu.org/pub/gnu/gettext/gettext-0.20.1.tar.gz
tar xzf gettext-0.20.1.tar.gz
cd gettext-0.20.1
./configure --prefix=$PREFIX --host=$ARCH --disable-dependency-tracking --enable-silent-rules \
            --disable-rpath --enable-nls --disable-csharp --disable-java \
            --enable-relocatable --enable-static
#make
#make install
cd gettext-runtime
make install
cd ../..


# glib
cd /tmp
wget -c http://ftp.gnome.org/pub/gnome/sources/glib/2.63/glib-2.63.2.tar.xz
tar xvf glib-2.63.2.tar.xz
cd glib-2.63.2
mkdir build
cd build
meson --prefix=$PREFIX --cross-file=/tmp/cross_file_mingw64.txt --buildtype=plain ../

ninja
ninja install


#pushd .

#wget ftp.postgresql.org/pub/latest/postgresql-11.3.tar.gz
#tar -z -xf postgresql-11.3.tar.gz
#cd postgresql-11.3
#./configure --host=x86_64-w64-mingw32 --without-zlib --prefix=/usr/x86_64-w64-mingw32
#make
#make install

#popd

popd

ls -la
mkdir win64
pushd win64
ls -la
cmake -Dbinding/python:BOOL=FALSE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=y -DHAVE_POSTGRESQL=n -DXSLTS=windows -DCMAKE_TOOLCHAIN_FILE=../Toolchain/mingw-w64-x86_64.cmake  ../ 
make
make package
popd

ls -la

cp win64/navit.exe win64/navit64install.exe
ls -la win64
