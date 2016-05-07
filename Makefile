.PHONY: all clean build tests

SRCDIR=src
CFLAGS:=${CFLAGS} -I${SRCDIR} -g
CORE_SOURCES=${SRCDIR}/mig_core.c
CORE_HEADERS=${SRCDIR}/mig_core.h

MHTTP_SOURCES=\
${SRCDIR}/mhttp_util.c\
${SRCDIR}/mhttp_range.c
MHTTP_HEADERS=\
${SRCDIR}/mhttp_method.h\
${SRCDIR}/mhttp_util.h\
${SRCDIR}/mhttp_range.h

all: build

clean:
	rm -f midnighttpd tchat test

build: midnighttpd tchat

test: ${CORE_SOURCES} ${CORE_HEADERS} tests/test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o test tests/test.c ${CORE_SOURCES}

tchat: ${CORE_SOURCES} ${CORE_HEADERS} tests/tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat tests/tchat.c ${CORE_SOURCES}

midnighttpd: ${CORE_SOURCES} ${CORE_HEADERS} ${MHTTP_SOURCES} ${MHTTP_HEADERS} src/midnighttpd.c
	${CC} ${LDFLAGS} ${CFLAGS} -o midnighttpd src/midnighttpd.c ${CORE_SOURCES} ${MHTTP_SOURCES}
