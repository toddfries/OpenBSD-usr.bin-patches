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

#
# Scan for a -o option in the arguments and record the filename given.
# This is needed, since "cc -M -o out" writes to the file "out", not to
# stdout.
#
set -A cargs
scanfordasho() {
	while [ $# != 0 ]
	do case "$1" in
		-o|--output)	
			file="$2"; shift; shift ;;
		-o*)
			file="${1#-o}"; shift ;;
		# List options that take files, filled in at build time
		%ARGLIST%)
			cargs[${#cargs[*]}]=$(echo "$1" | sed 's/"/\\"/g')
			cargs[${#cargs[*]}]=$(echo "$2" | sed 's/"/\\"/g')
			shift; shift ;;

		# Any options single or double that don't take files
		-*)
			cargs[${#cargs[*]}]=$(echo "$1" | sed 's/"/\\"/g')

			# If a non file and not a -* then its a flag arg
			if ! [ -f "$2" ]; then
				if [ "${2}" = "${2#-*}" ]; then
					cargs[${#cargs[*]}]=$(echo "$2" | sed 's/"/\\"/g')
					shift
				fi
			fi
			shift ;;
		*)
			files[${#files[*]}]="$1"; shift;;
		esac
	done
}
checkflags() {
	if [ "$MAKEFLAGS" ]; then
		set -- $MAKEFLAGS
		while [ $# != 0 ]; do case "$1" in
			-j)
				concurrence="-j$2"; shift; shift ;;
			-j*)
				concurrence="$1"; shift ;;
			*)
				shift ;;
			esac
		done
	fi
}

D=.depend			# default dependency file is .depend
append=0
debug=0
pflag=
args="$@"

# if nothing else, fallback on hw.ncpu
# use cmdline -j# if provided
# permit MAKEFLAGS to over-ride all
ncpu=$(sysctl -n hw.ncpu)
# Per Kettenis, set max default upper bounds at 16, so we do not run out of
# processes
[ ncpu -gt 16 ] && ncpu=16
if [ ncpu -gt 1 ]; then
	concurrence="-j$ncpu"
else
	concurrence=""
fi

while :
do
	case "$1" in
		# -a appends to the depend file
		-a)
			append=1
			shift ;;

		# debug mode
		-d)
			debug=1
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
			concurrence="-j$2"; shift; shift;;
		-j*)
			concurrence="-j$1"; shift ;;
		*)
			break ;;
	esac
done

checkflags

if [ $# = 0 ] ; then
	echo 'usage: mkdep [-ap] [-f file] [flags] file ...'
	exit 1
fi
if [ debug -gt 0 ]; then
	at=""
	echo "# DEBUG: mkdep $args"
	rm=":"
else
	at="@"
	rm="rm"
fi

scanfordasho "$@"

DIR=`mktemp -d /tmp/mkdep.XXXXXXXXXX` || exit 1

trap '$rm -rf $DIR ; trap 2 ; kill -2 $$' 1 2 3 13 15

{
	if [ x$pflag = x ]; then
		echo "SUBST=	${at}sed -e 's; \./; ;g'"
	else
		echo "SUBST=	${at}sed -e 's;\.o[ ]*:; :;' -e 's; \./; ;g'"
	fi

	echo default:: depend
	i=0
	while [ i -lt ${#cargs[*]} ]
	do
		echo "CCDEP+=\"${cargs[$i]}\""
		let i=i+1
	done

	i=0
	while [ i -lt ${#files[*]} ]
	do
		ARG="${files[$i]}"
		NAME="$i.${ARG##*/}"
		echo "TMPDEP+= $DIR/${NAME}.dep\n$DIR/${NAME}.dep: $ARG"
		echo -n "\t${at}${CC:-cc} -M \${CCDEP} $ARG > $DIR/${NAME}.dep"
		echo " || : > $DIR/${NAME}.dep"
		let i=i+1
	done
	echo depend: \${TMPDEP}
	if [ $append = 1 ]; then
		echo "\t${at}\${SUBST} \${.ALLSRC} >> $D"
	else
		echo "\t${at}\${SUBST} \${.ALLSRC} >  $D"
	fi
} > $DIR/Makefile

if ! make $concurrence -rf $DIR/Makefile; then
	echo 'mkdep: compile failed.'
	$rm -rf $DIR $D
	exit 1
fi

$rm -rf $DIR
exit 0
