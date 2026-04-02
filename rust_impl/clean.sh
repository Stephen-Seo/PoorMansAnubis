#!/usr/bin/env sh

set -ev

cargo clean

pushd msql_ffi >&/dev/null && cargo clean ; popd >&/dev/null

make -C "$(dirname "$0")/../cxx_impl/bundled" clean
