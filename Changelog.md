# Changelog

## Upcoming Changes

## Version 0.2.4

Refactor `libdb_msql_capi.a` compilation.

cxx\_impl CMakeLists.txt:

 - opt-in to flags that Debian Trixie's gcc doesn't support.
 - Minor fix to git-version related messages in CMakeLists.txt .

## Version 0.2.3

cxx bundled: Update bundled dependencies:

 - lz4 1.11.0 -> 1.11.1

## Version 0.2.2

cxx\_impl: Fix endpoint urls not resolving correctly due to inconsistent
handling of urls.

rust\_impl: `cargo update`, clippy fixes

## Version 0.2.1

cxx\_impl: Minor refactorings of CMakeLists.txt .

Fix `cxx_impl/Makefile` to build `libdb_msql_capi.a` properly.

Refactor `cxx_impl/bundled/CMakeLists.txt` and `cxx_impl/bundled/Makefile` to
use variables for bundled dependencies' versions.

Update cxx bundled dependencies:

 - lz4 1.10.0 -> 1.11.0
 - zlib 1.3.2 -> 1.4

rust\_impl: `cargo update`

## Version 0.2.0

Fix rust\_impl rebuild failing due to build-script.

Use "git describe --tags --long" for additional version info when using the
`--version` flag.

For both rust\_impl and cxx\_impl:  
Rebuild sources that display "git-version" when using `--version` flag.

Update bundled dependency:

 - nghttp2 1.70.0 -> 1.71.0

rust\_impl: `cargo update`, and update dep `rusqlite`.

## Version 0.1.0

First versioned PoorMansAnubs with features:

 - Protects a destination url with 2 possible challenge-verification types
    - Verify via "factors" that is done automatically in the browser with JS
       - This method generates a large number and the client's answer must be all the factors of the large number
    - Verify via generated image and manual input with matching word (captcha)
       - This verification method defaults to JS/JSON submission, but can be changed to use a REST API by using the `--enable-rest-api` option
 - Implemented in both C++ and Rust
    - Rust version uses reqwest to forward requests, and salvo to handle requests
    - C++ version uses unix sockets for most requests, and libcurl (when enabled with `--enable-libcurl`) to forward requests to https destinations
 - Uses either Sqlite or MySQL to persist data (temporarily) to keep track of allowed clients, timed-out clients, and data related to challenges presented to the client

Check the `-h` or `--help` options for either Rust or C++ versions for more info.
