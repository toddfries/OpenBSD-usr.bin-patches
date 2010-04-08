/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 	- Redistributions of source code must retain the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer in the documentation and/or other
 * 	  materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sndio.h>
#include <unistd.h>

void
usage(void)
{
	fprintf(stderr, "usage: aucatvol [-c chan] [-f dev] vol\n");
	exit(1);
}

/*
 * send a MIDI ``volume change'' message, on the given channel
 */
void
setvol(struct mio_hdl *hdl, unsigned chan, unsigned vol)
{
#define MSGLEN 3
	char msg[MSGLEN];

	msg[0] = 0xb0 | chan;
	msg[1] = 0x07;
	msg[2] = vol;
	if (mio_write(hdl, msg, MSGLEN) != MSGLEN) {
		fprintf(stderr, "couldn't write message\n");
		exit(1);
	}
}


int
main(int argc, char **argv)
{
	char *dev = "aucat:0";
	struct mio_hdl *hdl;
	unsigned chan, vol;
	int c, all = 1;

	while ((c = getopt(argc, argv, "f:c:")) != -1) {
		switch (c) {
		case 'f':
			dev = optarg;
			break;
		case 'c':
			if (sscanf(optarg, "%u", &chan) != 1) {
				fprintf(stderr, "%s: not a number\n", optarg);
				exit(1);
			}
			if (chan > 15) {
				fprintf(stderr, "%u: not in 0..15\n", chan);
				exit(1);
			}
			all = 0;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	if (sscanf(*argv, "%u", &vol) != 1) {
		fprintf(stderr, "%s: not a number\n", *argv);
		exit(1);
	}
	if (vol > 127) {
		fprintf(stderr, "%u: not in 0..127\n", vol);
		exit(1);
	}

	/*
	 * open the MIDI device, and send volume messages
	 */
	hdl = mio_open(dev, MIO_OUT, 0);
	if (hdl == NULL) {
		fprintf(stderr, "%s: couldn't open MIDI device\n", dev);
		exit(1);
	}
	if (all) {
		for (chan = 0; chan < 16; chan++)
			setvol(hdl, chan, vol);
	} else
		setvol(hdl, chan, vol);
	mio_close(hdl);
	return 0;
}
