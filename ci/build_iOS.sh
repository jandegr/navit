#!/bin/bash

echo "build iOS"
brew install cmake
brew install gettext
brew link --force gettext
#cd $HOME
#wget ftp://ftp.imagemagick.org/pub/ImageMagick/binaries/ImageMagick-x86_64-apple-darwin15.4.0.tar.gz
#tar xvzf ImageMagick-x86_64-apple-darwin15.4.0.tar.gz
#export MAGICK_HOME="$HOME/ImageMagick-7.0.1"
#export PATH="$MAGICK_HOME/bin:$PATH"
#export DYLD_LIBRARY_PATH="$MAGICK_HOME/lib/"
#brew install librsvg
cd $HOME/git
rm navit/xpm/gui_map.svg
ls -la
mkdir navit-build
cd navit-build
cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/xcode-iphone.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone 
xcodebuild -configuration RelWithDebInfo 
