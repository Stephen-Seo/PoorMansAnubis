cmake_minimum_required(VERSION 3.10)
project(PoorMansAnubis C)

set(PoorMansAnubis_VERSION 1.0)

set(PoorMansAnubis_SOURCES
    src/main.c
    src/work.c
    src/base64.c
    third_party/SimpleArchiver/src/data_structures/linked_list.c
    third_party/SimpleArchiver/src/data_structures/chunked_array.c
    third_party/SimpleArchiver/src/data_structures/priority_heap.c
)

add_compile_options(
    -Wall -Wextra -Wpedantic -Wno-missing-braces
    $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
    $<$<CONFIG:DEBUG>:-Og>
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug', none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

add_executable(PoorMansAnubis ${PoorMansAnubis_SOURCES})

target_compile_options(PoorMansAnubis PUBLIC
    -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
    -Werror=format-security
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
    -D_GLIBCXX_ASSERTIONS
    -fstack-clash-protection -fstack-protector-strong
    -Wl,-z,nodlopen -Wl,-z,noexecstack
    -Wl,-z,relro -Wl,-z,now
    -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
    -fPIE -pie
)

target_link_options(PoorMansAnubis PUBLIC
    -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
    -Werror=format-security
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
    -D_GLIBCXX_ASSERTIONS
    -fstack-clash-protection -fstack-protector-strong
    -Wl,-z,nodlopen -Wl,-z,noexecstack
    -Wl,-z,relro -Wl,-z,now
    -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
    -fPIE -pie
)

if(NOT DEFINED DISABLE_FLAGS_FOR_COMPAT)
    target_compile_options(PoorMansAnubis PUBLIC
        -fstrict-flex-arrays=3
    )
    target_link_options(PoorMansAnubis PUBLIC
        -fstrict-flex-arrays=3
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(PoorMansAnubis PUBLIC
        -fno-delete-null-pointer-checks -fno-strict-overflow
        -fno-strict-aliasing
    )
    target_link_options(PoorMansAnubis PUBLIC
        -fno-delete-null-pointer-checks -fno-strict-overflow
        -fno-strict-aliasing
    )
    if(NOT DEFINED DISABLE_FLAGS_FOR_COMPAT)
        target_compile_options(PoorMansAnubis PUBLIC
            -ftrivial-auto-var-init=zero
        )
        target_link_options(PoorMansAnubis PUBLIC
            -ftrivial-auto-var-init=zero
        )
    endif()
endif()

target_include_directories(PoorMansAnubis PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src"
)
