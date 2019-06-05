set -e

echo "downloading PBF"

wget -c -O  ~/europe-latest-complete.osm.pbf http://ftp.snt.utwente.nl/pub/misc/openstreetmap/europe-latest.osm.pbf
wget http://m.m.i24.cc/osmconvert.c && gcc osmconvert.c -lz -o osmconvert
ls -la /home/ubuntu/navit
./osmconvert --complete-ways --drop-author --verbose ~/europe-latest-complete.osm.pbf -B=/home/ubuntu/navit/BFR.poly -o=/home/ubuntu/europe-latest.osm.pbf
