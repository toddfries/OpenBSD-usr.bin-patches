/*	$OpenBSD: cl.h,v 1.7 2006/01/08 21:05:39 miod Exp $	*/

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	@(#)cl.h	10.19 (Berkeley) 9/24/96
 */

typedef struct _cl_private {
	CHAR_T	 ibuf[256];	/* Input keys. */

	int	 eof_count;	/* EOF count. */

	struct termios orig;	/* Original terminal values. */
	struct termios ex_enter;/* Terminal values to enter ex. */
	struct termios vi_enter;/* Terminal values to enter vi. */

	char	*el;		/* Clear to EOL terminal string. */
	char	*cup;		/* Cursor movement terminal string. */
	char	*cuu1;		/* Cursor up terminal string. */
	char	*rmso, *smso;	/* Inverse video terminal strings. */
	char	*smcup, *rmcup;	/* Terminal start/stop strings. */

	int	 killersig;	/* Killer signal. */
#define	INDX_HUP	0
#define	INDX_INT	1
#define	INDX_TERM	2
#define	INDX_WINCH	3
#define	INDX_MAX	4	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

	enum {			/* Tty group write mode. */
	    TGW_UNKNOWN=0, TGW_SET, TGW_UNSET } tgw;

	enum {			/* Terminal initialization strings. */
	    TE_SENT=0, TI_SENT } ti_te;

#define	CL_IN_EX	0x0001	/* Currently running ex. */
#define	CL_RENAME	0x0002	/* X11 xterm icon/window renamed. */
#define	CL_RENAME_OK	0x0004	/* User wants the windows renamed. */
#define	CL_SCR_EX_INIT	0x0008	/* Ex screen initialized. */
#define	CL_SCR_VI_INIT	0x0010	/* Vi screen initialized. */
#define	CL_SIGHUP	0x0020	/* SIGHUP arrived. */
#define	CL_SIGINT	0x0040	/* SIGINT arrived. */
#define	CL_SIGTERM	0x0080	/* SIGTERM arrived. */
#define	CL_SIGWINCH	0x0100	/* SIGWINCH arrived. */
#define	CL_STDIN_TTY	0x0200	/* Talking to a terminal. */
	u_int32_t flags;
} CL_PRIVATE;

#define	CLP(sp)		((CL_PRIVATE *)((sp)->gp->cl_private))
#define	GCLP(gp)	((CL_PRIVATE *)(gp)->cl_private)

/* Return possibilities from the keyboard read routine. */
typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/* X11 xterm escape sequence to rename the icon/window. */
#define	XTERM_RENAME	"\033]0;%s\007"

/*
 * XXX
 * Some implementations of curses.h don't define these for us.  Used for
 * compatibility only.
 */
#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

#include "cl_extern.h"
