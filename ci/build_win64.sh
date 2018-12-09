apt-get update && apt-get install -y mingw-w64 mingw-w64-x86-64-dev default-jdk nsis libsaxonb-java

mkdir win64
pushd win64
cmake -Dbinding/python:BOOL=FALSE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=n -DXSLTS=windows -DCMAKE_TOOLCHAIN_FILE=../Toolchain/mingw-w64-x86_64.cmake  ../ 
make
make package
popd

ls -la

cp win64/navit.exe win64/navit64install.exe
ls -la win64
