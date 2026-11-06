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

SQLITE_VERSION := 3.54.1
SQLITE_VERSION_NUMBER := 3540100
SQLITE_DOWNLOAD_URL := https://sqlite.org/2026/sqlite-autoconf-${SQLITE_VERSION_NUMBER}.tar.gz

BLAKE3_VERSION := 1.8.7
BLAKE3_DOWNLOAD_URL := https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/${BLAKE3_VERSION}.tar.gz

NGHTTP2_VERSION := 1.71.0
NGHTTP2_DOWNLOAD_URL := https://github.com/nghttp2/nghttp2/releases/download/v${NGHTTP2_VERSION}/nghttp2-${NGHTTP2_VERSION}.tar.xz

LIBUNISTRING_VERSION := 1.5
LIBUNISTRING_DOWNLOAD_URL := https://mirrors.ocf.berkeley.edu/gnu/libunistring/libunistring-${LIBUNISTRING_VERSION}.tar.xz

LIBIDN2_VERSION := 2.3.8
LIBIDN2_DOWNLOAD_URL := https://mirrors.ocf.berkeley.edu/gnu/libidn/libidn2-${LIBIDN2_VERSION}.tar.gz

ZLIB_VERSION := 1.4
ZLIB_DOWNLOAD_URL := https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}/zlib-${ZLIB_VERSION}.tar.xz

XZ_VERSION := 5.8.4
XZ_DOWNLOAD_URL := https://github.com/tukaani-project/xz/archive/refs/tags/v${XZ_VERSION}.tar.gz

LZ4_VERSION := 1.11.0
LZ4_DOWNLOAD_URL := https://github.com/lz4/lz4/archive/refs/tags/v${LZ4_VERSION}.tar.gz

ZSTD_VERSION := 1.5.7
ZSTD_DOWNLOAD_URL := https://github.com/facebook/zstd/releases/download/v${ZSTD_VERSION}/zstd-${ZSTD_VERSION}.tar.zst

BROTLI_VERSION := 1.2.1
BROTLI_DOWNLOAD_URL := https://github.com/google/brotli/archive/refs/tags/v${BROTLI_VERSION}.tar.gz

OPENSSL_VERSION := 4.1.0
OPENSSL_DOWNLOAD_URL := https://github.com/openssl/openssl/archive/refs/tags/openssl-${OPENSSL_VERSION}.tar.gz

CURL_VERSION := 8.23.0
CURL_TAG := curl-8_23_0
CURL_DOWNLOAD_URL := https://github.com/curl/curl/releases/download/${CURL_TAG}/curl-${CURL_VERSION}.tar.xz

ifeq (${TARGET_ARCH},aarch64)
	BLAKE3_DISABLE_ASM_FLAG := -DBLAKE3_SIMD_TYPE=neon-intrinsics
else
	BLAKE3_DISABLE_ASM_FLAG :=
endif

PWD != pwd

all: ../cxx_backend_impl

../cxx_backend_impl: sqlite3_TARGET out/lib/libblake3.a out/lib/libcurl.a
	PMA_PRE_LINKER_FLAGS="-L${PWD}/out/lib -L${PWD}/out/ssl/usr/lib" PMA_POST_LINKER_FLAGS="-lnghttp2 -lidn2 -lunistring -lzstd -llzma -llz4 -lbrotlidec -lbrotlicommon -lz -lssl -lcrypto" RELEASE=1 ${MAKE} -C ..

sqlite_build/: download_cache/sqlite-autoconf-${SQLITE_VERSION_NUMBER}.tar.gz
	@mkdir -p sqlite_build
	(test -e out/include/sqlite3.h && test -e out/lib/libsqlite3.a) || tar -xf download_cache/sqlite-autoconf-${SQLITE_VERSION_NUMBER}.tar.gz -C sqlite_build

out/include/sqlite3.h: | sqlite_build/
	install -D -m444 sqlite_build/sqlite-autoconf-${SQLITE_VERSION_NUMBER}/sqlite3.h out/include/sqlite3.h

