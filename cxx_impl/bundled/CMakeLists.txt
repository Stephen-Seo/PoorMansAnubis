cmake_minimum_required(VERSION 3.31)
project(poor_mans_anubis C CXX)

if(NOT DISABLE_BUNDLED_NOTICE)
    message(WARNING "Note that this CMakeLists.txt builds cxx_impl BUNDLED!")
endif()

if(NOT DEFINED BUILD_CC)
    if("$ENV{CC}" STREQUAL "")
        set(BUILD_CC "/usr/bin/gcc")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_CC to /usr/bin/gcc")
        endif()
    else()
        set(BUILD_CC "$ENV{CC}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_CC to ${BUILD_CC}")
        endif()
    endif()
endif()

if(NOT DEFINED BUILD_CXX)
    if("$ENV{CXX}" STREQUAL "")
        set(BUILD_CXX "/usr/bin/g++")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_CXX to /usr/bin/g++")
        endif()
    else()
        set(BUILD_CXX "$ENV{CXX}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_CXX to ${BUILD_CXX}")
        endif()
    endif()
endif()

if(NOT DEFINED BUILD_AR)
    if("$ENV{AR}" STREQUAL "")
        set(BUILD_AR "/usr/bin/ar")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_AR to /usr/bin/ar")
        endif()
    else()
        set(BUILD_AR "$ENV{AR}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_AR to ${BUILD_AR}")
        endif()
    endif()
endif()

if(NOT DEFINED BUILD_RANLIB)
    if("$ENV{RANLIB}" STREQUAL "")
        set(BUILD_RANLIB "/usr/bin/ranlib")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_RANLIB to /usr/bin/ranlib")
        endif()
    else()
        set(BUILD_RANLIB "$ENV{RANLIB}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_RANLIB to ${BUILD_RANLIB}")
        endif()
    endif()
endif()

if(NOT DEFINED BUILD_LD)
    if("$ENV{LD}" STREQUAL "")
        set(BUILD_LD "/usr/bin/ld")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_LD to /usr/bin/ld")
        endif()
    else()
        set(BUILD_LD "$ENV{LD}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_LD to ${BUILD_LD}")
        endif()
    endif()
endif()

# System we are building on
if(NOT DEFINED BUILD_TRIPLE)
    if("$ENV{BUILD_TRIPLE}" STREQUAL "")
        set(BUILD_TRIPLE "x86_64-pc-linux-gnu")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting BUILD_TRIPLE to x86_64-pc-linux-gnu")
        endif()
    else()
        set(BUILD_TRIPLE "$ENV{BUILD_TRIPLE}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set BUILD_TRIPLE to ${BUILD_TRIPLE}")
        endif()
    endif()
endif()

# System we are building for
if(NOT DEFINED TARGET_TRIPLE)
    if("$ENV{TARGET_TRIPLE}" STREQUAL "")
        set(TARGET_TRIPLE "x86_64-pc-linux-gnu")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting TARGET_TRIPLE to x86_64-pc-linux-gnu")
        endif()
    else()
        set(TARGET_TRIPLE "$ENV{TARGET_TRIPLE}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set TARGET_TRIPLE to ${TARGET_TRIPLE}")
        endif()
    endif()
endif()

# Note that aarch64 linux is typically: aarch64-linux-gnu

if(NOT DEFINED TARGET_ARCH)
    if("$ENV{TARGET_ARCH}" STREQUAL "")
        set(TARGET_ARCH "x86_64")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(WARNING "Defaulting TARGET_ARCH to x86_64")
        endif()
    else()
        set(TARGET_ARCH "$ENV{TARGET_ARCH}")
        if(NOT DISABLE_BUILD_VAR_SET_MSG)
            message(STATUS "Set TARGET_ARCH to ${TARGET_ARCH}")
        endif()
    endif()
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    message(STATUS "CMAKE_BUILD_TYPE not specified, defaulting to \"Debug\".")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
else()
    message(STATUS "CMAKE_BUILD_TYPE is set to \"${CMAKE_BUILD_TYPE}\".")
endif()

set(poor_mans_anubis_bundled_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/db.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/constants.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/http.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/helpers.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/poor_mans_print.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/args.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/db_msql.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/thread_pool.cc"
)

set(challenge_impl_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src/work.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src/base64.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src/work2.cc"
)

