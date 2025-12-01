CC ?= /usr/bin/gcc
CXX ?= /usr/bin/g++
AR ?= /usr/bin/ar
RANLIB ?= /usr/bin/ranlib
LD ?= /usr/bin/ld
# System we are building on
BUILD_TRIPLE ?= x86_64-pc-linux-gnu
# System we are building for
TARGET_TRIPLE ?= x86_64-pc-linux-gnu
TARGET_ARCH ?= x86_64
# Note that aarch64 linux is typically: aarch64-linux-gnu

ifeq (${TARGET_ARCH},aarch64)
	BLAKE3_DISABLE_ASM_FLAG := -DBLAKE3_SIMD_TYPE=neon-intrinsics
else
	BLAKE3_DISABLE_ASM_FLAG :=
endif

PWD != pwd

all: ../cxx_backend_impl

../cxx_backend_impl: out/lib/libsqlite3.a out/lib/libblake3.a out/lib/libcurl.a
	PMA_PRE_LINKER_FLAGS="-L${PWD}/out/lib -L${PWD}/out/ssl/usr/lib" PMA_POST_LINKER_FLAGS="-lnghttp2 -lidn2 -lunistring -lzstd -llzma -llz4 -lbrotlidec -lbrotlicommon -lz -lssl -lcrypto" PMA_PRE_COMMON_FLAGS="-I${PWD}/out/include" PMA_PRE_COMMON_CFLAGS="-I${PWD}/out/include" RELEASE=1 ${MAKE} -C ..

out/lib/libsqlite3.a out/include/sqlite3.h: download_cache/sqlite-autoconf-3510100.tar.gz
	@mkdir -p sqlite_build
	tar -xf download_cache/sqlite-autoconf-3510100.tar.gz -C sqlite_build
	cd sqlite_build/sqlite-autoconf-3510100 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --disable-shared && ${MAKE} libsqlite3.a
	install -D -m444 sqlite_build/sqlite-autoconf-3510100/libsqlite3.a out/lib/libsqlite3.a
	install -D -m444 sqlite_build/sqlite-autoconf-3510100/sqlite3.h out/include/sqlite3.h
	rm -rf sqlite_build

download_cache/sqlite-autoconf-3510100.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://sqlite.org/2025/sqlite-autoconf-3510100.tar.gz
	sha256sum -c SHA256SUM_sqlite-3.51.1.txt || (rm -f download_cache/sqlite-autoconf-3510100.tar.gz && false)

out/lib/libblake3.a out/include/blake3.h: download_cache/blake3_1.8.2.tar.gz
	@mkdir -p blake3_build
	tar -xf download_cache/blake3_1.8.2.tar.gz -C blake3_build
	sed -i -e '/^target_compile_features/d' blake3_build/BLAKE3-1.8.2/c/CMakeLists.txt
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S blake3_build/BLAKE3-1.8.2/c -B blake3_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} -DCMAKE_C_COMPILER_TARGET=${TARGET_ARCH} -DCMAKE_CXX_COMPILER_TARGET=${TARGET_ARCH} ${BLAKE3_DISABLE_ASM_FLAG}
	${MAKE} -C blake3_build/build
	install -D -m444 blake3_build/build/libblake3.a out/lib/libblake3.a
	install -D -m444 blake3_build/BLAKE3-1.8.2/c/blake3.h out/include/blake3.h
	rm -rf blake3_build

download_cache/blake3_1.8.2.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -L -o blake3_1.8.2.tar.gz https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/1.8.2.tar.gz
	sha256sum -c SHA256SUM_blake3_1.8.2.tar.gz.txt || (rm -f download_cache/blake3_1.8.2.tar.gz && false)

