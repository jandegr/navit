apt-get update && apt-get install -y mingw-w64 default-jdk nsis libsaxonb-java

mkdir win32
pushd win32
cmake -Dbinding/python:BOOL=FALSE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=n -DXSLTS=windows -DCMAKE_TOOLCHAIN_FILE=../Toolchain/mingw-w64-i686.cmake  ../ 
make
make package
popd

ls -la
ls -la win32
cp win32/*.exe $CIRCLE_ARTIFACTS/
