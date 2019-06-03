apt-get update && apt-get install -y mingw-w64 default-jre nsis libsaxonb-java



ls -la
mkdir win64
pushd win64
ls -la
cmake -Dbinding/python:BOOL=FALSE -DSAMPLE_MAP=n -DBUILD_MAPTOOL=y -DHAVE_POSTGRESQL=n -DXSLTS=windows -DCMAKE_TOOLCHAIN_FILE=../Toolchain/mingw-w64-x86_64.cmake  ../ 
make
make package
popd

ls -la

cp win64/navit.exe win64/navit64install.exe
ls -la win64