out/lib/libcurl.a out/include/curl/curl.h: download_cache/curl-8.17.0.tar.xz out/lib/libnghttp2.a out/lib/libidn2.a out/lib/libzstd.a out/lib/libbrotlicommon.a out/lib/libz.a out/ssl/usr/lib/libssl.a
	@mkdir -p curl_build
	tar -xf download_cache/curl-8.17.0.tar.xz -C curl_build
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S curl_build/curl-8.17.0 -B curl_build/BUILD -DBUILD_SHARED_LIBS=Off -DBUILD_LIBCURL_DOCS=Off -DBUILD_MISC_DOCS=Off -DBUILD_STATIC_LIBS=On -DBUILD_TESTING=Off -DENABLE_CURL_MANUAL=Off -DCURL_DEFAULT_SSL_BACKEND=openssl -DCURL_USE_OPENSSL=On -DOPENSSL_ROOT_DIR="${PWD}/out/ssl" -DOPENSSL_INCLUDE_DIR="${PWD}/out/ssl/usr/include" -DOPENSSL_SSL_LIBRARY="${PWD}/out/ssl/usr/lib/libssl.a" -DOPENSSL_CRYPTO_LIBRARY="${PWD}/out/ssl/usr/lib/libcrypto.a" -DOPENSSL_USE_STATIC_LIBS=On -DZLIB_INCLUDE_DIR="${PWD}/out/include" -DZLIB_LIBRARY="${PWD}/out/lib/libz.a" -DZLIB_USE_STATIC_LIBS=On -DBROTLI_INCLUDE_DIR="${PWD}/out/include" -DBROTLICOMMON_LIBRARY="${PWD}/out/lib/libbrotlicommon.a" -DBROTLIDEC_LIBRARY="${PWD}/out/lib/libbrotlidec.a" -DZSTD_INCLUDE_DIR="${PWD}/out/include" -DZSTD_LIBRARY="${PWD}/out/lib/libzstd.a" -DNGHTTP2_INCLUDE_DIR="${PWD}/out/include" -DNGHTTP2_LIBRARY="${PWD}/out/lib/libnghttp2.a" -DCURL_DISABLE_LDAP=On -DLIBIDN2_INCLUDE_DIR="${PWD}/out/include" -DLIBIDN2_LIBRARY="${PWD}/out/lib/libidn2.a" -DCURL_USE_LIBPSL=Off -DCURL_USE_LIBSSH2=Off && ${MAKE} -C curl_build/BUILD libcurl_static
	install -D -m444 curl_build/BUILD/lib/libcurl.a out/lib/libcurl.a
	find curl_build/curl-8.17.0/include/curl -regex '.*\.h$$' -execdir install -D -m444 '{}' "${PWD}/out/include/curl/{}" ';'
	rm -rf curl_build

download_cache/curl-8.17.0.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://github.com/curl/curl/releases/download/curl-8_17_0/curl-8.17.0.tar.xz
	sha256sum -c SHA256SUM_curl-8.17.0.txt || (rm -f download_cache/curl-8.17.0.tar.xz && false)

out/lib/libnghttp2.a: download_cache/nghttp2-1.68.0.tar.xz
	@mkdir -p nghttp2_build
	tar -xf download_cache/nghttp2-1.68.0.tar.xz -C nghttp2_build
	cd nghttp2_build/nghttp2-1.68.0 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --disable-examples --disable-python-bindings --enable-lib-only && ${MAKE}
	install -D -m444 nghttp2_build/nghttp2-1.68.0/lib/.libs/libnghttp2.a out/lib/libnghttp2.a
	find nghttp2_build/nghttp2-1.68.0/lib/includes/nghttp2 -regex '.*\.h$$' -execdir install -D -m444 '{}' "${PWD}/out/include/nghttp2/{}" ';'
	rm -rf nghttp2_build

download_cache/nghttp2-1.68.0.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://github.com/nghttp2/nghttp2/releases/download/v1.68.0/nghttp2-1.68.0.tar.xz
	sha256sum -c SHA256SUM_nghttp2-1.68.0.tar.xz.txt || (rm -f download_cache/nghttp2-1.68.0.tar.xz && false)

out/lib/libidn2.a: download_cache/libidn2-2.3.8.tar.gz out/lib/libunistring.a
	@mkdir -p idn2_build
	tar -xf download_cache/libidn2-2.3.8.tar.gz -C idn2_build
	cd idn2_build/libidn2-2.3.8 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --with-libunistring-prefix="${PWD}/out" && ${MAKE}
	install -D -m444 idn2_build/libidn2-2.3.8/lib/.libs/libidn2.a out/lib/libidn2.a
	install -D -m444 idn2_build/libidn2-2.3.8/lib/idn2.h out/include/idn2.h
	rm -rf idn2_build

