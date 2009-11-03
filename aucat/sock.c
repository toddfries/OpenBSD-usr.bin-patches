/*	$OpenBSD: sock.c,v 1.35 2009/11/03 21:31:37 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * TODO:
 *
 *	change f->bufsz to contain only socket-side buffer,
 *	because it's less error prone
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "midi.h"
#include "opt.h"
#include "sock.h"

int sock_attach(struct sock *, int);
int sock_read(struct sock *);
int sock_write(struct sock *);
int sock_execmsg(struct sock *);
void sock_reset(struct sock *);

struct fileops sock_ops = {
	"sock",
	sizeof(struct sock),
       	pipe_close,
	pipe_read,
	pipe_write,
	NULL, /* start */
	NULL, /* stop */
	pipe_nfds,
	pipe_pollfd,
	pipe_revents
};


void sock_setvol(void *, unsigned);
void sock_startreq(void *);

struct ctl_ops ctl_sockops = {
	sock_setvol,
	sock_startreq
};

void
rsock_done(struct aproc *p)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f == NULL)
		return;
	sock_reset(f);
	f->pipe.file.rproc = NULL;
	if (f->pipe.file.wproc) {
		if (f->slot >= 0)
			ctl_slotdel(dev_midi, f->slot);
		aproc_del(f->pipe.file.wproc);
		file_del(&f->pipe.file);
	}
	p->u.io.file = NULL;
}

int
rsock_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct sock *f = (struct sock *)p->u.io.file;
	struct abuf *obuf;

	if (!sock_read(f))
		return 0;
	obuf = LIST_FIRST(&p->obuflist);
	if (obuf && f->pstate >= SOCK_RUN) {
		if (!abuf_flush(obuf))
			return 0;
	}
	return 1;
}

int
rsock_out(struct aproc *p, struct abuf *obuf)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->pipe.file.state & FILE_RINUSE)
		return 0;

	/*
	 * When calling sock_read(), we may receive a ``STOP'' command,
	 * and detach ``obuf''. In this case, there's no more caller and
	 * we'll stop processing further messages, resulting in a deadlock.
	 * The solution is to iterate over sock_read() in order to
	 * consume all messages().
	 */
	for (;;) {
		if (!sock_read(f))
			return 0;
	}
	return 1;
}

void
rsock_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	aproc_del(p);
}

void
rsock_hup(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
rsock_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->mode & AMSG_REC)
		return;

	f->delta += delta;
	f->tickpending++;
	for (;;) {
		if (!sock_write(f))
			break;
	}
}

struct aproc_ops rsock_ops = {
	"rsock",
	rsock_in,
	rsock_out,
	rsock_eof,
	rsock_hup,
	NULL, /* newin */
	NULL, /* newout */
	NULL, /* ipos */
	rsock_opos,
	rsock_done
};

void
wsock_done(struct aproc *p)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f == NULL)
		return;
	sock_reset(f);
	f->pipe.file.wproc = NULL;
	if (f->pipe.file.rproc) {
		if (f->slot >= 0)
			ctl_slotdel(dev_midi, f->slot);
		aproc_del(f->pipe.file.rproc);
		file_del(&f->pipe.file);
	}
	p->u.io.file = NULL;
}

int
wsock_in(struct aproc *p, struct abuf *ibuf)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->pipe.file.state & FILE_WINUSE)
		return 0;
	/*
	 * See remark in rsock_out().
	 */
	for (;;) {
		if (!sock_write(f))
			return 0;
	}
	return 1;
}

int
wsock_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ibuflist);
	struct sock *f = (struct sock *)p->u.io.file;

	if (ibuf) {
		if (!abuf_fill(ibuf))
			return 0;
	}
	if (!sock_write(f))
		return 0;
	return 1;
}

void
wsock_eof(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

void
wsock_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	aproc_del(p);
}

