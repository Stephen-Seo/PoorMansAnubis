#!/usr/bin/env bash

set -e

echo -n "Build in /tmp ? (y/n) > "

read reply

if [[ "$reply" =~ ^[yY]([eE][sS])?$ ]]; then
    BUILD_DESTINATION="/tmp/buildPMA_aarch64"
else
    BUILD_DESTINATION="buildPMA_aarch64"
fi

echo "BUILD_DESTINATION set to $BUILD_DESTINATION"
echo "Will build in 3 seconds..."

sleep 3

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

CC=/usr/bin/aarch64-linux-gnu-gcc CXX=/usr/bin/aarch64-linux-gnu-g++ AR=/usr/bin/aarch64-linux-gnu-ar RANLIB=/usr/bin/aarch64-linux-gnu-ranlib LD=/usr/bin/aarch64-linux-gnu-ld TARGET_TRIPLE="aarch64-linux-gnu" TARGET_ARCH=aarch64 AS=/usr/bin/aarch64-linux-gnu-as cmake -S bundled/ -B "$BUILD_DESTINATION" -DCMAKE_BUILD_TYPE=Release

make -C "$BUILD_DESTINATION" -j $MAX_JOBS -l $MAX_LOAD_AVG poor_mans_anubis_bundled
/usr/bin/aarch64-linux-gnu-strip --strip-all "$BUILD_DESTINATION/poor_mans_anubis_bundled"
