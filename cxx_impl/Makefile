CWD != pwd

LINKER_FLAGS := ${PMA_PRE_LINKER_FLAGS} -lsqlite3 -lblake3 -lcurl -lcrypto ${PMA_POST_LINKER_FLAGS}
COMMON_FLAGS := ${PMA_PRE_COMMON_FLAGS} -std=c++20 -I../challenge_impl/src -I../challenge_impl/third_party/SimpleArchiver/src \
	-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough \
	-Werror=format-security \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	-D_GLIBCXX_ASSERTIONS \
	-fstrict-flex-arrays=3 \
	-fstack-clash-protection -fstack-protector-strong \
	-Wl,-z,nodlopen -Wl,-z,noexecstack \
	-Wl,-z,relro -Wl,-z,now \
	-Wl,--as-needed -Wl,--no-copy-dt-needed-entries \
	-Wtrampolines -fzero-init-padding-bits=all \
	-Wbidi-chars=any \
	-fPIE -pie
COMMON_CFLAGS := ${PMA_PRE_COMMON_CFLAGS} -I../challenge_impl/src -I../challenge_impl/third_party/SimpleArchiver/src \
	-Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough \
	-Werror=format-security \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	-D_GLIBCXX_ASSERTIONS \
	-fstrict-flex-arrays=3 \
	-fstack-clash-protection -fstack-protector-strong \
	-Wl,-z,nodlopen -Wl,-z,noexecstack \
	-Wl,-z,relro -Wl,-z,now \
	-Wl,--as-needed -Wl,--no-copy-dt-needed-entries \
	-Wtrampolines -fzero-init-padding-bits=all \
	-Wbidi-chars=any \
	-fPIE -pie

ifdef CI_BUILD
FINAL_COMMON_FLAGS := -Og -g $(filter-out -fzero-init-padding-bits=all,${COMMON_FLAGS})
FINAL_COMMON_CFLAGS := -Og -g $(filter-out -fzero-init-padding-bits=all,${COMMON_CFLAGS})
else ifdef RELEASE
FINAL_COMMON_FLAGS := -DNDEBUG -O2 ${COMMON_FLAGS} \
	-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing \
	-ftrivial-auto-var-init=zero
FINAL_COMMON_CFLAGS := -DNDEBUG -O2 ${COMMON_CFLAGS} \
	-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing \
	-ftrivial-auto-var-init=zero
else ifdef NOOPT
FINAL_COMMON_FLAGS := -O0 -g $(filter-out -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3,${COMMON_FLAGS}) -Werror
FINAL_COMMON_CFLAGS := -O0 -g $(filter-out -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3,${COMMON_CFLAGS}) -Werror
else
FINAL_COMMON_FLAGS := -Og -g ${COMMON_FLAGS} -Werror
FINAL_COMMON_CFLAGS := -Og -g ${COMMON_CFLAGS} -Werror
endif

OBJDIR := objdir
OBJDIR_ABS := ${CWD}/${OBJDIR}

SOURCES := \
	src/main.cc \
	src/db.cc \
	src/constants.cc \
	src/http.cc \
	src/helpers.cc \
	src/poor_mans_print.cc \
	src/args.cc \
	src/db_msql.cc \
	src/thread_pool.cc
HEADERS := \
	src/db.h \
	src/constants.h \
	src/http.h \
	src/helpers.h \
	src/poor_mans_print.h \
	src/args.h \
	src/db_msql.h \
	src/db_msql_capi.h \
	src/thread_pool.h

CHALL_IMPL_SOURCES := \
	../challenge_impl/src/work.c \
	../challenge_impl/src/base64.c \
	../challenge_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c \
	../challenge_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c \
	../challenge_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c \
	../challenge_impl/src/work2.cc
CHALL_IMPL_OBJECTS := \
	$(subst ../challenge_impl,${OBJDIR_ABS}/dotdot,$(subst .c,.c.o,$(filter %.c,${CHALL_IMPL_SOURCES}))) \
	$(subst ../challenge_impl,${OBJDIR_ABS}/dotdot,$(subst .cc,.cc.o,$(filter %.cc,${CHALL_IMPL_SOURCES}))) \

