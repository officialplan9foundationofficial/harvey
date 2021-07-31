#include <u.h>
#include <libc.h>
#include <thread.h>
#include "usb.h"
#include "usbfs.h"
#include "serial.h"
#include "ucons.h"
#include "prolific.h"

typedef struct Parg Parg;
enum {
	Arglen = 80,
};



static void
usage(void)
{
	fprint(2, "usage: %s [-d] [-a n] [dev...]\n", argv0);
	threadexitsall("usage");
}

static int
matchserial(char *info, void*)
{
	if(uconsmatch(info) == 0 || plmatch(info) == 0)
		return 0;
	return -1;
}

void
threadmain(int argc, char **argv)
{
	char *mnt, *srv, *as, *ae;
	char args[Arglen];

	mnt = "/dev";
	srv = nil;

	quotefmtinstall();
	ae = args + sizeof args;
	as = seprint(args, ae, "serial");
	ARGBEGIN{
	case 'D':
		usbfsdebug++;
		break;
	case 'd':
		usbdebug++;
		as = seprint(as, ae, " -d");
		break;
	case 'm':
		mnt = EARGF(usage());
		break;
	case 's':
		srv = EARGF(usage());
		break;
	default:
		usage();
		break;
	}ARGEND;

	rfork(RFNOTEG);
	fmtinstall('U', Ufmt);
	threadsetgrp(threadid());

	usbfsinit(srv, mnt, &usbdirfs, MAFTER|MCREATE);
	startdevs(args, argv, argc, matchserial, nil, serialmain);
	threadexits(nil);
}
