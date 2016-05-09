.PHONY: all clean build test

SRCDIR=src
CFLAGS:=${CFLAGS} -I${SRCDIR} -g -pedantic
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
	rm -f midnighttpd tchat mig_test

build: midnighttpd tchat

test:
mig_test: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mig_test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mig_test testprogs/mig_test.c ${CORE_SOURCES}

tchat: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat testprogs/tchat.c ${CORE_SOURCES}

midnighttpd: ${CORE_SOURCES} ${CORE_HEADERS} ${MHTTP_SOURCES} ${MHTTP_HEADERS} src/midnighttpd.c
	${CC} ${LDFLAGS} ${CFLAGS} -o midnighttpd src/midnighttpd.c ${CORE_SOURCES} ${MHTTP_SOURCES}
