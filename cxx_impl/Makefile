CWD != pwd

LINKER_FLAGS := -lsqlite3

ifdef RELEASE
COMMON_FLAGS := -DNDEBUG -O2
else
COMMON_FLAGS := -Og -g
endif

SOURCES := \
	src/main.cc \
	src/db.cc
HEADERS := \
	src/db.h

OBJDIR := ${CWD}/objdir
OBJECTS := $(addprefix ${OBJDIR}/,$(subst .cc,.cc.o,${SOURCES}))

all: cxx_backend_impl

cxx_backend_impl: ${OBJECTS}
	${CXX} -o cxx_backend_impl ${COMMON_FLAGS} ${LINKER_FLAGS} $^

${OBJDIR}/src/%.cc.o: ${CWD}/src/%.cc | clang-format
	@mkdir -p $(dir $@)
	${CXX} -o $@ -c ${COMMON_FLAGS} $<

.PHONY: clean clang-format

clean:
	rm -rf ${OBJDIR}
	rm -f cxx_backend_impl

clang-format:
	test -x /usr/bin/clang-format && /usr/bin/clang-format -i --style=Google ${SOURCES} ${HEADERS}
