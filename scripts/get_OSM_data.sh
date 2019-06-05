set -e

echo "downloading PBF"

# wget -c -O  ~/europe-latest-complete.osm.pbf http://ftp.snt.utwente.nl/pub/misc/openstreetmap/europe-latest.osm.pbf
wget -c -O  europe-latest-complete.osm.pbf http://download.geofabrik.de/europe-latest.osm.pbf
wget http://m.m.i24.cc/osmconvert.c && gcc osmconvert.c -lz -o osmconvert
ls -la 
./osmconvert --complete-ways --drop-author --verbose ~/europe-latest-complete.osm.pbf -B=BFR.poly -o=europe-latest.osm.pbf
ls -la
