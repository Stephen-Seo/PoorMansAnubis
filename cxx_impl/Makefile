CWD != pwd

LINKER_FLAGS := ${PMA_PRE_LINKER_FLAGS} -lsqlite3 -lblake3 -lcurl -lcrypto ${PMA_POST_LINKER_FLAGS}
COMMON_FLAGS := ${PMA_PRE_COMMON_FLAGS} -std=c++20 -I../c_impl/src -I../c_impl/third_party/SimpleArchiver/src \
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
COMMON_CFLAGS := ${PMA_PRE_COMMON_CFLAGS} -I../c_impl/src -I../c_impl/third_party/SimpleArchiver/src \
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
	src/db_msql.cc
HEADERS := \
	src/db.h \
	src/constants.h \
	src/http.h \
	src/helpers.h \
	src/poor_mans_print.h \
	src/args.h \
	src/db_msql.h \
	src/db_msql_capi.h

C_IMPL_SOURCES := \
	../c_impl/src/work.c \
	../c_impl/src/base64.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c
C_IMPL_OBJECTS := $(subst ../c_impl,${OBJDIR}/dotdot,$(subst .c,.c.o,${C_IMPL_SOURCES}))

OBJECTS := $(addprefix ${OBJDIR_ABS}/,$(subst .cc,.cc.o,${SOURCES}))

all: cxx_backend_impl test debug_serve debug_client libdb_msql_capi.a

cxx_backend_impl: ${OBJECTS} ${C_IMPL_OBJECTS}
	${CXX} -o cxx_backend_impl ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

test: ${OBJDIR_ABS}/src/test.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${C_IMPL_OBJECTS}
	${CXX} -o test ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

debug_serve: ${OBJDIR_ABS}/debug/test_serve.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${C_IMPL_OBJECTS}
	${CXX} -o debug_serve ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

debug_client: ${OBJDIR_ABS}/debug/test_client.cc.o $(filter-out ${OBJDIR_ABS}/src/main.cc.o,${OBJECTS}) ${C_IMPL_OBJECTS}
	${CXX} -o debug_client ${FINAL_COMMON_FLAGS} $^ ${LINKER_FLAGS}

libdb_msql_capi.a: ${OBJDIR_ABS}/src/db_msql.cc.o
	${AR} rcs libdb_msql_capi.a $^

${OBJDIR_ABS}/src/%.cc.o: ${CWD}/src/%.cc ${HEADERS} | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR_ABS}/debug/%.cc.o: ${CWD}/debug/%.cc ${HEADERS} | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR}/dotdot/%.c.o: ../c_impl/%.c ${HEADERS}
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

clang-format:
	test -x /usr/bin/clang-format && /usr/bin/clang-format -i --style=file ${SOURCES} ${HEADERS} src/test.cc || true
