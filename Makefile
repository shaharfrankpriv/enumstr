
TARGET=enumstr
GLOBAL=${HOME}/work
INSTALL=install
INSTALL_DIR=~/bin

GIT_VERSION=${shell git log --format="%h" -n 1}

PWD:=${shell pwd -P}
TOOL=./enumstr.py
CFLAGS += -Wall -Werror

ENUM_GRAMMER = enumstr.peg
DOT_SCRIPT = ${GLOBAL}/scripts/gendot.py

LIBDIR=${GLOBAL}/lib
INCDIR=${GLOBAL}/include
BASIC_LIB=libbasic
PEG_LIB=peppa
ARGS_LIB=argparse

EXTRA_HEADERS=${INCDIR}/libbasic.h ${INCDIR}/peppa.h ${INCDIR}/argparse.h
HDRS=${EXTRA_HEADERS} enumpeg.h

enumstr: enumstr.o
	${CC} ${LDFLAGS} -L ${LIBDIR} -Wl,-R${LIBDIR} $< -o $@ -l ${PEG_LIB} -l ${ARGS_LIB} -l${BASIC_LIB}


enumstr.o: enumstr.c ${HDRS} Makefile
	${CC} ${CFLAGS} -I ${INCDIR} -DARGPARSE_VERSION="${GIT_VERSION} ${shell date +%F}" -c $< -o $@

enumpeg.h: enumstr.peg
	sed -e 's/\\/\\\\/g;s/"/\\"/g;s/	/\\t/g;s/^/"/;s/$$/\\n"/' $< > $@

.PHONY: install
install: enumstr
	${INSTALL} $< ${INSTALL_DIR}

.PHONY: test
test: enumstr
	cd tests && make

# Peg utilitys
.PHONY: graph
graph: tests/sample.svg
	eog $^

%.svg: %.asl
	rm -f /tmp/$@
	cat $< | python3 ${DOT_SCRIPT} | dot -Tsvg -o$@


%.asl: %.txt ${ENUM_GRAMMER}
	peppa parse -G ${ENUM_GRAMMER} -e SourceFile $< > $@

# General
clean:
	rm -f enumstr.o enumstr *.o *.svg *.asl enumpeg.h
	cd tests && make clean
