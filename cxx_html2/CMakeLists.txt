cmake_minimum_required(VERSION 3.31)
project(CXX_HTTP2 CXX)

set(CXX_HTTP2_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc
)

add_executable(cxx_http2 ${CXX_HTTP2_SOURCES})

if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "Setting build type to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Set build type" FORCE)
endif()

message(STATUS "Build type set to ${CMAKE_BUILD_TYPE}")

target_compile_features(cxx_http2 PUBLIC cxx_std_23)

target_compile_options(cxx_http2 PUBLIC
-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
-Werror=format-security
-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
-D_GLIBCXX_ASSERTIONS
-fstrict-flex-arrays=3
-fstack-clash-protection -fstack-protector-strong
-Wl,-z,nodlopen -Wl,-z,noexecstack
-Wl,-z,relro -Wl,-z,now
-Wl,--as-needed -Wl,--no-copy-dt-needed-entries

-fPIE -pie

$<$<STREQUAL:${CMAKE_BUILD_TYPE},Release>:-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing -ftrivial-auto-var-init=zero -O2 -DNDEBUG>
$<$<STREQUAL:${CMAKE_BUILD_TYPE},Debug>:-Og -g>
)
