cmake_minimum_required(VERSION 3.10)
project(PoorMansAnubis C CXX)

set(PoorMansAnubis_VERSION 1.0)

set(PoorMansAnubis_SOURCES
    src/work.c
    src/base64.c
    src/raylib_test.c
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

add_executable(PoorMansAnubis src/main.c src/work2.cc ${PoorMansAnubis_SOURCES})

add_executable(UnitTest src/test.cc ${PoorMansAnubis_SOURCES})

add_library(PMA_Challenge STATIC src/work2.cc ${PoorMansAnubis_SOURCES})

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

target_compile_options(PMA_Challenge PUBLIC
    -O2 -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
    -Werror=format-security
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
    -D_GLIBCXX_ASSERTIONS
    -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
    -fstack-clash-protection -fstack-protector-strong
    -Wl,-z,nodlopen -Wl,-z,noexecstack
    -Wl,-z,relro -Wl,-z,now
    -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
    -fPIC
)
target_link_options(PMA_Challenge PUBLIC
    -O2 -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough
    -Werror=format-security
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3
    -D_GLIBCXX_ASSERTIONS
    -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST
    -fstack-clash-protection -fstack-protector-strong
    -Wl,-z,nodlopen -Wl,-z,noexecstack
    -Wl,-z,relro -Wl,-z,now
    -Wl,--as-needed -Wl,--no-copy-dt-needed-entries
    -fPIC
)

if(NOT DEFINED DISABLE_FLAGS_FOR_COMPAT)
    target_compile_options(PoorMansAnubis PUBLIC
        -fstrict-flex-arrays=3
    )
    target_link_options(PoorMansAnubis PUBLIC
        -fstrict-flex-arrays=3
    )
    target_compile_options(PMA_Challenge PUBLIC
        -fstrict-flex-arrays=3
    )
    target_link_options(PMA_Challenge PUBLIC
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
    target_compile_options(PMA_Challenge PUBLIC
        -fno-delete-null-pointer-checks -fno-strict-overflow
        -fno-strict-aliasing
    )
    target_link_options(PMA_Challenge PUBLIC
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
        target_compile_options(PMA_Challenge PUBLIC
            -ftrivial-auto-var-init=zero
        )
        target_link_options(PMA_Challenge PUBLIC
            -ftrivial-auto-var-init=zero
        )
    endif()
endif()

target_include_directories(PoorMansAnubis PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src"
)
target_include_directories(UnitTest PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src"
)
target_include_directories(PMA_Challenge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SimpleArchiver/src"
)

if("$ENV{CC}" STREQUAL "")
    set(ENV{CC} "gcc")
endif()
if("$ENV{AR}" STREQUAL "")
    set(ENV{AR} "ar")
endif()

# Raylib for generating images.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a"
    COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/raylib"
    COMMAND test -e "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib.tar.gz" || curl -L -o "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib.tar.gz" "https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz"
    COMMAND tar -xf "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib.tar.gz" -C "${CMAKE_CURRENT_BINARY_DIR}/raylib"
    COMMAND sed -i -e "/^CC/cCC ?= gcc" -e "/^AR/cAR ?= ar" "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src/Makefile"
    COMMAND env PLATFORM=PLATFORM_MEMORY CC=$ENV{CC} AR=$ENV{AR} make -C "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src"
    COMMAND ln -s "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src/libraylib.a" "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a"
    VERBATIM)
add_custom_target(RAYLIB DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a")
set_target_properties(RAYLIB PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/raylib"
)

add_dependencies(PoorMansAnubis RAYLIB)
add_dependencies(UnitTest RAYLIB)
add_dependencies(PMA_Challenge RAYLIB)

target_include_directories(PoorMansAnubis PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src"
)
target_include_directories(UnitTest PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src"
)
target_include_directories(PMA_Challenge PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/raylib/raylib-6.0/src"
)

target_link_libraries(PoorMansAnubis "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a")
target_link_libraries(UnitTest "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a")
target_link_libraries(PMA_Challenge "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a")

install(TARGETS PMA_Challenge)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/raylib/libraylib.a"
    TYPE LIB)