void
wsock_ipos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (!(f->mode & AMSG_REC))
		return;

	f->delta += delta;
	f->tickpending++;
	for (;;) {
		if (!sock_write(f))
			break;
	}
}

struct aproc_ops wsock_ops = {
	"wsock",
	wsock_in,
	wsock_out,
	wsock_eof,
	wsock_hup,
	NULL, /* newin */
	NULL, /* newout */
	wsock_ipos,
	NULL, /* opos */
	wsock_done
};

/*
 * Initialise socket in the SOCK_HELLO state with default
 * parameters.
 */
struct sock *
sock_new(struct fileops *ops, int fd)
{
	struct aproc *rproc, *wproc;
	struct sock *f;

	f = (struct sock *)pipe_new(ops, fd, "sock");
	if (f == NULL)
		return NULL;
	f->pstate = SOCK_HELLO;
	f->mode = 0;
	f->opt = opt_byname("default");
	if (f->opt) {
		if (dev_sub)
			f->wpar = f->opt->wpar;
		if (dev_mix)
			f->rpar = f->opt->rpar;
	}
	f->xrun = AMSG_IGNORE;
	f->bufsz = dev_bufsz;
	f->round = dev_round;
	f->delta = 0;
	f->tickpending = 0;
	f->vol = f->lastvol = MIDI_MAXCTL;
	f->slot = -1;

	wproc = aproc_new(&wsock_ops, f->pipe.file.name);
	wproc->u.io.file = &f->pipe.file;
	f->pipe.file.wproc = wproc;
	f->wstate = SOCK_WIDLE;
	f->wtodo = 0xdeadbeef;

	rproc = aproc_new(&rsock_ops, f->pipe.file.name);
	rproc->u.io.file = &f->pipe.file;
	f->pipe.file.rproc = rproc;
	f->rstate = SOCK_RMSG;
	f->rtodo = sizeof(struct amsg);
	return f;
}

/*
 * Free buffers.
 */
void
sock_freebuf(struct sock *f)
{
	struct abuf *rbuf, *wbuf;

	f->pstate = SOCK_INIT;
	wbuf = LIST_FIRST(&f->pipe.file.wproc->ibuflist);
	rbuf = LIST_FIRST(&f->pipe.file.rproc->obuflist);
	if (rbuf || wbuf)
		ctl_slotstop(dev_midi, f->slot);
	if (rbuf)
		abuf_eof(rbuf);
	if (wbuf)
		abuf_hup(wbuf);
	f->tickpending = 0;
}

/*
 * Allocate buffers, so client can start filling write-end.
 */
void
sock_allocbuf(struct sock *f)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;

	if (f->mode & AMSG_PLAY) {
		rbuf = abuf_new(f->bufsz, &f->rpar);
		aproc_setout(f->pipe.file.rproc, rbuf);
	}
	if (f->mode & AMSG_REC) {
		wbuf = abuf_new(f->bufsz, &f->wpar);
		aproc_setin(f->pipe.file.wproc, wbuf);
	}
	f->delta = 0;
	f->tickpending = 0;
	f->pstate = SOCK_START;
	if (!(f->mode & AMSG_PLAY) && ctl_slotstart(dev_midi, f->slot))
		(void)sock_attach(f, 0);
}

/*
 * Set volume. Callback invoked when volume is modified externally
 */
void
sock_setvol(void *arg, unsigned vol)
{
	struct sock *f = (struct sock *)arg;
	struct abuf *rbuf;

	f->vol = vol;
	rbuf = LIST_FIRST(&f->pipe.file.rproc->obuflist);
	if (!rbuf) {
		return;
	}
	dev_setvol(rbuf, MIDI_TO_ADATA(vol));
}

/*
 * Attach the stream. Callback invoked when MMC start
 */
void
sock_startreq(void *arg)
{
	struct sock *f = (struct sock *)arg;

	(void)sock_attach(f, 0);
}

/*
 * Attach play and/or record buffers to dev_mix and/or dev_sub.
 */
