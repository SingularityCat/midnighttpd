.PHONY: all clean build tests

SRCDIR=src/
CFLAGS:=${CFLAGS} -I${SRCDIR} -g
SOURCES=${SRCDIR}/mig_core.c
HEADERS=${SRCDIR}/mig_core.h

all: build

clean:
	rm -f midnighttpd tchat test

build: midnighttpd tchat

test: ${SOURCES} ${HEADERS} tests/test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o test tests/test.c ${SOURCES}

tchat: ${SOURCES} ${HEADERS} tests/tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat tests/tchat.c ${SOURCES}

midnighttpd: ${SOURCES} ${HEADERS} src/midnighttpd.c
	${CC} ${LDFLAGS} ${CFLAGS} -o midnighttpd src/midnighttpd.c ${SOURCES}
