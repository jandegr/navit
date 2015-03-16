spinner()
{
    local pid=$1
    local delay=0.75
    local spinstr='|/-\'
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        local temp=${spinstr#?}
        printf " [%c]  " "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
        printf "\b\b\b\b\b\b"
    done
    printf "    \b\b\b\b"
}

for phase in `seq 1 10`; do 
bzcat  ~/planet-150202.osm.bz2 | ./bin/navit/maptool/maptool -s $phase -e $phase ~/binmaps/planet-150202.bin > ~/assets/150202.log &
done

bzcat  ~/planet-150202.osm.bz2 | ./bin/navit/maptool/maptool -s 11 ~/binmaps/planet-150202.bin > ~/assets/150202.log &

spinner  $!
