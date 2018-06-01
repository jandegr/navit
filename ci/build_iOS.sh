#!/bin/bash

echo "build iOS"
brew install cmake
brew install gettext
brew link --force gettext
brew install librsvg
#gem install shenzhen
cd $HOME/git
rm navit/xpm/gui_map.svg
rm navit/xpm/country_BR.svgz
rm navit/xpm/country_KY.svgz
mkdir navit-build
cd navit-build
cmake -G Xcode ../ -DCMAKE_TOOLCHAIN_FILE=Toolchain/xcode-iphone.cmake -DUSE_PLUGINS=0 -DBUILD_MAPTOOL=0 -DSAMPLE_MAP=0 -DXSLTS=iphone
# mv ../navit.xcworkspace ./
ls -la
xcodebuild -list -project navit.xcodeproj
#ipa --help
#ipa build
xcodebuild -configuration RelWithDebInfo ARCHS="arm64"
ls -la navit/RelWithDebInfo-iphoneos/navit.app
ls -la navit/RelWithDebInfo-iphoneos/navit.app/share
ls -la navit/RelWithDebInfo-iphoneos/navit.app/share/navit
# try to make bitrise show the available simulators
xcrun simctl list
ls -la
echo "END OF build_iOS.sh"
