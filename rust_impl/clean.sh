#!/usr/bin/env sh

set -ev

cargo clean

make -C "$(dirname "$0")/../cxx_impl/bundled" clean