int
sock_attach(struct sock *f, int force)
{
	struct abuf *rbuf, *wbuf;

	rbuf = LIST_FIRST(&f->pipe.file.rproc->obuflist);
	wbuf = LIST_FIRST(&f->pipe.file.wproc->ibuflist);

	/*
	 * If in SOCK_START state, dont attach until
	 * the buffer isn't completely filled.
	 */
	if (!force && rbuf && ABUF_WOK(rbuf))
		return 0;

	f->pstate = SOCK_RUN;

	/*
	 * Attach them to the device.
	 */
	dev_attach(f->pipe.file.name,
	    (f->mode & AMSG_PLAY) ? rbuf : NULL, &f->rpar, f->xrun,
	    (f->mode & AMSG_REC)  ? wbuf : NULL, &f->wpar, f->xrun,
	    f->opt->maxweight);
	if (f->mode & AMSG_PLAY)
		dev_setvol(rbuf, MIDI_TO_ADATA(f->vol));

	/*
	 * Send the initial position, if needed.
	 */
	for (;;) {
		if (!sock_write(f))
			break;
	}
	return 1;
}

void
sock_reset(struct sock *f)
{
	switch (f->pstate) {
	case SOCK_START:
		if (ctl_slotstart(dev_midi, f->slot)) {
			(void)sock_attach(f, 1);
			f->pstate = SOCK_RUN;
		}
		/* PASSTHROUGH */
	case SOCK_RUN:
		sock_freebuf(f);
		f->pstate = SOCK_INIT;
		/* PASSTHROUGH */
	case SOCK_INIT:
		/* nothing yet */
		break;
	}
}

/*
 * Read a message from the file descriptor, return 1 if done, 0
 * otherwise. The message is stored in f->rmsg.
 */
int
sock_rmsg(struct sock *f)
{
	unsigned count;
	unsigned char *data;

	while (f->rtodo > 0) {
		if (!(f->pipe.file.state & FILE_ROK)) {
			return 0;
		}
		data = (unsigned char *)&f->rmsg;
		data += sizeof(struct amsg) - f->rtodo;
		count = file_read(&f->pipe.file, data, f->rtodo);
		if (count == 0)
			return 0;
		f->rtodo -= count;
	}
	return 1;
}

/*
 * Write a message to the file descriptor, return 1 if done, 0
 * otherwise.  The "m" argument is f->rmsg or f->wmsg, and the "ptodo"
 * points to the f->rtodo or f->wtodo respectively.
 */
int
sock_wmsg(struct sock *f, struct amsg *m, unsigned *ptodo)
{
	unsigned count;
	unsigned char *data;

	while (*ptodo > 0) {
		if (!(f->pipe.file.state & FILE_WOK)) {
			return 0;
		}
		data = (unsigned char *)m;
		data += sizeof(struct amsg) - *ptodo;
		count = file_write(&f->pipe.file, data, *ptodo);
		if (count == 0)
			return 0;
		*ptodo -= count;
	}
	return 1;
}

/*
 * Read data chunk from the file descriptor, return 1 if at least one
 * byte was read, 0 if the file blocked.
 */
int
sock_rdata(struct sock *f)
{
	struct aproc *p;
	struct abuf *obuf;
	unsigned char *data;
	unsigned count, n;

	p = f->pipe.file.rproc;
	obuf = LIST_FIRST(&p->obuflist);
	if (obuf == NULL)
		return 0;
	if (ABUF_FULL(obuf) || !(f->pipe.file.state & FILE_ROK))
		return 0;
	data = abuf_wgetblk(obuf, &count, 0);
	if (f->pstate != SOCK_MIDI && count > f->rtodo)
		count = f->rtodo;
	n = file_read(&f->pipe.file, data, count);
	if (n == 0)
		return 0;
	abuf_wcommit(obuf, n);
	if (f->pstate != SOCK_MIDI)
		f->rtodo -= n;
	return 1;
}

