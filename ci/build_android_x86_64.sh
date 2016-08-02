export START_PATH=~/
export SOURCE_PATH=$START_PATH"/"${CIRCLE_PROJECT_REPONAME}"/"
export CMAKE_FILE=$SOURCE_PATH"/Toolchain/x86_64-android.cmake"
export ANDROID_NDK="/usr/local/android-ndk/"
export ANDROID_NDK_BIN=$ANDROID_NDK"/toolchains/x86-4.8/prebuilt/linux-x86_64/bin"
export ANDROID_SDK="/usr/local/android-sdk-linux/"
export ANDROID_SDK_PLATFORM_TOOLS=$ANDROID_SDK"/platform-tools"
export PATH=$ANDROID_NDK_BIN:$ANDROID_SDK_PLATFORM_TOOLS:$PATH

mkdir android-x86 && cd android-x86
cmake -DCMAKE_TOOLCHAIN_FILE=$CMAKE_FILE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=n -DANDROID_API_VERSION=23 -DANDROID_NDK_API_VERSION=19 -DDISABLE_CXX=1 -DDISABLE_QT=1 ../ || exit -1
make || exit -1
make apkg-release
make apkg

if [[ "${CIRCLE_BRANCH}" == "master" ]]; then
  make apkg-release && mv navit/android/bin/Navit-release-unsigned.apk $CIRCLE_ARTIFACTS/navit-$CIRCLE_SHA1-x86_64-release-unsigned.apk || exit 1
else
  make apkg && mv navit/android/bin/Navit-debug.apk $CIRCLE_ARTIFACTS/navit-$CIRCLE_SHA1-x86_64-debug.apk || exit 1
fi

echo
echo "Build leftovers :"
ls -la 
ls -la navit
ls -la navit/android
ls -la navit/android/bin