add_compile_options(
$<IF:$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>,-Og,-O2>
$<IF:$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>,-g,-DNDEBUG>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-Werror=format-security
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-Wl,-z,nodlopen -Wl,-z,noexecstack
-Wl,-z,relro -Wl,-z,now
-Wl,--as-needed -Wl,--no-copy-dt-needed-entries
-Wtrampolines -fzero-init-padding-bits=all
-fPIE -pie
)
add_link_options(
$<IF:$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>,-Og,-O2>
$<IF:$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>,-g,-DNDEBUG>
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-Werror=format-security
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-Wl,-z,nodlopen -Wl,-z,noexecstack
-Wl,-z,relro -Wl,-z,now
-Wl,--as-needed -Wl,--no-copy-dt-needed-entries
-Wtrampolines -fzero-init-padding-bits=all
-fPIE -pie
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "DEBUG")
    add_compile_options(
        -Werror
    )
    add_link_options(
        -Werror
    )
else()
    add_compile_options(
        -fno-delete-null-pointer-checks -fno-strict-overflow
        -fno-strict-aliasing -ftrivial-auto-var-init=zero
    )
    add_link_options(
        -fno-delete-null-pointer-checks -fno-strict-overflow
        -fno-strict-aliasing -ftrivial-auto-var-init=zero
    )
endif()

include_directories("${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include")
link_directories("${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib")

