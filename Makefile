.PHONY: all clean build tests

SRCDIR=src/
CFLAGS:=${CFLAGS} -I${SRCDIR} -g
SOURCES=${SRCDIR}/mig_core.c
HEADERS=${SRCDIR}/mig_core.h


all: build

clean:
	rm -f test tcpchat

build: test

test: ${SOURCES} ${HEADERS} test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o test test.c ${SOURCES}

tchat: ${SOURCES} ${HEADERS} tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat tchat.c ${SOURCES}
