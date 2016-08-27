PREFIX ?= /usr
BINDIR ?= ${PREFIX}/bin
RUNDIR ?= /run

WEBROOT ?= /var/www

CFLAGS:=-std=gnu11 -pedantic -Isrc ${CFLAGS} -g

CORE_SOURCES=\
src/mig_core.c\
src/mig_io.c\
src/mig_opt.c\
src/mig_dynarray.c\
src/mig_radix_tree.c

CORE_HEADERS=\
src/mig_core.h\
src/mig_io.h\
src/mig_parse.h\
src/mig_opt.h\
src/mig_dynarray.h\
src/mig_radix_tree.h

MHTTP_SOURCES=\
src/mhttp_util.c\
src/mhttp_range.c\
src/mhttp_req.c

MHTTP_HEADERS=\
src/mhttp_util.h\
src/mhttp_range.h\
src/mhttp_method.h\
src/mhttp_status.h\
src/mhttp_req.h

MIDNIGHTTPD_SOURCES=\
src/midnighttpd_core.c\
src/midnighttpd_config.c\
src/midnighttpd.c

MIDNIGHTTPD_HEADERS=\
src/midnighttpd_core.h\
src/midnighttpd_config_opt.h\
src/midnighttpd_config.h

LINT ?= clang-tidy

SYSTEMD_FILES = systemd/tmpfiles-midnighttpd.conf systemd/system-midnighttpd.service systemd/user-midnighttpd.service
CONFIG_FILES = conf/midnighttpd.conf

.PHONY: all
all: build

.PHONY: install
install: midnighttpd
	install -m 0755 midnighttpd ${BINDIR}

.PHONY: clean
clean:
	rm -f mig_test mot mrt tchat
	rm -f midnighttpd
	rm -f ${SYSTEMD_FILES} ${CONFIG_FILES}

.PHONY: build
build: midnighttpd ${SYSTEMD_FILES} ${CONFIG_FILES}

.PHONY: test
test:

mig_test: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mig_test.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mig_test testprogs/mig_test.c ${CORE_SOURCES}

mot: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mot.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mot testprogs/mot.c ${CORE_SOURCES}

mrt: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/mrt.c
	${CC} ${LDFLAGS} ${CFLAGS} -o mot testprogs/mrt.c ${CORE_SOURCES}

tchat: ${CORE_SOURCES} ${CORE_HEADERS} testprogs/tchat.c
	${CC} ${LDFLAGS} ${CFLAGS} -o tchat testprogs/tchat.c ${CORE_SOURCES}

midnighttpd: ${CORE_SOURCES} ${CORE_HEADERS} ${MHTTP_SOURCES} ${MHTTP_HEADERS} ${MIDNIGHTTPD_SOURCES} ${MIDNIGHTTPD_HEADERS}
	${CC} ${LDFLAGS} ${CFLAGS} -o midnighttpd ${CORE_SOURCES} ${MHTTP_SOURCES} ${MIDNIGHTTPD_SOURCES}

%: %.in
	sed -e "s~%bindir%~${BINDIR}~" \
	    -e "s~%rundir%~${RUNDIR}~" \
	    -e "s~%webroot%~${WEBROOT}~" \
	    $< > $@

.PHONY: lint
lint:
	${LINT} ${CORE_SOURCES} ${CORE_HEADERS} ${MHTTP_SOURCES} ${MHTTP_HEADERS} ${MIDNIGHTTPD_SOURCES} ${MIDNIGHTTPD_HEADERS} --

