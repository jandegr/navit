SET(USE_UIKIT 1)
SET(TARGETSDK iPhoneOS9.3.sdk)
# selection of arm64 apparently not working, stays armv7
SET(CMAKE_OSX_ARCHITECTURES arm64)
SET(ARCHS "-arch arm64")
SET(CMAKE_MACOSX_BUNDLE YES)
SET(CMAKE_XCODE_GENERATE_SCHEME YES)
SET(IOS_PLATFORM "OS")
SET(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
#SET(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
SET(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/PLatforms/iPhoneOS.platform/Developer/SDKs/${TARGETSDK} CACHE STRING "")
SET(CMAKE_TRY_COMPILE_OSX_BUNDLE 1)
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT})
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
