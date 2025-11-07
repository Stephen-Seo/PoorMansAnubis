#!/usr/bin/env sh

CC=/usr/bin/aarch64-linux-gnu-gcc \
CXX=/usr/bin/aarch64-linux-gnu-g++ \
AR=/usr/bin/aarch64-linux-gnu-ar \
RANLIB=/usr/bin/aarch64-linux-gnu-ranlib \
LD=/usr/bin/aarch64-linux-gnu-ld \
TARGET_TRIPLE=aarch64-unknown-linux-gnu \
TARGET_ARCH=aarch64 \
make $*
