
TARGET=enumstr
GLOBAL=${HOME}/work

PWD:=${shell pwd -P}
TOOL=./enumstr.py
CFLAGS += -Wall -Werror

ENUM_GRAMMER = enumstr.peg
DOT_SCRIPT = ${GLOBAL}/scripts/gendot.py

LIBDIR=${GLOBAL}/lib
INCDIR=${GLOBAL}/include
PEG_LIB=peppa
ARGS_LIB=argparse

EXTRA_HEADERS=${INCDIR}/peppa.h ${INCDIR}/argparse.h
HDRS=${EXTRA_HEADERS} enumpeg.h

enumstr: enumstr.o
	${CC} ${LDFLAGS} -L ${LIBDIR} -Wl,-R${LIBDIR} $< -o $@ -l ${PEG_LIB} -l ${ARGS_LIB}


enumstr.o: enumstr.c ${HDRS}
	${CC} ${CFLAGS} -I ${INCDIR} -c $< -o $@

enumpeg.h: enumstr.peg
	sed -e 's/\\/\\\\/g;s/"/\\"/g;s/	/\\t/g;s/^/"/;s/$$/\\n"/' $< > $@

.PHONY: test
test: enumstr
	cd tests && make

# Peg utilitys
.PHONY: graph
graph: enumstr.svg
	eog $^

%.svg: %.asl
	rm -f /tmp/$@
	cat $< | python3 ${DOT_SCRIPT} | dot -Tsvg -o$@


%.asl: %.c ${ENUM_GRAMMER}
	peppa parse -G ${ENUM_GRAMMER} -e SourceFile $< > $@

# General
clean:
	rm -f sample.c sample.h sample enumstr.o enumstr *.o *.svg *.asl enumpeg.h
	cd tests && make clean