out/lib/libsqlite3.a: | sqlite_build/
	cd sqlite_build/sqlite-autoconf-${SQLITE_VERSION_NUMBER} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --disable-shared && ${MAKE} libsqlite3.a
	install -D -m444 sqlite_build/sqlite-autoconf-${SQLITE_VERSION_NUMBER}/libsqlite3.a out/lib/libsqlite3.a

sqlite3_TARGET: out/include/sqlite3.h out/lib/libsqlite3.a
	rm -rf sqlite_build

download_cache/sqlite-autoconf-${SQLITE_VERSION_NUMBER}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${SQLITE_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_sqlite-${SQLITE_VERSION}.txt || (rm -f download_cache/sqlite-autoconf-${SQLITE_VERSION_NUMBER}.tar.gz && false)

out/lib/libblake3.a out/include/blake3.h: download_cache/blake3_${BLAKE3_VERSION}.tar.gz
	@mkdir -p blake3_build
	tar -xf download_cache/blake3_${BLAKE3_VERSION}.tar.gz -C blake3_build
	sed -i -e '/^target_compile_features/d' blake3_build/BLAKE3-${BLAKE3_VERSION}/c/CMakeLists.txt
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S blake3_build/BLAKE3-${BLAKE3_VERSION}/c -B blake3_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} -DCMAKE_C_COMPILER_TARGET=${TARGET_ARCH} -DCMAKE_CXX_COMPILER_TARGET=${TARGET_ARCH} ${BLAKE3_DISABLE_ASM_FLAG}
	${MAKE} -C blake3_build/build
	install -D -m444 blake3_build/build/libblake3.a out/lib/libblake3.a
	install -D -m444 blake3_build/BLAKE3-${BLAKE3_VERSION}/c/blake3.h out/include/blake3.h
	rm -rf blake3_build

