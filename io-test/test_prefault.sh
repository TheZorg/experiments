#!/bin/bash

main() {
    if [[ $EUID -ne 0 ]]
    then
        echo "Must be root to flush cache"
        return
    fi

    local large_file=large_file
    local program=./io-test
    local args=

    local separator="--------------------------------------------------------------------------------"
    local blue='\033[0;34m'
    local red='\033[0;31m'
    local cyan='\033[0;36m'
    local NC='\033[0m'

    if [ ! -e $large_file ]
    then
        echo "Creating large test file"
        ../scripts/make_large_file.sh large_file
    fi

    echo -e "${cyan}Testing without prefault${NC}"
    args="-t 1 -i 1"
    echo -e "${blue}Cache cold${NC}"
    ../scripts/cache_cold.sh $program $args $large_file
    echo -e "${red}Cache hot${NC}"
    $program $args $large_file
    echo $separator

    echo -e "${cyan}Testing with prefault${NC}"
    args="-t 1 -i 1 -p"
    echo -e "${blue}Cache cold${NC}"
    ../scripts/cache_cold.sh $program $args $large_file
    echo -e "${red}Cache hot${NC}"
    $program $args $large_file
    echo $separator
}

main $@
