CWD != pwd

LINKER_FLAGS := -lsqlite3 -lblake3
COMMON_FLAGS := -std=c++23 -I../c_impl/src -I../c_impl/third_party/SimpleArchiver/src
COMMON_CFLAGS := -I../c_impl/src -I../c_impl/third_party/SimpleArchiver/src

ifdef RELEASE
FINAL_COMMON_FLAGS := -DNDEBUG -O2 ${COMMON_FLAGS}
FINAL_COMMON_CFLAGS := -DNDEBUG -O2 ${COMMON_CFLAGS}
else
FINAL_COMMON_FLAGS := -Og -g ${COMMON_FLAGS}
FINAL_COMMON_CFLAGS := -Og -g ${COMMON_CFLAGS}
endif

OBJDIR := objdir
OBJDIR_ABS := ${CWD}/${OBJDIR}

SOURCES := \
	src/main.cc \
	src/db.cc \
	src/constants.cc
HEADERS := \
	src/db.h \
	src/constants.h

C_IMPL_SOURCES := \
	../c_impl/src/work.c \
	../c_impl/src/base64.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/priority_heap.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/chunked_array.c \
	../c_impl/third_party/SimpleArchiver/src/data_structures/linked_list.c
C_IMPL_OBJECTS := $(subst ../c_impl,${OBJDIR}/dotdot,$(subst .c,.c.o,${C_IMPL_SOURCES}))

OBJECTS := $(addprefix ${OBJDIR_ABS}/,$(subst .cc,.cc.o,${SOURCES}))

all: cxx_backend_impl

cxx_backend_impl: ${OBJECTS} ${C_IMPL_OBJECTS}
	${CXX} -o cxx_backend_impl ${FINAL_COMMON_FLAGS} ${LINKER_FLAGS} $^

${OBJDIR_ABS}/src/%.cc.o: ${CWD}/src/%.cc | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${FINAL_COMMON_FLAGS} $<

${OBJDIR}/dotdot/%.c.o: ../c_impl/%.c
	@mkdir -p $(dir $@)
	${CC} -o $@ -c ${FINAL_COMMON_CFLAGS} $<

.PHONY: clean clang-format

clean:
	rm -rf ${OBJDIR_ABS}
	rm -f cxx_backend_impl

clang-format:
	test -x /usr/bin/clang-format && /usr/bin/clang-format -i --style=Google ${SOURCES} ${HEADERS}
