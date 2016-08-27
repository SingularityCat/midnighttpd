PREFIX ?= /usr
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/share/man
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
CONFIG_FILES = conf/midnighttpd.conf conf/mimetypes.conf

SYSTEMD_TFD := $(shell pkg-config systemd --variable=tmpfilesdir)
SYSTEMD_SSD := $(shell pkg-config systemd --variable=systemdsystemunitdir)
SYSTEMD_SUD := $(shell pkg-config systemd --variable=systemduserunitdir)

.PHONY: all
all: build

.PHONY: build
build: midnighttpd ${SYSTEMD_FILES} ${CONFIG_FILES}

.PHONY: install
install: build
	mkdir -m 0755 -p ${DESTDIR}${BINDIR}
	install -m 0756 midnighttpd ${DESTDIR}${BINDIR}
	mkdir -m 0755 -p ${DESTDIR}{${SYSTEMD_TFD},${SYSTEMD_SSD},${SYSTEMD_SUD}}
	install -m 0644 systemd/tmpfiles-midnighttpd.conf ${DESTDIR}${SYSTEMD_TFD}/midnighttpd.conf
	install -m 0644 systemd/system-midnighttpd.service ${DESTDIR}${SYSTEMD_SSD}/midnighttpd.service
	install -m 0644 systemd/user-midnighttpd.service ${DESTDIR}${SYSTEMD_SUD}/midnighttpd.service
	mkdir -m 0755 -p ${DESTDIR}/etc/midnighttpd
	install -m 0644 conf/midnighttpd.conf ${DESTDIR}/etc/midnighttpd/midnighttpd.conf
	install -m 0644 conf/mimetypes.conf ${DESTDIR}/etc/midnighttpd/mimetypes.conf

.PHONY: uninstall
uninstall:
	rm ${DESTDIR}${BINDIR}/midnighttpd
	rm ${DESTDIR}${SYSTEMD_TFD}/midnighttpd.conf
	rm ${DESTDIR}${SYSTEMD_SSD}/midnighttpd.service
	rm ${DESTDIR}${SYSTEMD_SUD}/midnighttpd.service
	rm ${DESTDIR}/etc/midnighttpd/midnighttpd.conf
	rm ${DESTDIR}/etc/midnighttpd/mimetypes.conf
	-rmdir ${DESTDIR}/etc/midnighttpd

.PHONY: clean
clean:
	rm -f mig_test mot mrt tchat
	rm -f midnighttpd
	rm -f ${SYSTEMD_FILES} ${CONFIG_FILES}

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