/*
 * Write data chunk to the file descriptor, return 1 if at least one
 * byte was written, 0 if the file blocked.
 */
int
sock_wdata(struct sock *f)
{
	struct aproc *p;
	struct abuf *ibuf;
	unsigned char *data;
	unsigned count, n;
#define ZERO_MAX 0x1000
	static char zero[ZERO_MAX];

	if (!(f->pipe.file.state & FILE_WOK))
		return 0;
	p = f->pipe.file.wproc;
	ibuf = LIST_FIRST(&p->ibuflist);
	if (ibuf) {
		if (ABUF_EMPTY(ibuf))
			return 0;
		data = abuf_rgetblk(ibuf, &count, 0);
		if (f->pstate != SOCK_MIDI && count > f->wtodo)
			count = f->wtodo;
		n = file_write(&f->pipe.file, data, count);
		if (n == 0)
			return 0;
		abuf_rdiscard(ibuf, n);
		if (f->pstate != SOCK_MIDI)
			f->wtodo -= n;
	} else {
		if (f->pstate == SOCK_MIDI)
			return 0; 
		/*
		 * There's no dev_detach() routine yet,
		 * so now we abruptly destroy the buffer.
		 * Until we implement dev_detach, complete
		 * the packet with zeros...
		 */
		count = ZERO_MAX;
		if (count > f->wtodo)
			count = f->wtodo;
		n = file_write(&f->pipe.file, zero, count);
		if (n == 0)
			return 0;
		f->wtodo -= n;
	}
	return 1;
}

int
sock_setpar(struct sock *f)
{
	struct amsg_par *p = &f->rmsg.u.par;
	unsigned min, max, rate;

	if (AMSG_ISSET(p->bits)) {
		if (p->bits < BITS_MIN || p->bits > BITS_MAX) {
			return 0;
		}
		if (AMSG_ISSET(p->bps)) {
			if (p->bps < ((p->bits + 7) / 8) || p->bps > 4) {
				return 0;
			}
		} else
			p->bps = APARAMS_BPS(p->bits);
		f->rpar.bits = f->wpar.bits = p->bits;
		f->rpar.bps = f->wpar.bps = p->bps;
	}
	if (AMSG_ISSET(p->sig))
		f->rpar.sig = f->wpar.sig = p->sig ? 1 : 0;
	if (AMSG_ISSET(p->le))
		f->rpar.le = f->wpar.le = p->le ? 1 : 0;
	if (AMSG_ISSET(p->msb))
		f->rpar.msb = f->wpar.msb = p->msb ? 1 : 0;
	if (AMSG_ISSET(p->rchan) && (f->mode & AMSG_REC)) {
		if (p->rchan < 1)
			p->rchan = 1;
		if (p->rchan > NCHAN_MAX)
			p->rchan = NCHAN_MAX;
		f->wpar.cmin = f->opt->wpar.cmin;
		f->wpar.cmax = f->opt->wpar.cmin + p->rchan - 1;
		if (f->wpar.cmax > f->opt->wpar.cmax)
			f->wpar.cmax = f->opt->wpar.cmax;
	}
	if (AMSG_ISSET(p->pchan) && (f->mode & AMSG_PLAY)) {
		if (p->pchan < 1)
			p->pchan = 1;
		if (p->pchan > NCHAN_MAX)
			p->pchan = NCHAN_MAX;
		f->rpar.cmin = f->opt->rpar.cmin;
		f->rpar.cmax = f->opt->rpar.cmin + p->pchan - 1;
		if (f->rpar.cmax > f->opt->rpar.cmax)
			f->rpar.cmax = f->opt->rpar.cmax;
	}
	if (AMSG_ISSET(p->rate)) {
		if (p->rate < RATE_MIN)
			p->rate = RATE_MIN;
		if (p->rate > RATE_MAX)
			p->rate = RATE_MAX;
		f->round = dev_roundof(p->rate);
		f->rpar.rate = f->wpar.rate = p->rate;
		if (!AMSG_ISSET(p->appbufsz)) {
			p->appbufsz = dev_bufsz / dev_round * f->round;
		}
	}
	if (AMSG_ISSET(p->xrun)) {
		if (p->xrun != AMSG_IGNORE &&
		    p->xrun != AMSG_SYNC &&
		    p->xrun != AMSG_ERROR) {
			return 0;
		}
		f->xrun = p->xrun;
		if (f->opt->mmc && f->xrun == AMSG_IGNORE)
			f->xrun = AMSG_SYNC;
	}
	if (AMSG_ISSET(p->bufsz)) {
		/*
		 * XXX: bufsz will become read-only, but for now
		 *      allow old library to properly work
		 */
		min = (dev_bufsz / dev_round) * f->round;
		if (p->bufsz < min)
			p->bufsz = min;
		p->appbufsz = p->bufsz - min;
	}
	if (AMSG_ISSET(p->appbufsz)) {
		rate = (f->mode & AMSG_PLAY) ? f->rpar.rate : f->wpar.rate;
		min = 1;
		max = 1 + rate / dev_round;
		min *= f->round;
		max *= f->round;
		p->appbufsz += f->round - 1;
		p->appbufsz -= p->appbufsz % f->round;
		if (p->appbufsz < min)
			p->appbufsz = min;
		if (p->appbufsz > max)
			p->appbufsz = max;
		f->bufsz = p->appbufsz;
	}
	return 1;
}