set_property(DIRECTORY PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundled_build;${CMAKE_CURRENT_BINARY_DIR}/bundled_out")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite/sqlite-autoconf-3510300/"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/sqlite-autoconf-3510300.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/sqlite-autoconf-3510300.tar.gz" -L "https://sqlite.org/2026/sqlite-autoconf-3510300.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_sqlite-3.51.3.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/sqlite-autoconf-3510300.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite"
    VERBATIM
)
add_custom_target(SQLITE3_BINARY_DIR DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite/sqlite-autoconf-3510300/")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/sqlite3.h"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite/sqlite-autoconf-3510300/sqlite3.h" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/sqlite3.h"
    VERBATIM
)
add_custom_target(SQLITE3_HEADER DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/sqlite3.h")
add_dependencies(SQLITE3_HEADER SQLITE3_BINARY_DIR)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libsqlite3.a"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite/sqlite-autoconf-3510300" && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --disable-shared && make libsqlite3.a
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/sqlite/sqlite-autoconf-3510300/libsqlite3.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libsqlite3.a"
    VERBATIM
)
add_custom_target(SQLITE3_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libsqlite3.a")
add_dependencies(SQLITE3_LIB SQLITE3_BINARY_DIR)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libblake3.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/blake3_1.8.4.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/blake3_1.8.4.tar.gz" -L "https://github.com/BLAKE3-team/BLAKE3/archive/refs/tags/1.8.4.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_blake3_1.8.4.tar.gz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/blake3_1.8.4.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3"
    COMMAND env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} cmake -S "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3/BLAKE3-1.8.4/c" -B "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} -DCMAKE_C_COMPILER_TARGET=${TARGET_ARCH} -DCMAKE_CXX_COMPILER_TARGET=${TARGET_ARCH} "$<$<STREQUAL:aarch64,${TARGET_ARCH}>:-DBLAKE3_SIMD_TYPE=neon-intrinsics>"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3/build"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3/build/libblake3.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libblake3.a"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/blake3/BLAKE3-1.8.4/c/blake3.h" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/blake3.h"
    VERBATIM
)
add_custom_target(BLAKE3_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libblake3.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libnghttp2.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/nghttp2-1.68.1.tar.xz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/nghttp2-1.68.1.tar.xz" -L "https://github.com/nghttp2/nghttp2/releases/download/v1.68.1/nghttp2-1.68.1.tar.xz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_nghttp2-1.68.1.tar.xz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/nghttp2-1.68.1.tar.xz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2/nghttp2-1.68.1" && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --disable-examples --disable-python-bindings --enable-lib-only
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2/nghttp2-1.68.1"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2/nghttp2-1.68.1/lib/.libs/libnghttp2.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libnghttp2.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/nghttp2"
    COMMAND find "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/nghttp2/nghttp2-1.68.1/lib/includes/nghttp2" -regex .*\.h$ -execdir install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/nghttp2/{}" ";"
    VERBATIM
)
add_custom_target(NGHTTP2_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libnghttp2.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libunistring.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libunistring-1.4.2.tar.xz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libunistring-1.4.2.tar.xz" -L "https://mirrors.ocf.berkeley.edu/gnu/libunistring/libunistring-1.4.2.tar.xz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_libunistring-1.4.2.tar.xz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libunistring-1.4.2.tar.xz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/libunistring-1.4.2" && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --prefix=/usr
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/libunistring-1.4.2"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/libunistring-1.4.2" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/INSTALL_OUT install
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/INSTALL_OUT/usr/lib/libunistring.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libunistring.a"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libunistring/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBUNISTRING_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libunistring.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libidn2.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libidn2-2.3.8.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libidn2-2.3.8.tar.gz" -L "https://mirrors.ocf.berkeley.edu/gnu/libidn/libidn2-2.3.8.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c SHA256SUM_idn2-2.3.8.tar.gz.txt
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/libidn2-2.3.8.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2/libidn2-2.3.8" && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --enable-static --disable-shared --with-unistring-prefix="${CMAKE_CURRENT_BINARY_DIR}/bundled_out"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2/libidn2-2.3.8"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2/libidn2-2.3.8/lib/.libs/libidn2.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libidn2.a"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/libidn2/libidn2-2.3.8/lib/idn2.h" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/idn2.h"
    VERBATIM
)
add_custom_target(LIBIDN2_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libidn2.a")
add_dependencies(LIBIDN2_LIB LIBUNISTRING_LIB)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libz.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zlib-1.3.2.tar.xz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zlib-1.3.2.tar.xz" -L "https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.xz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_zlib-1.3.2.tar.xz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zlib-1.3.2.tar.xz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/zlib-1.3.2" && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --prefix=/usr --static
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/zlib-1.3.2"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/zlib-1.3.2" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/INSTALL_OUT install
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/INSTALL_OUT/usr/lib/libz.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libz.a"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zlib/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBZ_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libz.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblzma.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/xz_v5.8.3.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/xz_v5.8.3.tar.gz" -L "https://github.com/tukaani-project/xz/archive/refs/tags/v5.8.3.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_xz_v5.8.3.tar.gz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/xz_v5.8.3.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma/xz-5.8.3" && ./autogen.sh --no-po4a && CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./configure --prefix=/usr --disable-rpath --disable-doxygen --build=${BUILD_TRIPLE} --host=${TARGET_TRIPLE} --disable-shared --enable-static
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma/xz-5.8.3"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma/xz-5.8.3/src/liblzma/.libs/liblzma.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblzma.a"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/xz_lzma/xz-5.8.3/src/liblzma/api" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBLZMA_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblzma.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblz4.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/lz4_v1.10.0.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/lz4_v1.10.0.tar.gz" -L "https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c "SHA256SUM_lz4_v1.10.0.tar.gz.txt"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/lz4_v1.10.0.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4"
    COMMAND env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} cmake -S "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/lz4-1.10.0/build/cmake" -B "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/BUILD" -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBUILD_STATIC_LIBS=On -DLZ4_BUILD_CLI=Off
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/BUILD"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/BUILD" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/INSTALL_OUT install
    COMMAND find "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/INSTALL_OUT" -name liblz4.a -execdir install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblz4.a" ";"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/lz4/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBLZ4_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblz4.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libzstd.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zstd-1.5.7.tar.zst" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zstd-1.5.7.tar.zst" -L "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.zst"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c SHA256SUM_zstd-1.5.7.tar.zst.txt
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/zstd-1.5.7.tar.zst" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd"
    COMMAND env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} cmake -S "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/zstd-1.5.7/build/cmake" -B "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DZSTD_ZLIB_SUPPORT=On -DZSTD_LZMA_SUPPORT=On -DZSTD_LZ4_SUPPORT=On -DZSTD_BUILD_CONTRIB=On -DZSTD_BUILD_STATIC=On -DZSTD_BUILD_SHARED=Off -DZSTD_BUILD_TESTS=Off -DZSTD_PROGRAMS_LINK_SHARED=Off -DZLIB_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DZLIB_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libz.a -DLIBLZMA_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DLIBLZMA_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblzma.a -DLIBLZ4_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DLIBLZ4_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/liblz4.a -DZSTD_BUILD_CONTRIB=Off
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/BUILD"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/BUILD" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/INSTALL_OUT install
    COMMAND find "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/INSTALL_OUT/usr/lib" -name libzstd.a -execdir install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libzstd.a" ";"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/zstd/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBZSTD_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libzstd.a")
