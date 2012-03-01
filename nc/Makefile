#	$OpenBSD: Makefile,v 1.6 2001/09/02 18:45:41 jakob Exp $

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

.if !${unix:MOpenBSD.}
LDADD=	-lbsd
.endif

.include <bsd.prog.mk>
