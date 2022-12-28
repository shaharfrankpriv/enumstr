
TARGET=enumstr

PWD:=${shell pwd -P}
TOOL=./enumstr.py
CFLAGS += -Wall

ENUM_GRAMMER = enumstr.peg
DOT_SCRIPT = peppapeg/gendot.py

PEG_HDRDIR=./peppapeg
PEG_HEADER=peppa.h
PEG_LIBDIR=${PWD}/peppapeg
PEG_LIB=peppa
ARGS_LIBDIR=${PWD}/argparse
ARGS_LIB=argparse

HDRS=peppapeg/peppa.h argparse/argparse.h

enumstr: enumstr.o
	${CC} ${LDFLAGS} -L ${PEG_LIBDIR} -L ${ARGS_LIBDIR} -Wl,-R${PEG_LIBDIR} -Wl,-R${ARGS_LIBDIR} $< -o $@ -l ${PEG_LIB} -l ${ARGS_LIB}

enumstr.o: enumstr.c ${HDRS}
	${CC} ${CFLAGS} -I ${PEG_HDRDIR} -c $< -o $@


# Python section
test: sample
	./sample

sample: sample.c sample.h
	 ${CC} ${CFLAGS} $< -o $@

sample.c: sample.h ${TOOL}
	${TOOL} --sample --use_header $< > $@

sample.h: ${TOOL}
	${TOOL} --sample --dump --gen_header $@ > $@

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
	rm -f sample.c sample.h sample enumstr.o enumstr *.o *.svg *.asl
