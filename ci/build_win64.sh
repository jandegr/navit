apt-get update && apt-get install -y mingw-w64 default-jre nsis libsaxonb-java

# pushd .

# wget ftp.postgresql.org/pub/latest/postgresql-11.3.tar.gz
# tar -z -xf postgresql-11.3.tar.gz
# cd postgresql-11.3
# ./configure --host=x86_64-w64-mingw32 --without-zlib --prefix=/usr/x86_64-w64-mingw32
# make
# make install

popd

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
