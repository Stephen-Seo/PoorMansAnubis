cmake_minimum_required(VERSION 3.31)
project(poor_mans_anubis C CXX)

if(NOT DISABLE_NOT_BUNDLED_NOTICE)
    message(WARNING "Note that this CMakeLists.txt builds cxx_impl NOT bundled!")
endif()

set(poor_mans_anubis_bundled_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/db.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/constants.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/http.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/helpers.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/poor_mans_print.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/args.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/db_msql.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thread_pool.cc"
)

set(challenge_impl_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src/work.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src/base64.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src/work2.cc"
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    message(STATUS "CMAKE_BUILD_TYPE not specified, defaulting to \"Debug\".")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
else()
    message(STATUS "CMAKE_BUILD_TYPE is set to \"${CMAKE_BUILD_TYPE}\".")
endif()

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

find_package(blake3 REQUIRED)
find_package(openssl REQUIRED NAMES OpenSSL openssl)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
pkg_check_modules(LIBCURL REQUIRED libcurl)

add_executable(poor_mans_anubis
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(poor_mans_anubis PUBLIC cxx_std_23)
target_include_directories(poor_mans_anubis PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src"
	${SQLITE3_INCLUDE_DIRS}
	${LIBCURL_INCLUDE_DIRS}
)
target_link_directories(poor_mans_anubis PUBLIC ${SQLITE3_LIBRARY_DIRS} ${LIBCURL_LIBRARY_DIRS})
target_link_libraries(poor_mans_anubis BLAKE3::blake3 ${SQLITE3_LIBRARIES} ${LIBCURL_LIBRARIES} OpenSSL::Crypto)

add_executable(unittest
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(unittest PUBLIC cxx_std_23)
target_include_directories(unittest PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src"
	${SQLITE3_INCLUDE_DIRS}
	${LIBCURL_INCLUDE_DIRS}
)
target_link_directories(unittest PUBLIC ${SQLITE3_LIBRARY_DIRS})
target_link_libraries(unittest BLAKE3::blake3 ${SQLITE3_LIBRARIES} OpenSSL::Crypto)

add_executable(debug_serve
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/test_serve.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(debug_serve PUBLIC cxx_std_23)
target_include_directories(debug_serve PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src"
)
target_link_directories(debug_serve PUBLIC ${SQLITE3_LIBRARY_DIRS})
target_link_libraries(debug_serve BLAKE3::blake3 ${SQLITE3_LIBRARIES} OpenSSL::Crypto)

add_executable(debug_client
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/test_client.cc"
    ${poor_mans_anubis_bundled_SOURCES} ${challenge_impl_SOURCES}
)
target_compile_features(debug_client PUBLIC cxx_std_23)
target_include_directories(debug_client PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/../challenge_impl/third_party/SimpleArchiver/src"
)
target_link_directories(debug_client PUBLIC ${SQLITE3_LIBRARY_DIRS})
target_link_libraries(debug_client BLAKE3::blake3 ${SQLITE3_LIBRARIES} OpenSSL::Crypto)
