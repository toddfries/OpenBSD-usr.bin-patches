#!/bin/sh -
#
#	$OpenBSD: mkdep.gcc.sh,v 1.15 2008/08/28 08:39:44 jmc Exp $
#	$NetBSD: mkdep.gcc.sh,v 1.9 1994/12/23 07:34:59 jtc Exp $
#
# Copyright (c) 1991, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)mkdep.gcc.sh	8.1 (Berkeley) 6/6/93
#

echo "...mkdep... $@"

#
# Scan for a -o option in the arguments and record the filename given.
# This is needed, since "cc -M -o out" writes to the file "out", not to
# stdout.
#
scanfordasho() {
	while [ $# != 0 ]
	do case "$1" in
		-o)	
			file="$2"; shift; shift ;;
		-o*)
			file="${1#-o}"; shift ;;
		-x)
			commonargs="$commonargs $1 $2"; shift; shift ;;
		-*)
			commonargs="$commonargs $1"; shift ;;
		*)
			 argv[${#argv[*]}]="$1"; shift;;
		esac
	done
	if [ "$MAKEFLAGS" ]; then
		set -- $MAKEFLAGS
		while [ "$1" ]; do case "$1" in
			-j)	concurrence="$2"; shift; shift;;
			-j*)	concurrence="${1#-j}"; shift ;;
			*) shift;;
			esac
		done
	fi
}

D=.depend			# default dependency file is .depend
append=0
pflag=

while :
do
	case "$1" in
		# -a appends to the depend file
		-a)
			append=1
			shift ;;

		# -f allows you to select a makefile name
		-f)
			D=$2
			shift; shift ;;

		# the -p flag produces "program: program.c" style dependencies
		# so .o's don't get produced
		-p)
			pflag=p
			shift ;;
		# the -j flag mimics make's -j flag, sets concurrency
		-j)
			concurrence="$2"; shift; shift;;
		-j*)
			concurrence="${1#-j}"; shift ;;
		*)
			break ;;
	esac
done

if [ $# = 0 ] ; then
	echo 'usage: mkdep [-ap] [-f file] [flags] file ...'
	exit 1
fi

scanfordasho "$@"



DIR=`mktemp -d /tmp/mkdep.XXXXXXXXXX` || exit 1

trap 'rm -f $DIR ; trap 2 ; kill -2 $$' 1 2 3 13 15

if [ x$concurrence = x ]; then
	if [ "x$file" = x ]; then
		${CC:-cc} -M "$@"
	else
		${CC:-cc} -M "$@" && cat "$file"
	fi |
	if [ x$pflag = x ]; then
		sed -e 's; \./; ;g' > $DIR/depend
	else
		sed -e 's;\.o[ ]*:; :;' -e 's; \./; ;g' > $DIR/depend
	fi
else
	echo default:: depend > $DIR/Makefile
	i=0
	while [ i -lt ${#argv[*]} ]
	do
		ARG="${argv[$i]}"
		NAME="$i.${ARG##*/}"
		echo SRCS+= $DIR/${NAME}.dep
		echo $DIR/${NAME}.dep: $ARG
		echo "\t${CC:-cc} -M $commonargs $ARG > $DIR/${NAME}.dep"
		let i=i+1
	done >> $DIR/Makefile
	echo depend: \${SRCS} >> $DIR/Makefile
	echo "\tcat \${.ALLSRC} > $DIR/depend" >> $DIR/Makefile
	make -f $DIR/Makefile || { echo $DIR/Makefile; exit 1; }
fi

if [ $? != 0 ]; then
	echo 'mkdep: compile failed.'
	rm -rf $DIR
	exit 1
fi

if [ $append = 1 ]; then
	cat $DIR/depend >> $D
	if [ $? != 0 ]; then
		echo 'mkdep: append failed.'
		rm -rf $DIR
		exit 1
	fi
else
	mv -f $DIR/depend $D
	if [ $? != 0 ]; then
		echo 'mkdep: rename failed.'
		rm -rf $DIR
		exit 1
	fi
fi

rm -rf $DIR
exit 0
