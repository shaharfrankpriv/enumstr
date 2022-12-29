
GLOBAL=${HOME}/work

PWD:=${shell pwd -P}
TOOL=./enumstr.py
CFLAGS += -Wall -Werror

ENUM_GRAMMER = enumstr.peg
DOT_SCRIPT = peppapeg/gendot.py

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