/*
 * allocate buffers, so client can start filling write-end.
 */
void
sock_midiattach(struct sock *f, unsigned mode)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;
	
	if (mode & AMSG_MIDIOUT) {
		rbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setout(f->pipe.file.rproc, rbuf);
	}
	if (mode & AMSG_MIDIIN) {
		wbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setin(f->pipe.file.wproc, wbuf);
	}
	dev_midiattach(rbuf, wbuf);
}

int
sock_hello(struct sock *f)
{
	struct amsg_hello *p = &f->rmsg.u.hello;

	if (p->version != AMSG_VERSION) {
		return 0;
	}
	/*
	 * XXX : dev_midi can no longer be NULL, right ?
	 */
	if (dev_midi && (p->proto & (AMSG_MIDIIN | AMSG_MIDIOUT))) {
		if (p->proto & ~(AMSG_MIDIIN | AMSG_MIDIOUT)) {
			return 0;
		}
		f->mode = p->proto;
		f->pstate = SOCK_MIDI;
		sock_midiattach(f, p->proto);
		return 1;
	}
	f->opt = opt_byname(p->opt);
	if (f->opt == NULL)
		return 0;
	if (dev_sub)
		f->wpar = f->opt->wpar;
	if (dev_mix)
		f->rpar = f->opt->rpar;
	if (f->opt->mmc)
		f->xrun = AMSG_SYNC;
	if ((p->proto & ~(AMSG_PLAY | AMSG_REC)) != 0 ||
	    (p->proto &  (AMSG_PLAY | AMSG_REC)) == 0) {
		return 0;
	}
	f->mode = 0;
	if (p->proto & AMSG_PLAY) {
		if (!dev_mix) {
			return 0;
		}
		f->mode |= AMSG_PLAY;
	}
	if (p->proto & AMSG_REC) {
		if (!dev_sub) {
			return 0;
		}
		f->mode |= AMSG_REC;
	}
	if (dev_midi) {
		f->slot = ctl_slotnew(dev_midi,
		     p->who, &ctl_sockops, f,
		     f->opt->mmc);
		if (f->slot < 0) {
			return 0;
		}
	}
	f->pstate = SOCK_INIT;
	return 1;
}

/*
 * Execute message in f->rmsg and change the state accordingly; return 1
 * on success, and 0 on failure, in which case the socket is destroyed.
 */
