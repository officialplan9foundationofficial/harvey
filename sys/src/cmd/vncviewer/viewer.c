#include "vnc.h"

char*	encodings = "copyrect hextile corre rre raw";
int		bpp12;
int		shared;
int		verbose;
Vnc*		vnc;

enum { Pkbd, Pmouse, Pdraw, Npid };
int 		pid[Npid];

/*
 * like sysfatal(2) but sends a message to the
 * others before dying.  also serves as exits.
 */
void
sysfatal(char *fmt, ...)
{
	char buf[1024];
	va_list arg;
	int i, mypid;

	mypid = getpid();
	for(i=0; i<Npid; i++)
		if(mypid != pid[i])
			postnote(PNPROC, pid[i], "hangup");

	if(fmt) {
		va_start(arg, fmt);
		doprint(buf, buf+sizeof(buf), fmt, arg);
		va_end(arg);
		if(argv0)
			fprint(2, "%s: %s\n", argv0, buf);
		else
			fprint(2, "%s\n", buf);
		exits(buf);
	} else
		exits(nil);
}

void
usage(void)
{
	fprint(2, "usage: vncviewer [-e encodings] [-csv] host[:n]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int dfd;
	char *addr;

	ARGBEGIN{
	case 'c':
		bpp12 = 1;
		break;
	case 'e':
		encodings = ARGF();
		break;
	case 's':
		shared = 1;
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage();
	}ARGEND;

	if(argc != 1)
		usage();

	addr = netmkvncaddr(argv[0]);
	if((dfd = dial(addr, nil, nil, nil)) < 0)
		sysfatal("cannot dial %s: %r", addr);

	vnc = openvnc(dfd);

	if(vncauth(vnc) < 0)
		sysfatal("authentication failure: %r");

	initdraw(0, 0, "vncviewer");

	display->locking = 1;
	unlockdisplay(display);

	choosecolor(vnc);
	sendencodings(vnc);

	initwindow(vnc);

	switch(rfork(RFPROC|RFMEM)){
	case -1:
		sysfatal("rfork: %r");
	default:
		break;
	case 0:
		pid[Pdraw] = getpid();
		readfromserver(vnc);
		sysfatal(nil);
	}

	switch(rfork(RFPROC|RFMEM)){
	case -1:
		sysfatal("rfork: %r");
	default:
		break;
	case 0:
		pid[Pkbd] = getpid();
		readkbd(vnc);
		sysfatal(nil);
	}

	pid[Pmouse] = getpid();
	readmouse(vnc);
	sysfatal(nil);
}
