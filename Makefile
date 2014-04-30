#	$OpenBSD: Makefile,v 1.141 2014/04/24 14:55:39 tedu Exp $

.include <bsd.own.mk>

SUBDIR= apply arch asa at aucat audioctl awk banner \
	basename bc bgplg \
	biff cal calendar cap_mkdb cdio chpass cmp col colrm \
	column comm compress cpp crontab csplit ctags cu cut \
	dc deroff diff diff3 dirname du encrypt env expand false file \
	find fgen finger fmt fold from fsplit fstat ftp gencat getcap \
	getconf getent getopt gprof grep gzsig head hexdump htpasswd id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm leave less lex \
	libtool lndir \
	locale locate lock logger login logname look lorder \
	m4 mail make man mandoc mesg mg \
	midiplay mixerctl mkdep mklocale mkstr mktemp modstat nc netstat \
	newsyslog \
	nfsstat nice nm nl nohup oldrdist pagesize passwd paste patch pctr \
	pkg-config pkill \
	pr printenv printf quota radioctl rcs rdist rdistd \
	readlink renice rev rpcgen rpcinfo rs rup rusers rwall \
	sdiff script sed sendbug shar showmount signify skey \
	skeyaudit skeyinfo skeyinit sndiod \
	sort spell split sqlite3 ssh stat su systat \
	sudo tail talk tcopy tcpbench tee telnet tftp tic tip time \
	tmux top touch tput tr true tset tsort tty usbhidaction usbhidctl \
	ul uname unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vi vis vmstat w wall wc \
	what which who whois write x99token xargs xinstall \
	xstr yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.include <bsd.subdir.mk>