download_cache/libidn2-2.3.8.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://mirrors.ocf.berkeley.edu/gnu/libidn/libidn2-2.3.8.tar.gz
	sha256sum -c SHA256SUM_idn2-2.3.8.tar.gz.txt || (rm -f download_cache/libidn2-2.3.8.tar.gz && false)

out/lib/libunistring.a: download_cache/libunistring-1.4.1.tar.xz
	@mkdir -p libunistring_build
	tar -xf download_cache/libunistring-1.4.1.tar.xz -C libunistring_build
	cd libunistring_build/libunistring-1.4.1 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --prefix=/usr && ${MAKE} && ${MAKE} DESTDIR="${PWD}/libunistring_build/libunistring-1.4.1/HERE_OUT" install
	install -D -m444 libunistring_build/libunistring-1.4.1/lib/.libs/libunistring.a out/lib/libunistring.a
	cd libunistring_build/libunistring-1.4.1/HERE_OUT/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf libunistring_build

download_cache/libunistring-1.4.1.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://mirrors.ocf.berkeley.edu/gnu/libunistring/libunistring-1.4.1.tar.xz
	sha256sum -c SHA256SUM_libunistring-1.4.1.tar.xz.txt || (rm -f download_cache/libunistring-1.4.1.tar.xz && false)

out/lib/libzstd.a: download_cache/zstd-1.5.7.tar.zst out/lib/libz.a out/lib/liblzma.a out/lib/liblz4.a
	@mkdir -p zstd_build
	tar -xf download_cache/zstd-1.5.7.tar.zst -C zstd_build
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S zstd_build/zstd-1.5.7/build/cmake -B zstd_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DZSTD_ZLIB_SUPPORT=On -DZSTD_LZMA_SUPPORT=On -DZSTD_LZ4_SUPPORT=On -DZSTD_BUILD_CONTRIB=On -DZSTD_BUILD_STATIC=On -DZSTD_BUILD_SHARED=Off -DZSTD_BUILD_TESTS=Off -DZSTD_PROGRAMS_LINK_SHARED=Off -DZLIB_INCLUDE_DIR="${PWD}/out/include" -DZLIB_LIBRARY="${PWD}/out/lib/libz.a" -DLIBLZMA_INCLUDE_DIR="${PWD}/out/include" -DLIBLZMA_LIBRARY="${PWD}/out/lib/liblzma.a" -DLIBLZ4_INCLUDE_DIR="${PWD}/out/include" -DLIBLZ4_LIBRARY="${PWD}/out/lib/liblz4.a" -DZSTD_BUILD_CONTRIB=Off && ${MAKE} -C zstd_build/build && ${MAKE} DESTDIR="${PWD}/zstd_build/zstd_out" -C zstd_build/build install
	cd zstd_build/zstd_out/usr/lib && find . -name libzstd.a -execdir install -D -m444 '{}' "${PWD}/out/lib/libzstd.a" ';'
	cd zstd_build/zstd_out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf zstd_build

download_cache/zstd-1.5.7.tar.zst:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.zst
	sha256sum -c SHA256SUM_zstd-1.5.7.tar.zst.txt || (rm -f download_cache/zstd-1.5.7.tar.zst && false)

out/lib/libbrotlicommon.a out/lib/libbrotlidec.a: download_cache/brotli-1.2.0.tar.gz
	@mkdir -p brotli_build
	tar -xf download_cache/brotli-1.2.0.tar.gz -C brotli_build
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S brotli_build/brotli-1.2.0 -B brotli_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBROTLI_DISABLE_TESTS=On && ${MAKE} -C brotli_build/build && ${MAKE} DESTDIR="${PWD}/brotli_build/brotli_out" -C brotli_build/build install
	cd brotli_build/brotli_out/usr/lib && find . -regex '.*\.a$$' -execdir install -D -m444 '{}' "${PWD}/out/lib/{}" ';'
	cd brotli_build/brotli_out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf brotli_build

download_cache/brotli-1.2.0.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o brotli-1.2.0.tar.gz -L https://github.com/google/brotli/archive/refs/tags/v1.2.0.tar.gz
	sha256sum -c SHA256SUM_brotli-1.2.0.tar.gz.txt || (rm -f download_cache/brotli-1.2.0.tar.gz && false)

