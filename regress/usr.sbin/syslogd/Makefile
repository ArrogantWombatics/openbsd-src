#	$OpenBSD: Makefile,v 1.15 2016/07/12 09:57:20 bluhm Exp $

# The following ports must be installed for the regression tests:
# p5-IO-Socket-INET6	object interface for AF_INET and AF_INET6 domain sockets
# p5-Socket6		Perl defines relating to AF_INET6 sockets
# p5-IO-Socket-SSL	perl interface to SSL sockets
# p5-BSD-Resource	BSD process resource limit and priority functions
#
# This package enables additional interoperability tests
# rsyslog		syslog daemon supporting databases, TCP, SSL, RELP
#
# Check wether all required perl packages are installed.  If some
# are missing print a warning and skip the tests, but do not fail.

PERL_REQUIRE !=	perl -Mstrict -Mwarnings -e ' \
    eval { require IO::Socket::INET6 } or print $@; \
    eval { require Socket6 } or print $@; \
    eval { require IO::Socket::SSL } or print $@; \
    eval { require BSD::Resource } or print $@; \
'
.if ! empty (PERL_REQUIRE)
regress:
	@echo "${PERL_REQUIRE}"
	@echo install these perl packages for additional tests
.endif

# Automatically generate regress targets from test cases in directory.

ARGS !=			cd ${.CURDIR} && ls args-*.pl
.if exists (/usr/local/sbin/rsyslogd)
TARGETS ?=		${ARGS}
.else
TARGETS ?=		${ARGS:Nargs-rsyslog*}
.endif
REGRESS_TARGETS =	${TARGETS:S/^/run-regress-/}
LDFLAGS +=		-lutil
CLEANFILES +=		*.log *.log.? *.conf ktrace.out stamp-*
CLEANFILES +=		*.out *.sock *.ktrace *.fstat ttylog
CLEANFILES +=		*.pem *.req *.crt *.key *.srl empty toobig

.MAIN: all

.if make (regress) || make (all)
.BEGIN:
	@echo
	[ -z "${SUDO}" ] || ${SUDO} true
	${SUDO} /etc/rc.d/syslogd stop
.END:
	@echo
	${SUDO} /etc/rc.d/syslogd restart
.endif

# Set variables so that make runs with and without obj directory.
# Only do that if necessary to keep visible output short.

.if ${.CURDIR} == ${.OBJDIR}
PERLINC =
PERLPATH =
.else
PERLINC =	-I${.CURDIR}
PERLPATH =	${.CURDIR}/
.endif

# The arg tests take a perl hash with arguments controlling the
# test parameters.  Generally they consist of client, syslogd, server.

.for a in ${ARGS}
run-regress-$a: $a
	@echo '\n======== $@ ========'
	time SUDO=${SUDO} KTRACE=${KTRACE} SYSLOGD=${SYSLOGD} perl ${PERLINC} ${PERLPATH}syslogd.pl ${PERLPATH}$a
.endfor

# create certificates for TLS

127.0.0.1.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=syslogd/CN=127.0.0.1/ -nodes -newkey rsa -keyout 127.0.0.1.key -x509 -out $@
	${SUDO} cp 127.0.0.1.crt /etc/ssl/
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/
	${SUDO} cp 127.0.0.1.crt /etc/ssl/localhost.crt
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/localhost.key
	${SUDO} cp 127.0.0.1.crt /etc/ssl/::1.crt
	${SUDO} cp 127.0.0.1.key /etc/ssl/private/::1.key

ca.crt fake-ca.crt:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=ca/CN=root/ -nodes -newkey rsa -keyout ${@:R}.key -x509 -out $@

client.req server.req:
	openssl req -batch -new -subj /L=OpenBSD/O=syslogd-regress/OU=${@:R}/CN=localhost/ -nodes -newkey rsa -keyout ${@:R}.key -out $@

client.crt server.crt: ca.crt ${@:R}.req
	openssl x509 -CAcreateserial -CAkey ca.key -CA ca.crt -req -in ${@:R}.req -out $@

empty:
	true >$@

toobig:
	dd if=/dev/zero of=$@ bs=1 count=1 seek=50M

${REGRESS_TARGETS:M*tls*}: client.crt server.crt 127.0.0.1.crt
${REGRESS_TARGETS:M*empty*}: empty
${REGRESS_TARGETS:M*toobig*}: toobig
${REGRESS_TARGETS:M*fake*}: fake-ca.crt
${REGRESS_TARGETS}: ttylog

# make perl syntax check for all args files

.PHONY: syntax libevent

syntax: stamp-syntax

stamp-syntax: ${ARGS}
.for a in ${ARGS}
	@perl -c ${PERLPATH}$a
.endfor
	@date >$@

# run the tests with all variants of libevent backend
libevent:
	cd ${.CURDIR} && EVENT_NOKQUEUE=1 EVENT_NOPOLL=1 ${MAKE} regress
	cd ${.CURDIR} && EVENT_NOKQUEUE=1 EVENT_NOSELECT=1 ${MAKE} regress
	cd ${.CURDIR} && EVENT_NOPOLL=1 EVENT_NOSELECT=1 ${MAKE} regress

.include <bsd.regress.mk>
