#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:50:02 deraadt Exp $

MAN=	mkdep.1


all: mkdep.gcc

mkdep.gcc: mkdep.gcc.sh
		sed 's/%ARGLIST%/'"$$(echo $$(${CC} -v --help 2>&1 | \
		sed -n '/-[a-zA-Z]* <file>/{s/^[ ]*\(-[a-zA-Z0-9-]*\) <file> .*$$/\1/;p;}') | \
		sed 's/ /|/g')"'/;s/[ ]|/ /' ${.CURDIR}/mkdep.gcc.sh > mkdep.gcc
	

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    mkdep.gcc ${DESTDIR}/usr/bin/mkdep

CLEANFILES+= mkdep.list mkdep.gcc

.include <bsd.prog.mk>
