#	$OpenBSD: Makefile,v 1.119 2010/10/29 14:47:14 millert Exp $

.include <bsd.own.mk>

#SUBDIR= apply apropos ar arch asa asn1_compile aucat audioctl banner
#	bdes bgplg \
#	biff cal calendar cap_mkdb cdio chpass \
#	crontab csplit col colrm \
#	column comm compile_et compress 
#	dc deroff expand fgen finger from fsplit fstat 
#	gencat getcap \
#	getconf getent getopt gprof gzsig at bc
SUBDIR= awk \
	basename \
	cmp \
	cpp ctags cut \
	diff diff3 dirname du encrypt env false file \
	file2c find fmt fold ftp grep head hexdump id indent \
	infocmp ipcrm ipcs \
	join jot kdump keynote ktrace lam last lastcomm leave less lex lndir \
	locate lock logger login logname look lorder \
	m4 mail make man mandoc mesg mg \
	midiplay mixerctl mkdep mklocale mkstr mktemp modstat nc netstat \
	newsyslog \
	nfsstat nice nm nohup oldrdist pagesize passwd paste patch pctr \
	pkg-config pkill \
	pmdb pr printenv printf quota radioctl ranlib rcs rdist rdistd \
	readlink renice rev rpcgen rpcinfo rs rsh rup ruptime rusers rwall \
	rwho sdiff script sed sendbug shar showmount skey \
	skeyaudit skeyinfo skeyinit sort spell split ssh stat su systat \
	sudo tail talk tcopy tcpbench tee telnet tftp tic time tip \
	tmux top touch tput tr true tset tsort tty usbhidaction usbhidctl \
	ul uname unexpand unifdef uniq units \
	unvis users uudecode uuencode vacation vi vis vmstat w wall wc \
	what whatis which who whois write x99token xargs xinstall xlint \
	xstr yacc yes

.if (${YP:L} == "yes")
SUBDIR+=ypcat ypmatch ypwhich
.endif

.if (${ELF_TOOLCHAIN:L} == "no")
SUBDIR+= strip strings
.endif

.include <bsd.subdir.mk>
