#!/bin/bash

echo "build iOS"
brew install cmake
brew install gettext
brew link --force gettext
#brew install librsvg
#gem install shenzhen
cd $HOME/git
rm navit/xpm/gui_map.svg
rm navit/xpm/country_BR.svgz
rm navit/xpm/country_KY.svgz
mkdir navit-build
cd navit-build
cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/xcode-iphone.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone -DIOS_PLATFORM=OS
#cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/ios.toolchain.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone -DIOS_PLATFORM=OS
# mv ../navit.xcworkspace ./
ls -la
ls -la navit.xcodeproj
xcodebuild -list -project navit.xcodeproj
#ipa --help
#ipa build
#xcodebuild -configuration RelWithDebInfo ARCHS="arm64"
xcodebuild -configuration Release archive
ls -la navit
ls -la navit/Release-iphoneos/navit.app
ls -la navit/Release-iphoneos/navit.app/share
ls -la navit/Release-iphoneos/navit.app/share/navit
xcrun simctl list
ls -la
xcodebuild -list -project navit.xcodeproj
echo "END OF build_iOS.sh"
