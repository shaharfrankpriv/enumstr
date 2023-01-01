TOOL=../enumstr
SRC=./sample.txt
GLOBAL=~/work

INCDIR=${GLOBAL}/include
UNITY_LIB=${GLOBAL}/lib/libunity.a

# Python section
test: sample
	./sample

sample: test_sample.c sample.c sample.h
	 ${CC} -I ${INCDIR} ${CFLAGS} test_sample.c sample.c -o $@ ${UNITY_LIB}

sample.c: ${SRC} sample.h ${TOOL}
	${TOOL} --skip_includes --use_header sample.h  $< > $@

sample.h: ${SRC} ${TOOL}
	${TOOL} --header $@ --dump_enums $< > $@

clean:
	rm -f sample.c sample.h *.o sample sample.svg sample.asl