OBJECTS := $(addprefix ${OBJDIR_ABS}/,$(subst .cc,.cc.o,${SOURCES}))

all: cxx_backend_impl test debug_serve debug_client libdb_msql_capi.a debug_test_capi

cxx_backend_impl: ${OBJECTS} ${CHALL_IMPL_OBJECTS}
	${CXX} -o cxx_backend_impl ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

test: ${OBJDIR_ABS}/src/test.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${CHALL_IMPL_OBJECTS}
	${CXX} -o test ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

debug_serve: ${OBJDIR_ABS}/debug/test_serve.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${CHALL_IMPL_OBJECTS}
	${CXX} -o debug_serve ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

debug_client: ${OBJDIR_ABS}/debug/test_client.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${CHALL_IMPL_OBJECTS}
	${CXX} -o debug_client ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

bundled/out/ssl/usr/lib/libssl.a:
	${MAKE} -C bundled out/ssl/usr/lib/libssl.a

bundled/out/include/sqlite3.h:
	${MAKE} -C bundled out/include/sqlite3.h

${OBJDIR_ABS}/src/db_msql.cc.o: bundled/out/include/sqlite3.h

${OBJDIR_ABS}/src/helpers.cc.o: bundled/out/ssl/usr/lib/libssl.a

libdb_msql_capi.a: ${OBJDIR_ABS}/src/db_msql.cc.o ${OBJDIR_ABS}/src/http.cc.o ${OBJDIR_ABS}/src/helpers.cc.o ${OBJDIR_ABS}/src/poor_mans_print.cc.o ${CHALL_IMPL_OBJECTS}
	cd bundled/out/ssl/usr/lib && mkdir -p crypto_out && ${AR} --output crypto_out x libcrypto.a
	${AR} rcs libdb_msql_capi.a $^ bundled/out/ssl/usr/lib/crypto_out/*

debug_test_capi: ${OBJDIR}/src/db_msql_capi_test.c.o libdb_msql_capi.a
	${CXX} -o debug_test_capi ${FINAL_COMMON_FLAGS} $< -L. -ldb_msql_capi ${LINKER_FLAGS}

${OBJDIR_ABS}/src/%.cc.o: ${CWD}/src/%.cc ${HEADERS} | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR_ABS}/debug/%.cc.o: ${CWD}/debug/%.cc ${HEADERS} | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR_ABS}/dotdot/%.c.o: ../challenge_impl/%.c ${HEADERS}
	@mkdir -p $(dir $@)
	${CC} -o $@ -c ${FINAL_COMMON_CFLAGS} $<

${OBJDIR_ABS}/dotdot/%.cc.o: ../challenge_impl/%.cc ${HEADERS}
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR}/src/%.c.o: ${CWD}/src/%.c ${HEADERS} | clang-format
	@mkdir -p $(dir $@)
	${CC} -o $@ -c ${FINAL_COMMON_CFLAGS} $<

.PHONY: clean clang-format

clean:
	rm -rf ${OBJDIR_ABS}
	rm -f cxx_backend_impl
	rm -f test
	rm -f debug_serve
	rm -f debug_client
	rm -f libdb_msql_capi.a
	rm -f debug_test_capi
	rm -rf bundled/out
	rm -rf bundled/sqlite_build
	rm -rf bundled/blake3_build
	rm -rf bundled/curl_build
	rm -rf bundled/nghttp2_build
	rm -rf bundled/idn2_build
	rm -rf bundled/libunistring_build
	rm -rf bundled/zstd_build
	rm -rf bundled/brotli_build
	rm -rf bundled/zlib_build
	rm -rf bundled/openssl_build
	rm -rf bundled/xz_utils_build
	rm -rf bundled/lz4_build

clang-format:
	test -x /usr/bin/clang-format && /usr/bin/clang-format -i --style=file ${SOURCES} ${HEADERS} src/db_msql_capi_test.c src/test.cc || true
