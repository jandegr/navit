#!/bin/bash

echo "build iOS"
brew install cmake
ls -la
mkdir navit-build
ls -la
cd navit-build
ls -la
cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/xcode-iphone.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone 
