#!/bin/bash

echo "build iOS"
brew install cmake
brew install gettext
brew link --force gettext
wget ftp://ftp.imagemagick.org/pub/ImageMagick/binaries/ImageMagick-x86_64-apple-darwin15.4.0.tar.gz
ls -la
mkdir navit-build
cd navit-build
cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/xcode-iphone.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone 
xcodebuild -configuration RelWithDebInfo 