int
sock_execmsg(struct sock *f)
{
	struct amsg *m = &f->rmsg;

	switch (m->cmd) {
	case AMSG_DATA:
		if (f->pstate != SOCK_RUN && f->pstate != SOCK_START) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!(f->mode & AMSG_PLAY)) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (f->pstate == SOCK_START &&
		    ABUF_FULL(LIST_FIRST(&f->pipe.file.rproc->obuflist))) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		f->rstate = SOCK_RDATA;
		f->rtodo = m->u.data.size;
		if (f->rtodo == 0) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		break;
	case AMSG_START:
		if (f->pstate != SOCK_INIT) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		sock_allocbuf(f);
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_STOP:
		if (f->pstate != SOCK_RUN && f->pstate != SOCK_START) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (f->pstate == SOCK_START &&
		    ctl_slotstart(dev_midi, f->slot))
			(void)sock_attach(f, 1);
		sock_freebuf(f);
		AMSG_INIT(m);
		m->cmd = AMSG_ACK;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETPAR:
		if (f->pstate != SOCK_INIT) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!sock_setpar(f)) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_GETPAR:
		if (f->pstate != SOCK_INIT) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_GETPAR;
		m->u.par.legacy_mode = f->mode;
		m->u.par.bits = f->rpar.bits;
		m->u.par.bps = f->rpar.bps;
		m->u.par.sig = f->rpar.sig;
		m->u.par.le = f->rpar.le;
		m->u.par.msb = f->rpar.msb;
		m->u.par.rate = f->rpar.rate;
		m->u.par.rchan = f->wpar.cmax - f->wpar.cmin + 1;
		m->u.par.pchan = f->rpar.cmax - f->rpar.cmin + 1;
		m->u.par.appbufsz = f->bufsz;
		m->u.par.bufsz =
		    f->bufsz + (dev_bufsz / dev_round) * f->round;
		m->u.par.round = f->round;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_GETCAP:
		if (f->pstate != SOCK_INIT) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_GETCAP;
		m->u.cap.rate = dev_rate;
		m->u.cap.pchan = dev_mix ?
		    (f->opt->rpar.cmax - f->opt->rpar.cmin + 1) : 0;
		m->u.cap.rchan = dev_sub ?
		    (f->opt->wpar.cmax - f->opt->wpar.cmin + 1) : 0;
		m->u.cap.bits = sizeof(short) * 8;
		m->u.cap.bps = sizeof(short);
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETVOL:
		if (f->pstate != SOCK_RUN &&
		    f->pstate != SOCK_START && f->pstate != SOCK_INIT) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (m->u.vol.ctl > MIDI_MAXCTL) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		sock_setvol(f, m->u.vol.ctl);
		if (f->slot >= 0)
			ctl_slotvol(dev_midi, f->slot, m->u.vol.ctl);
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_HELLO:
		if (f->pstate != SOCK_HELLO) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!sock_hello(f)) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_ACK;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_BYE:
		if (f->pstate != SOCK_INIT) {
		}
		aproc_del(f->pipe.file.rproc);
		return 0;
	default:
		aproc_del(f->pipe.file.rproc);
		return 0;
	}
	if (f->rstate == SOCK_RRET) {
		if (f->wstate != SOCK_WIDLE ||
		    !sock_wmsg(f, &f->rmsg, &f->rtodo))
			return 0;
		if (f->pstate == SOCK_MIDI && (f->mode & AMSG_MIDIOUT)) {
			f->rstate = SOCK_RDATA;
			f->rtodo = 0;
		} else {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
	}
	return 1;
}

/*
 * Create a new data/pos message.
 */
