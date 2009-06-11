#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:02 deraadt Exp $

MAN=	mkdep.1


all: mkdep.gcc

_cc_cookie=MagicCookie0cd489f0c1479952
.for _pat in <file> FILE FILENAME
_cc_sed += -e 's/^.* \(-[a-zA-Z0-9-]*\) ${_pat},.* \(-[a-zA-Z0-9-]*\) ${_pat} .*$$/\1 \2 ${_cc_cookie}/'
_cc_sed += -e 's/^.* \(-[a-zA-Z0-9-]*\) ${_pat}[, ].*$$/\1 ${_cc_cookie}/'
_cc_sed += -e 's/.*,.* \(-[a-zA-Z0-9-]*\) ${_pat} .*$$/\1 ${_cc_cookie}/'
.endfor
_cc_sed += -e '/${_cc_cookie}/{s/ ${_cc_cookie}$$//;p;}'

mkdep.gcc: mkdep.gcc.sh
		sed 's/%ARGLIST%/'"$$(echo $$(${CC} -v --help 2>&1 | \
		sed -n ${_cc_sed} | xargs -n 1 echo | \
		sort -u) | \
		sed 's/ /|/g')"'/' ${.CURDIR}/mkdep.gcc.sh > mkdep.gcc
	

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    mkdep.gcc ${DESTDIR}/usr/bin/mkdep

CLEANFILES+= mkdep.list mkdep.gcc

.include <bsd.prog.mk>
