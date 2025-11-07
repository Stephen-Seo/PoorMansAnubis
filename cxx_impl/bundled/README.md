# Bundled C++ Implementation of Poor Man's Anubis Backend

The Makefile in this directory will bundle almost all dependencies of
PoorMansAnubs C++ backend into static libraries and will compile
`cxx_backend_impl` into the parent directory with all dependencies statically
linked.

~~Note that this version of `cxx_backend_impl` does not have OpenSSL support
compiled in: this means that a --dest-url=... or --port-to-dest-url=... cannot
redirect to a site hosted with HTTPS (only HTTP).~~

`openssl` is now built as part of the dependencies. Though it is uncertain if
openssl needs to be installed on the machine running the `cxx_backend_impl`...