int
sock_buildmsg(struct sock *f)
{
	struct aproc *p;
	struct abuf *ibuf;

	if (f->pstate == SOCK_MIDI) {
		f->wstate = SOCK_WDATA;
		f->wtodo = 0;
		return 1;
	}

	/*
	 * If pos changed, build a MOVE message.
	 */
	if (f->tickpending) {
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_MOVE;
		f->wmsg.u.ts.delta = f->delta;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->delta = 0;
		f->tickpending = 0;
		return 1;
	}

	/*
	 * if volume changed build a SETVOL message
	 */
	if (f->pstate >= SOCK_START && f->vol != f->lastvol) {
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_SETVOL;
		f->wmsg.u.vol.ctl = f->vol;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->lastvol = f->vol;
		return 1;
	}

	/*
	 * If data available, build a DATA message.
	 */
	p = f->pipe.file.wproc;
	ibuf = LIST_FIRST(&p->ibuflist);
	if (ibuf && ABUF_ROK(ibuf)) {
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_DATA;
		f->wmsg.u.data.size = ibuf->used - (ibuf->used % ibuf->bpf);
		if (f->wmsg.u.data.size > AMSG_DATAMAX)
			f->wmsg.u.data.size =
			    AMSG_DATAMAX - (AMSG_DATAMAX % ibuf->bpf);
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}
	f->wstate = SOCK_WIDLE;
	return 0;
}

/*
 * Read from the socket file descriptor, fill input buffer and update
 * the state. Return 1 if at least one message or 1 data byte was
 * processed, 0 if something blocked.
 */
int
sock_read(struct sock *f)
{
	switch (f->rstate) {
	case SOCK_RMSG:
		if (!sock_rmsg(f))
			return 0;
		if (!sock_execmsg(f))
			return 0;
		break;
	case SOCK_RDATA:
		if (!sock_rdata(f))
			return 0;
		if (f->pstate != SOCK_MIDI && f->rtodo == 0) {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
		/*
		 * XXX: have to way that the buffer is full before starting
		 */
		if (f->pstate == SOCK_START && ctl_slotstart(dev_midi, f->slot))
			(void)sock_attach(f, 0);
		break;
	case SOCK_RRET:
		return 0;
	}
	return 1;
}

/*
 * Process messages to return.
 */
int
sock_return(struct sock *f)
{
	struct aproc *rp;

	while (f->rstate == SOCK_RRET) {
		if (!sock_wmsg(f, &f->rmsg, &f->rtodo))
			return 0;
		if (f->pstate == SOCK_MIDI && (f->mode & AMSG_MIDIOUT)) {
			f->rstate = SOCK_RDATA;
			f->rtodo = 0;
		} else {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
		if (f->pipe.file.state & FILE_RINUSE)
			break;
		f->pipe.file.state |= FILE_RINUSE;
		for (;;) {
			/*
			 * in() may trigger rsock_done and destroy the
			 * wsock.
			 */
			rp = f->pipe.file.rproc;
			if (!rp || !rp->ops->in(rp, NULL))
				break;
		}
		f->pipe.file.state &= ~FILE_RINUSE;
		if (f->pipe.file.wproc == NULL)
			return 0;
	}
	return 1;
}

/*
 * Write messages and data on the socket file descriptor. Return 1 if
 * at least one message or one data byte was processed, 0 if something
 * blocked.
 */
int
sock_write(struct sock *f)
{
	switch (f->wstate) {
	case SOCK_WMSG:
		if (!sock_wmsg(f, &f->wmsg, &f->wtodo))
			return 0;
		if (f->wmsg.cmd != AMSG_DATA) {
			f->wstate = SOCK_WIDLE;
			f->wtodo = 0xdeadbeef;
			break;
		}
		f->wstate = SOCK_WDATA;
		f->wtodo = f->wmsg.u.data.size;
		/* PASSTHROUGH */
	case SOCK_WDATA:
		if (!sock_wdata(f))
			return 0;
		if (f->pstate == SOCK_MIDI || f->wtodo > 0)
			break;
		f->wstate = SOCK_WIDLE;
		f->wtodo = 0xdeadbeef;
		/* PASSTHROUGH */
	case SOCK_WIDLE:
		if (!sock_return(f))
			return 0;
		if (!sock_buildmsg(f))
			return 0;
		break;
	}
	return 1;
}
