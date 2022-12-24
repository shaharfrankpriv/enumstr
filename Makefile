
TOOL=./enumstr.py

PEG_HEADERS=/usr/local/include/peppa.h
PEG_LIBDIR=/usr/local/lib/
PEG_LIB=peppa

test: sample
	./sample

sample: sample.c sample.h
	 ${CC} ${CFLAGS} $< -o $@

sample.c: sample.h ${TOOL}
	${TOOL} --sample --use_header $< > $@

sample.h: ${TOOL}
	${TOOL} --sample --dump --gen_header $@ > $@

enumstr: enumstr.o
	${CC} ${LDFLAGS} -L ${PEG_LIB}  $< -o $@ -l ${PEG_LIB}

enumstr.o: enumstr.c ${PEG_HEADERS}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f sample.c sample.h sample enumstr.o enumstr
