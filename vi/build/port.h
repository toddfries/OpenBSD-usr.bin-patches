/*	$OpenBSD: port.h,v 1.4 1999/11/26 22:49:08 millert Exp $	*/

/*
 * Nothing needed here for OpenBSD.
 */
#if defined(__linux__)
typedef u_int32_t recno_t;
#if !defined(TCSASOFT)
#define TCSASOFT	0
#endif
#endif