add_dependencies(LIBZSTD_LIB
    LIBZ_LIB
    LIBLZMA_LIB
    LIBLZ4_LIB
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libbrotlicommon.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/brotli-1.2.0.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/brotli-1.2.0.tar.gz" -L "https://github.com/google/brotli/archive/refs/tags/v1.2.0.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c SHA256SUM_brotli-1.2.0.tar.gz.txt
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/brotli-1.2.0.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli"
    COMMAND env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} cmake -S "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/brotli-1.2.0" -B "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=Off -DBROTLI_DISABLE_TESTS=On
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/BUILD"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/BUILD" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/INSTALL_OUT install
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/INSTALL_OUT/usr/lib" && find . -regex .*\.a$ -execdir install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/{}" ";"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/brotli/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBBROTLI_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libbrotlicommon.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libssl.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/openssl-3.6.2.tar.gz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/openssl-3.6.2.tar.gz" -L "https://github.com/openssl/openssl/releases/download/openssl-3.6.2/openssl-3.6.2.tar.gz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c SHA256SUM_openssl-3.6.2.tar.gz.txt
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/openssl-3.6.2.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/openssl-3.6.2" && sed -i -e s|./demoCA|/etc/ssl| apps/CA.pl.in apps/openssl.cnf && env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} ./Configure --prefix=/usr --openssldir=/etc/ssl --libdir=lib no-docs no-shared enable-ktls enable-ec_nistp_64_gcc_128 disable-tests linux-${TARGET_ARCH}
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/openssl-3.6.2"
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/openssl-3.6.2" DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/INSTALL_OUT install
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/INSTALL_OUT/usr/lib/libssl.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libssl.a"
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/INSTALL_OUT/usr/lib/libcrypto.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcrypto.a"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/openssl/INSTALL_OUT/usr/include" && find . -regex .*\.h$ -exec install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/{}" ";"
    VERBATIM
)
add_custom_target(LIBOPENSSL_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libssl.a")

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcurl.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_SOURCE_DIR}/download_cache"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/curl-8.19.0.tar.xz" || curl -o "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/curl-8.19.0.tar.xz" -L "https://github.com/curl/curl/releases/download/curl-8_19_0/curl-8.19.0.tar.xz"
    COMMAND cd "${CMAKE_CURRENT_SOURCE_DIR}" && sha256sum -c SHA256SUM_curl-8.19.0.txt
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/download_cache/curl-8.19.0.tar.xz" -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl"
    COMMAND env CC=${BUILD_CC} CXX=${BUILD_CXX} AR=${BUILD_AR} RANLIB=${BUILD_RANLIB} LD=${BUILD_LD} cmake -S "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl/curl-8.19.0" -B "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl/BUILD" -DBUILD_SHARED_LIBS=Off -DBUILD_LIBCURL_DOCS=Off -DBUILD_MISC_DOCS=Off -DBUILD_STATIC_LIBS=On -DBUILD_TESTING=Off -DENABLE_CURL_MANUAL=Off -DCURL_DEFAULT_SSL_BACKEND=openssl -DCURL_USE_OPENSSL=On -DOPENSSL_ROOT_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out -DOPENSSL_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DOPENSSL_SSL_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libssl.a -DOPENSSL_CRYPTO_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcrypto.a -DOPENSSL_USE_STATIC_LIBS=On -DZLIB_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DZLIB_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libz.a -DZLIB_USE_STATIC_LIBS=On -DBROTLI_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DBROTLICOMMON_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libbrotlicommon.a -DBROTLIDEC_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libbrotlidec.a -DZSTD_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DZSTD_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libzstd.a -DNGHTTP2_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DNGHTTP2_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libnghttp2.a -DCURL_DISABLE_LDAP=On -DLIBIDN2_INCLUDE_DIR=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include -DLIBIDN2_LIBRARY=${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libidn2.a -DCURL_USE_LIBPSL=Off -DCURL_USE_LIBSSH2=Off
    COMMAND make -C "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl/BUILD" libcurl_static
    COMMAND install -D -m444 "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl/BUILD/lib/libcurl.a" "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcurl.a"
    COMMAND find "${CMAKE_CURRENT_BINARY_DIR}/bundled_build/curl/curl-8.19.0/include/curl" -regex .*\.h$ -execdir install -D -m444 {} "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/include/curl/{}" ";"
    VERBATIM
)
add_custom_target(LIBCURL_LIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcurl.a")
add_dependencies(LIBCURL_LIB
    NGHTTP2_LIB
    LIBIDN2_LIB
    LIBZSTD_LIB
    LIBBROTLI_LIB
    LIBOPENSSL_LIB
)

add_executable(poor_mans_anubis_bundled
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(poor_mans_anubis_bundled PUBLIC cxx_std_23)
target_include_directories(poor_mans_anubis_bundled PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src"
)
add_dependencies(poor_mans_anubis_bundled
    SQLITE3_LIB
    SQLITE3_HEADER
    BLAKE3_LIB
    LIBCURL_LIB
)
target_link_libraries(poor_mans_anubis_bundled
    sqlite3
    blake3
    curl
    nghttp2
    idn2
    unistring
    zstd
    lzma
    lz4
    brotlidec
    brotlicommon
    z
    ssl
    crypto
)

add_executable(unittest_bundled
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/test.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(unittest_bundled PUBLIC cxx_std_23)
target_include_directories(unittest_bundled PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src"
)
target_link_libraries(unittest_bundled
    sqlite3
    blake3
    crypto
)
add_dependencies(unittest_bundled
    SQLITE3_LIB
    SQLITE3_HEADER
    BLAKE3_LIB
    LIBOPENSSL_LIB
)

add_executable(debug_serve_bundled
    "${CMAKE_CURRENT_SOURCE_DIR}/../debug/test_serve.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(debug_serve_bundled PUBLIC cxx_std_23)
target_include_directories(debug_serve_bundled PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src"
)
target_link_libraries(debug_serve_bundled
    sqlite3
    blake3
    crypto
)
add_dependencies(debug_serve_bundled
    SQLITE3_LIB
    SQLITE3_HEADER
    BLAKE3_LIB
    LIBOPENSSL_LIB
)

add_executable(debug_client_bundled
    "${CMAKE_CURRENT_SOURCE_DIR}/../debug/test_client.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(debug_client_bundled PUBLIC cxx_std_23)
target_include_directories(debug_client_bundled PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src"
)
target_link_libraries(debug_client_bundled
    sqlite3
    blake3
    crypto
)
add_dependencies(debug_client_bundled
    SQLITE3_LIB
    SQLITE3_HEADER
    BLAKE3_LIB
    LIBOPENSSL_LIB
)

add_library(db_msql_api_pre STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/db_msql.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/http.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/helpers.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/poor_mans_print.cc"
    ${challenge_impl_SOURCES}
)
target_compile_features(db_msql_api_pre PUBLIC cxx_std_23)
target_include_directories(db_msql_api_pre PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../challenge_impl/third_party/SimpleArchiver/src"
)
add_dependencies(db_msql_api_pre
    SQLITE3_HEADER
    LIBOPENSSL_LIB
)

add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/libdb_msql_capi.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/db_msql_api_OBJECTS"
    COMMAND cd "${CMAKE_CURRENT_BINARY_DIR}/db_msql_api_OBJECTS" && ${BUILD_AR} x "${CMAKE_CURRENT_BINARY_DIR}/libdb_msql_api_pre.a" && ${BUILD_AR} x "${CMAKE_CURRENT_BINARY_DIR}/bundled_out/lib/libcrypto.a"
    COMMAND sh -c "${BUILD_AR} rcs \"${CMAKE_CURRENT_BINARY_DIR}/libdb_msql_capi.a\" $(find \"${CMAKE_CURRENT_BINARY_DIR}/db_msql_api_OBJECTS\" -type f)"
    VERBATIM
)
add_custom_target(LIBDB_MSQL_API_FINALIZED DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libdb_msql_capi.a")
add_dependencies(LIBDB_MSQL_API_FINALIZED
    db_msql_api_pre
)
set_property(TARGET LIBDB_MSQL_API_FINALIZED PROPERTY EXCLUDE_FROM_ALL OFF)
