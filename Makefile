.PHONY: all clean build test

SRCDIR=src
CFLAGS:=${CFLAGS} -I${SRCDIR} -g -pedantic
CORE_SOURCES=${SRCDIR}/mig_core.c ${SRCDIR}/mig_opt.c
CORE_HEADERS=${SRCDIR}/mig_core.h ${SRCDIR}/mig_io.h ${SRCDIR}/mig_opt.h

MHTTP_SOURCES=\
${SRCDIR}/mhttp_util.c\
${SRCDIR}/mhttp_range.c\
${SRCDIR}/mhttp_req.c

MHTTP_HEADERS=\
${SRCDIR}/mhttp_util.h\
${SRCDIR}/mhttp_range.h\
${SRCDIR}/mhttp_method.h\
${SRCDIR}/mhttp_status.h\
${SRCDIR}/mhttp_req.h

all: build

clean:
	rm -f midnighttpd mig_test mot tchat

build: midnighttpd tchat

test:
mig_test: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mig_test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mig_test testprogs/mig_test.c ${CORE_SOURCES}

mot: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mot.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mot testprogs/mot.c ${CORE_SOURCES}

tchat: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat testprogs/tchat.c ${CORE_SOURCES}

midnighttpd: ${CORE_SOURCES} ${CORE_HEADERS} ${MHTTP_SOURCES} ${MHTTP_HEADERS} src/midnighttpd.c src/midnighttpd_config.c src/midnighttpd_config.h
	${CC} ${LDFLAGS} ${CFLAGS} -o midnighttpd src/midnighttpd.c src/midnighttpd_config.c ${CORE_SOURCES} ${MHTTP_SOURCES}