download_cache/blake3_${BLAKE3_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -L -o blake3_${BLAKE3_VERSION}.tar.gz ${BLAKE3_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_blake3_${BLAKE3_VERSION}.tar.gz.txt || (rm -f download_cache/blake3_${BLAKE3_VERSION}.tar.gz && false)

out/lib/libcurl.a out/include/curl/curl.h: download_cache/curl-${CURL_VERSION}.tar.xz out/lib/libnghttp2.a out/lib/libidn2.a out/lib/libzstd.a out/lib/libbrotlicommon.a out/lib/libz.a out/ssl/usr/lib/libssl.a
	@mkdir -p curl_build
	tar -xf download_cache/curl-${CURL_VERSION}.tar.xz -C curl_build
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S curl_build/curl-${CURL_VERSION} -B curl_build/BUILD -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=Off -DBUILD_LIBCURL_DOCS=Off -DBUILD_MISC_DOCS=Off -DBUILD_STATIC_LIBS=On -DBUILD_TESTING=Off -DENABLE_CURL_MANUAL=Off -DCURL_DEFAULT_SSL_BACKEND=openssl -DCURL_USE_OPENSSL=On -DOPENSSL_ROOT_DIR="${PWD}/out/ssl" -DOPENSSL_INCLUDE_DIR="${PWD}/out/ssl/usr/include" -DOPENSSL_SSL_LIBRARY="${PWD}/out/ssl/usr/lib/libssl.a" -DOPENSSL_CRYPTO_LIBRARY="${PWD}/out/ssl/usr/lib/libcrypto.a" -DOPENSSL_USE_STATIC_LIBS=On -DZLIB_INCLUDE_DIR="${PWD}/out/include" -DZLIB_LIBRARY="${PWD}/out/lib/libz.a" -DZLIB_USE_STATIC_LIBS=On -DBROTLI_INCLUDE_DIR="${PWD}/out/include" -DBROTLICOMMON_LIBRARY="${PWD}/out/lib/libbrotlicommon.a" -DBROTLIDEC_LIBRARY="${PWD}/out/lib/libbrotlidec.a" -DZSTD_INCLUDE_DIR="${PWD}/out/include" -DZSTD_LIBRARY="${PWD}/out/lib/libzstd.a" -DNGHTTP2_INCLUDE_DIR="${PWD}/out/include" -DNGHTTP2_LIBRARY="${PWD}/out/lib/libnghttp2.a" -DCURL_DISABLE_LDAP=On -DLIBIDN2_INCLUDE_DIR="${PWD}/out/include" -DLIBIDN2_LIBRARY="${PWD}/out/lib/libidn2.a" -DCURL_USE_LIBPSL=Off -DCURL_USE_LIBSSH2=Off && ${MAKE} -C curl_build/BUILD libcurl_static
	install -D -m444 curl_build/BUILD/lib/libcurl.a out/lib/libcurl.a
	find curl_build/curl-${CURL_VERSION}/include/curl -regex '.*\.h$$' -execdir install -D -m444 '{}' "${PWD}/out/include/curl/{}" ';'
	rm -rf curl_build

download_cache/curl-${CURL_VERSION}.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${CURL_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_curl-${CURL_VERSION}.txt || (rm -f download_cache/curl-${CURL_VERSION}.tar.xz && false)

out/lib/libnghttp2.a: download_cache/nghttp2-${NGHTTP2_VERSION}.tar.xz
	@mkdir -p nghttp2_build
	tar -xf download_cache/nghttp2-${NGHTTP2_VERSION}.tar.xz -C nghttp2_build
	cd nghttp2_build/nghttp2-${NGHTTP2_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --disable-examples --disable-python-bindings --enable-lib-only && ${MAKE}
	install -D -m444 nghttp2_build/nghttp2-${NGHTTP2_VERSION}/lib/.libs/libnghttp2.a out/lib/libnghttp2.a
	find nghttp2_build/nghttp2-${NGHTTP2_VERSION}/lib/includes/nghttp2 -regex '.*\.h$$' -execdir install -D -m444 '{}' "${PWD}/out/include/nghttp2/{}" ';'
	rm -rf nghttp2_build

download_cache/nghttp2-${NGHTTP2_VERSION}.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${NGHTTP2_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_nghttp2-${NGHTTP2_VERSION}.tar.xz.txt || (rm -f download_cache/nghttp2-${NGHTTP2_VERSION}.tar.xz && false)

out/lib/libidn2.a: download_cache/libidn2-${LIBIDN2_VERSION}.tar.gz out/lib/libunistring.a
	@mkdir -p idn2_build
	tar -xf download_cache/libidn2-${LIBIDN2_VERSION}.tar.gz -C idn2_build
	cd idn2_build/libidn2-${LIBIDN2_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --with-libunistring-prefix="${PWD}/out" && ${MAKE}
	install -D -m444 idn2_build/libidn2-${LIBIDN2_VERSION}/lib/.libs/libidn2.a out/lib/libidn2.a
	install -D -m444 idn2_build/libidn2-${LIBIDN2_VERSION}/lib/idn2.h out/include/idn2.h
	rm -rf idn2_build

download_cache/libidn2-${LIBIDN2_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${LIBIDN2_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_idn2-${LIBIDN2_VERSION}.tar.gz.txt || (rm -f download_cache/libidn2-${LIBIDN2_VERSION}.tar.gz && false)

out/lib/libunistring.a: download_cache/libunistring-${LIBUNISTRING_VERSION}.tar.xz
	@mkdir -p libunistring_build
	tar -xf download_cache/libunistring-${LIBUNISTRING_VERSION}.tar.xz -C libunistring_build
	cd libunistring_build/libunistring-${LIBUNISTRING_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --prefix=/usr
	#patch -p1 < patches/libunistring-${LIBUNISTRING_VERSION}-fix-build.patch
	${MAKE} -C libunistring_build/libunistring-${LIBUNISTRING_VERSION} && ${MAKE} -C libunistring_build/libunistring-${LIBUNISTRING_VERSION} DESTDIR="${PWD}/libunistring_build/libunistring-${LIBUNISTRING_VERSION}/HERE_OUT" install
	install -D -m444 libunistring_build/libunistring-${LIBUNISTRING_VERSION}/lib/.libs/libunistring.a out/lib/libunistring.a
	cd libunistring_build/libunistring-${LIBUNISTRING_VERSION}/HERE_OUT/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf libunistring_build

download_cache/libunistring-${LIBUNISTRING_VERSION}.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${LIBUNISTRING_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_libunistring-${LIBUNISTRING_VERSION}.tar.xz.txt || (rm -f download_cache/libunistring-${LIBUNISTRING_VERSION}.tar.xz && false)

out/lib/libzstd.a: download_cache/zstd-${ZSTD_VERSION}.tar.zst out/lib/libz.a out/lib/liblzma.a out/lib/liblz4.a
	@mkdir -p zstd_build
	tar -xf download_cache/zstd-${ZSTD_VERSION}.tar.zst -C zstd_build
	patch -d zstd_build/zstd-${ZSTD_VERSION} -p1 < ${CURDIR}/zstd_patch_lz4_changes_2026-11-06.patch
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S zstd_build/zstd-${ZSTD_VERSION}/build/cmake -B zstd_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DZSTD_ZLIB_SUPPORT=On -DZSTD_LZMA_SUPPORT=On -DZSTD_LZ4_SUPPORT=On -DZSTD_BUILD_CONTRIB=On -DZSTD_BUILD_STATIC=On -DZSTD_BUILD_SHARED=Off -DZSTD_BUILD_TESTS=Off -DZSTD_PROGRAMS_LINK_SHARED=Off -DZLIB_INCLUDE_DIR="${PWD}/out/include" -DZLIB_LIBRARY="${PWD}/out/lib/libz.a" -DLIBLZMA_INCLUDE_DIR="${PWD}/out/include" -DLIBLZMA_LIBRARY="${PWD}/out/lib/liblzma.a" -DLIBLZ4_INCLUDE_DIR="${PWD}/out/include" -DLIBLZ4_LIBRARY="${PWD}/out/lib/liblz4.a" -DZSTD_BUILD_CONTRIB=Off && ${MAKE} -C zstd_build/build && ${MAKE} DESTDIR="${PWD}/zstd_build/zstd_out" -C zstd_build/build install
	cd zstd_build/zstd_out/usr/lib && find . -name libzstd.a -execdir install -D -m444 '{}' "${PWD}/out/lib/libzstd.a" ';'
	cd zstd_build/zstd_out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf zstd_build

download_cache/zstd-${ZSTD_VERSION}.tar.zst:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${ZSTD_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_zstd-${ZSTD_VERSION}.tar.zst.txt || (rm -f download_cache/zstd-${ZSTD_VERSION}.tar.zst && false)

out/lib/libbrotlicommon.a out/lib/libbrotlidec.a: download_cache/brotli-${BROTLI_VERSION}.tar.gz
	@mkdir -p brotli_build
	tar -xf download_cache/brotli-${BROTLI_VERSION}.tar.gz -C brotli_build
	CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S brotli_build/brotli-${BROTLI_VERSION} -B brotli_build/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBROTLI_DISABLE_TESTS=On && ${MAKE} -C brotli_build/build && ${MAKE} DESTDIR="${PWD}/brotli_build/brotli_out" -C brotli_build/build install
	cd brotli_build/brotli_out/usr/lib && find . -regex '.*\.a$$' -execdir install -D -m444 '{}' "${PWD}/out/lib/{}" ';'
	cd brotli_build/brotli_out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf brotli_build

download_cache/brotli-${BROTLI_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o brotli-${BROTLI_VERSION}.tar.gz -L ${BROTLI_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_brotli-${BROTLI_VERSION}.tar.gz.txt || (rm -f download_cache/brotli-${BROTLI_VERSION}.tar.gz && false)

out/lib/libz.a: download_cache/zlib-${ZLIB_VERSION}.tar.xz
	@mkdir -p zlib_build
	tar -xf download_cache/zlib-${ZLIB_VERSION}.tar.xz -C zlib_build
	cd zlib_build/zlib-${ZLIB_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --prefix=/usr --static && ${MAKE} && ${MAKE} DESTDIR="${PWD}/zlib_build/out/" install
	install -D -m444 zlib_build/out/usr/lib/libz.a out/lib/libz.a
	cd zlib_build/out/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf zlib_build

download_cache/zlib-${ZLIB_VERSION}.tar.xz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${ZLIB_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_zlib-${ZLIB_VERSION}.tar.xz.txt || (rm -f download_cache/zlib-${ZLIB_VERSION}.tar.xz && false)

out/ssl/usr/lib/libssl.a: download_cache/openssl-${OPENSSL_VERSION}.tar.gz
	@mkdir -p openssl_build
	tar -xf download_cache/openssl-${OPENSSL_VERSION}.tar.gz -C openssl_build
	cd openssl_build/openssl-openssl-${OPENSSL_VERSION} && sed -i -e 's|./demoCA|/etc/ssl|' apps/CA.pl.in apps/openssl.cnf
	cd openssl_build/openssl-openssl-${OPENSSL_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./Configure --prefix=/usr --openssldir=/etc/ssl --libdir=lib no-docs no-shared enable-ktls enable-ec_nistp_64_gcc_128 disable-tests linux-${TARGET_ARCH} && ${MAKE} && ${MAKE} DESTDIR="${PWD}/out/ssl" install
	rm -rf openssl_build

download_cache/openssl-${OPENSSL_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -OL ${OPENSSL_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_openssl-${OPENSSL_VERSION}.tar.gz.txt || (rm -f download_cache/openssl-${OPENSSL_VERSION}.tar.gz && false)

out/lib/liblzma.a: download_cache/xz_v${XZ_VERSION}.tar.gz
	@mkdir -p xz_utils_build
	tar -xf download_cache/xz_v${XZ_VERSION}.tar.gz -C xz_utils_build
	cd xz_utils_build/xz-${XZ_VERSION} && ./autogen.sh --no-po4a && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} ./configure --prefix=/usr --disable-rpath --disable-doxygen --build="${BUILD_TRIPLE}" --host="${TARGET_TRIPLE}" --disable-shared --enable-static && ${MAKE} -C src/liblzma
	install -D -m444 xz_utils_build/xz-${XZ_VERSION}/src/liblzma/.libs/liblzma.a out/lib/liblzma.a
	cd xz_utils_build/xz-${XZ_VERSION}/src/liblzma/api && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf xz_utils_build

download_cache/xz_v${XZ_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o xz_v${XZ_VERSION}.tar.gz -L ${XZ_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_xz_v${XZ_VERSION}.tar.gz.txt || (rm -f download_cache/xz_v${XZ_VERSION}.tar.gz && false)

out/lib/liblz4.a: download_cache/lz4_v${LZ4_VERSION}.tar.gz
	@mkdir -p lz4_build
	tar -xf download_cache/lz4_v${LZ4_VERSION}.tar.gz -C lz4_build
	cd lz4_build/lz4-${LZ4_VERSION} && CC=${CC} CXX=${CXX} AR=${AR} RANLIB=${RANLIB} LD=${LD} cmake -S build/cmake -B BUILD -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBUILD_STATIC_LIBS=On -DLZ4_BUILD_CLI=Off && ${MAKE} -C BUILD && ${MAKE} DESTDIR=HERE_OUT -C BUILD install
	find lz4_build/lz4-${LZ4_VERSION}/BUILD/HERE_OUT/usr/lib -name liblz4.a -execdir install -D -m444 '{}' "${PWD}/out/lib/liblz4.a" ';'
	cd lz4_build/lz4-${LZ4_VERSION}/BUILD/HERE_OUT/usr/include && find . -regex '.*\.h$$' -exec install -D -m444 '{}' "${PWD}/out/include/{}" ';'
	rm -rf lz4_build

download_cache/lz4_v${LZ4_VERSION}.tar.gz:
	@mkdir -p $(dir $@)
	cd download_cache && curl -o lz4_v${LZ4_VERSION}.tar.gz -L ${LZ4_DOWNLOAD_URL}
	sha256sum -c SHA256SUM_lz4_v${LZ4_VERSION}.tar.gz.txt || (rm -f download_cache/lz4_v${LZ4_VERSION}.tar.gz && false)

.PHONY: clean sqlite3_TARGET

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
