#!/bin/bash

set -e

# To use this, build for arm64 only and insert the following
# step in circleci before building navit for Android.
#
# This solution works but can use further refinement.
#
# - run:
#          name: Build with seperate Glib
#          command: |
#            sudo apt-get update
#            sudo apt-get install -y pkg-config
#            sudo apt-get install -y gcc
#            sudo apt-get install -y xz-utils ninja-build
#            sudo apt-get install -y python3-pip
#            sudo pip3 install meson
#            bash ci/build_android_glib.sh arm64 21
# ------ before this existing step
#      - run:
#          name: Install cmake gettext libsaxonb-java librsvg2-bin mmv rename

arch=$1
api=$2
toolchain_path=/opt/android/sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64
prefix_path=/opt/android/sdk/ndk-bundle/sysroot

target_host=aarch64-linux-android
export AR=$target_host-ar
export AS=$target_host-as
export CC=aarch64-linux-android21-clang
export CXX=aarch64-linux-android21-clang++
export LD=$target_host-ld
export STRIP=$target_host-strip
export PATH=$PATH:$toolchain_path/bin

ls -la /opt
ls -la /opt/android
ls -la /opt/android/sdk
ls -la /opt/android/sdk/ndk-bundle
ls -la /opt/android/sdk/ndk-bundle/toolchains
ls -la /opt/android/sdk/ndk-bundle/toolchains/llvm
ls -la /opt/android/sdk/ndk-bundle/toolchains/llvm/prebuilt
# Cross build libiconv when using API level <= 28.
# Newer Android has it in its libc already.
if [ "$api" -lt "28" ]; then
  wget http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz
  echo "1233fe3ca09341b53354fd4bfe342a7589181145a1232c9919583a8c9979636855839049f3406f253a9d9829908816bb71fd6d34dd544ba290d6f04251376b1a  libiconv-1.15.tar.gz" | sha512sum -c
  tar xzf libiconv-1.15.tar.gz
  pushd libiconv-1.15
  ./configure --host=$target_host --prefix=$prefix_path  --libdir=$prefix_path/lib64
  make
  make install
  popd
fi

ls -la $prefix_path/lib64

# Cross build libffi
#wget https://github.com/libffi/libffi/releases/download/v3.3-rc0/libffi-3.3-rc0.tar.gz
#echo "e6e695d32cd6eb7d65983f32986fccdfc786a593d2ea18af30ce741f58cfa1eb264b1a8d09df5084cb916001aea15187b005c2149a0620a44397a4453b6137d4  libffi-3.3-rc0.tar.gz" | sha512sum -c
#tar xzf libffi-3.3-rc0.tar.gz
#pushd libffi-3.3-rc0
#./configure --host=$target_host --prefix=$prefix_path --libdir=$prefix_path/lib64
#make
#make install
#popd

cd /tmp

# Create a cross file that can be passed to meson
cat > cross_file_android_${arch}_${api}.txt <<- EOM

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'arm64'
endian = 'little'

[properties]
c_args = ['-I${prefix_path}/include']
c_link_args = ['-L${prefix_path}/lib64','-fuse-ld=gold']
needs_exe_wrapper = true

[binaries]
c = '${toolchain_path}/bin/${CC}'
cpp = '${toolchain_path}/bin/${CXX}'
ar = '${toolchain_path}/bin/${AR}'
strip = '${toolchain_path}/bin/${STRIP}'
EOM

# meson needs a compiler for the build machine
export CC=gcc
export CXX=gcc

# glib
cd /tmp
wget -c http://ftp.gnome.org/pub/gnome/sources/glib/2.62/glib-2.62.0.tar.xz
tar xvf glib-2.62.0.tar.xz
cd glib-2.62.0
mkdir build

meson  --buildtype plain --prefix=$prefix_path --cross-file=/tmp/cross_file_android_arm64_21.txt -Diconv=auto -Dinternal_pcre=true build

ninja -C build
ninja -C build install

mkdir /opt/android/sdk/ndk-bundle/sysroot/lib/arm64-v8a
cp /opt/android/sdk/ndk-bundle/sysroot/lib/*.so /opt/android/sdk/ndk-bundle/sysroot/lib/arm64-v8a
cp /opt/android/sdk/ndk-bundle/sysroot/lib64/*.so /opt/android/sdk/ndk-bundle/sysroot/lib/arm64-v8a