out/lib/libz.a: download_cache/zlib-1.3.1.tar.xz
	@mkdir -p zlib_build
	tar -xf download_cache/zlib-1.3.1.tar.xz -C zlib_build
	cd zlib_build/zlib-1.3.1 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --prefix=/usr --static && ${MAKE} && ${MAKE} DESTDIR="${PWD}/zlib_build/out/" install
	install -D -m444 zlib_build/out/usr/lib/libz.a out/lib/libz.a
	cd zlib_build/out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf zlib_build

download_cache/zlib-1.3.1.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz
	sha256sum -c SHA256SUM_zlib-1.3.1.tar.xz.txt || (rm -f download_cache/zlib-1.3.1.tar.xz && false)

out/ssl/usr/lib/libssl.a: download_cache/openssl-3.6.0.tar.gz
	@mkdir -p openssl_build
	tar -xf download_cache/openssl-3.6.0.tar.gz -C openssl_build
	cd openssl_build/openssl-3.6.0 && sed -i -e 's|./demoCA|/etc/ssl|' apps/CA.pl.in apps/openssl.cnf
	cd openssl_build/openssl-3.6.0 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./Configure --prefix=/usr --openssldir=/etc/ssl --libdir=lib no-docs no-shared enable-ktls enable-ec_nistp_64_gcc_128 disable-tests linux-${TARGET_ARCH} && ${MAKE} && ${MAKE} DESTDIR="${PWD}/out/ssl" install
	rm -rf openssl_build

download_cache/openssl-3.6.0.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL https://github.com/openssl/openssl/releases/download/openssl-3.6.0/openssl-3.6.0.tar.gz
	sha256sum -c SHA256SUM_openssl-3.6.0.tar.gz.txt || (rm -f download_cache/openssl-3.6.0.tar.gz && false)

out/lib/liblzma.a: download_cache/xz_v5.8.1.tar.gz
	@mkdir -p xz_utils_build
	tar -xf download_cache/xz_v5.8.1.tar.gz -C xz_utils_build
	cd xz_utils_build/xz-5.8.1 && ./autogen.sh --no-po4a && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --prefix=/usr --disable-rpath --disable-doxygen --build="${BUILD_TRIPLE}" --host="${TARGET_TRIPLE}" --disable-shared --enable-static && ${MAKE} -C src/liblzma
	install -D -m444 xz_utils_build/xz-5.8.1/src/liblzma/.libs/liblzma.a out/lib/liblzma.a
	cd xz_utils_build/xz-5.8.1/src/liblzma/api && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf xz_utils_build

download_cache/xz_v5.8.1.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o xz_v5.8.1.tar.gz -L https://github.com/tukaani-project/xz/archive/refs/tags/v5.8.1.tar.gz
	sha256sum -c SHA256SUM_xz_v5.8.1.tar.gz.txt || (rm -f download_cache/xz_v5.8.1.tar.gz && false)

out/lib/liblz4.a: download_cache/lz4_v1.10.0.tar.gz
	@mkdir -p lz4_build
	tar -xf download_cache/lz4_v1.10.0.tar.gz -C lz4_build
	cd lz4_build/lz4-1.10.0 && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S build/cmake -B BUILD -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBUILD_STATIC_LIBS=On -DLZ4_BUILD_CLI=Off && ${MAKE} -C BUILD && ${MAKE} DESTDIR=HERE_OUT -C BUILD install
	find lz4_build/lz4-1.10.0/BUILD/HERE_OUT/usr/lib -name liblz4.a -execdir install -D -m444 '{}' "${PWD}/out/lib/liblz4.a" ';'
	cd lz4_build/lz4-1.10.0/BUILD/HERE_OUT/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf lz4_build

download_cache/lz4_v1.10.0.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o lz4_v1.10.0.tar.gz -L https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz
	sha256sum -c SHA256SUM_lz4_v1.10.0.tar.gz.txt || (rm -f download_cache/lz4_v1.10.0.tar.gz && false)

.PHONY: clean

clean:
	rm -rf out
	rm -rf sqlite_build
	rm -rf blake3_build
	rm -rf curl_build
	rm -rf nghttp2_build
	rm -rf idn2_build
	rm -rf libunistring_build
	rm -rf zstd_build
	rm -rf brotli_build
	rm -rf zlib_build
	rm -rf openssl_build
	rm -rf xz_utils_build
	rm -rf lz4_build
	${MAKE} -C .. clean
