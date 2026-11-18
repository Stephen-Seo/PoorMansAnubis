#!/usr/bin/env bash

export TARGET_ARCH=aarch64
export TARGET_TRIPLE=${TARGET_ARCH}-linux-gnu
export CC=/usr/bin/${TARGET_TRIPLE}-gcc
export CXX=/usr/bin/${TARGET_TRIPLE}-g++
export AS=/usr/bin/${TARGET_TRIPLE}-as
export AR=/usr/bin/${TARGET_TRIPLE}-ar
export RANLIB=/usr/bin/${TARGET_TRIPLE}-ranlib
export LD=/usr/bin/${TARGET_TRIPLE}-ld
export STRIP=/usr/bin/${TARGET_TRIPLE}-strip

set -e

if (( $# == 0 )); then
    echo -n "Build in /tmp ? (y/n) > "

    read reply

    if [[ "$reply" =~ ^[yY]([eE][sS])?$ ]]; then
        BUILD_DESTINATION="/tmp/buildPMA_aarch64"
    else
        BUILD_DESTINATION="buildPMA_aarch64"
    fi

    echo "BUILD_DESTINATION set to $BUILD_DESTINATION"
    echo "Will build in 3 seconds..."

    "$0" -d "$BUILD_DESTINATION"
    exit $?
else
    while getopts 'd:' op; do
        case $op in
            'd') BUILD_DESTINATION="$OPTARG";;
        esac
    done

    if [[ -z "$BUILD_DESTINATION" ]]; then
        echo 'ERROR: Build destination is unset!'
        exit 1
    fi

    echo "Build to dest \"$BUILD_DESTINATION\" in 3 seconds..."

    sleep 3

    cd "$(dirname "$0")"

    function do_build() {
        MAX_JOBS="$(nproc)"

        if ! [[ $MAX_JOBS =~ ^[0-9]+$ ]]; then
            MAX_JOBS=1
            MAX_LOAD_AVG=1
        elif (( MAX_JOBS > 1 )); then
            MAX_LOAD_AVG=$MAX_JOBS 
            MAX_JOBS=$(( MAX_JOBS - 1 ))
        else
            MAX_JOBS=1
            MAX_LOAD_AVG=1
        fi

        set -v

        cmake -S bundled/ -B "$BUILD_DESTINATION" -DCMAKE_BUILD_TYPE=Release

        make -C "$BUILD_DESTINATION" -j $MAX_JOBS -l $MAX_LOAD_AVG poor_mans_anubis_bundled
        ${STRIP} --strip-all "$BUILD_DESTINATION/poor_mans_anubis_bundled"
    }

    time do_build
fi
