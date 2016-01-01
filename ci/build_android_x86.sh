export START_PATH=~/
export SOURCE_PATH=$START_PATH"/"${CIRCLE_PROJECT_REPONAME}"/"
export CMAKE_FILE=$SOURCE_PATH"/Toolchain/i686-android.cmake"
export ANDROID_NDK="/usr/local/android-ndk/"
export ANDROID_NDK_BIN=$ANDROID_NDK"/toolchains/x86-4.8/prebuilt/linux-x86_64/bin"
export ANDROID_SDK="/usr/local/android-sdk-linux/"
export ANDROID_SDK_PLATFORM_TOOLS=$ANDROID_SDK"/platform-tools"
export PATH=$ANDROID_NDK_BIN:$ANDROID_SDK_PLATFORM_TOOLS:$PATH

mkdir android-x86 && cd android-x86
cmake -DCMAKE_TOOLCHAIN_FILE=$CMAKE_FILE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=n -DANDROID_API_VERSION=19 -DDISABLE_CXX=1 -DDISABLE_QT=1 ../ || exit -1
make || exit -1
make apkg-release
make apkg
echo
echo "Build leftovers :"
ls -la 
ls -la navit
ls -la navit/android
ls -la navit/android/bin

