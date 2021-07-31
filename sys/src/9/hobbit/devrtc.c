#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

#define RTCREAD		(0x40)
#define RTCWRITE	(0x80)

typedef struct {
	int	sec;
	int	min;
	int	hour;
	int	wday;
	int	mday;
	int	mon;
	int	year;
} Rtc;

enum {
	Qrtc =	1,
	Qnvram,
};

static QLock rtclock;		/* mutex on clock operations */

static Dirtab rtcdir[] = {
	"rtc",		{ Qrtc, 0 },	0,	0666,
	"nvram",	{ Qnvram, 0 },	0,	0666,
};
#define	NRTC	(sizeof(rtcdir)/sizeof(rtcdir[0]))

#define bcd2dec(bcd)	(((((bcd)>>4) & 0x0F) * 10) + ((bcd) & 0x0F))
#define dec2bcd(dec)	((((dec)/10)<<4)|((dec)%10))

#define SEC2MIN		60L
#define SEC2HOUR	(60L*SEC2MIN)
#define SEC2DAY		(24L*SEC2HOUR)

/*
 * days per month plus days/year
 */
static int dmsize[] = {
	365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static int ldmsize[] = {
	366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/*
 * return the days/month for the given year
 */
static int *
yrsize(int yr)
{
	if((yr % 4) == 0)
		return ldmsize;
	else
		return dmsize;
}

/*
 * compute rtc from seconds since Jan 1 1970
 */
static void
sec2rtc(ulong secs, Rtc *rtc)
{
	int d;
	long hms, day;
	int *d2m;

	/*
	 * break initial number into days
	 */
	hms = secs % SEC2DAY;
	day = secs / SEC2DAY;
	if(hms < 0) {
		hms += SEC2DAY;
		day -= 1;
	}

	/*
	 * generate hours:minutes:seconds
	 */
	rtc->sec = hms % 60;
	d = hms / 60;
	rtc->min = d % 60;
	d /= 60;
	rtc->hour = d;

	/*
	 * year number
	 */
	if(day >= 0)
		for(d = 1970; day >= *yrsize(d); d++)
			day -= *yrsize(d);
	else
		for (d = 1970; day < 0; d--)
			day += *yrsize(d-1);
	rtc->year = d;

	/*
	 * generate month
	 */
	d2m = yrsize(rtc->year);
	for(d = 1; day >= d2m[d]; d++)
		day -= d2m[d];
	rtc->mday = day + 1;
	rtc->mon = d;

	return;
}

/*
 *  compute seconds since Jan 1 1970
 */
static ulong
rtc2sec(Rtc *rtc)
{
	ulong secs;
	int i;
	int *d2m;

	secs = 0;

	/*
	 *  seconds per year
	 */
	for(i = 1970; i < rtc->year; i++){
		d2m = yrsize(i);
		secs += d2m[0] * SEC2DAY;
	}

	/*
	 *  seconds per month
	 */
	d2m = yrsize(rtc->year);
	for(i = 1; i < rtc->mon; i++)
		secs += d2m[i] * SEC2DAY;

	secs += (rtc->mday-1) * SEC2DAY;
	secs += rtc->hour * SEC2HOUR;
	secs += rtc->min * SEC2MIN;
	secs += rtc->sec;

	return secs;
}

static void
setrtc(Rtc *rtc)
{
	struct RTCdev *dev = RTCADDR;

	dev->control |= RTCWRITE;
	dev->year = dec2bcd(rtc->year % 100);
	dev->mon = dec2bcd(rtc->mon);
	dev->mday = dec2bcd(rtc->mday);
	dev->hour = dec2bcd(rtc->hour);
	dev->min = dec2bcd(rtc->min);
	dev->sec = dec2bcd(rtc->sec);
	dev->control &= ~RTCWRITE;
}

static long
rtctime(void)
{
	struct RTCdev *dev = RTCADDR;
	Rtc rtc;

	dev->control |= RTCREAD;
	rtc.sec = bcd2dec(dev->sec) & 0x7F;
	rtc.min = bcd2dec(dev->min & 0x7F);
	rtc.hour = bcd2dec(dev->hour & 0x3F);
	rtc.mday = bcd2dec(dev->mday & 0x3F);
	rtc.mon = bcd2dec(dev->mon & 0x3F);
	rtc.year = bcd2dec(dev->year);
	dev->control &= ~RTCREAD;

	if(rtc.mon < 1 || rtc.mon > 12)
		return 0;
	/*
	 *  the world starts Jan 1 1970
	 */
	if(rtc.year < 70)
		rtc.year += 2000;
	else
		rtc.year += 1900;

	return rtc2sec(&rtc);
}

void
rtcreset(void)
{
}

void
rtcinit(void)
{
}

Chan *
rtcattach(char *spec)
{
	return devattach('r', spec);
}

Chan *
rtcclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int	 
rtcwalk(Chan *c, char *name)
{
	return devwalk(c, name, rtcdir, NRTC, devgen);
}

void	 
rtcstat(Chan *c, char *dp)
{
	devstat(c, dp, rtcdir, NRTC, devgen);
}

Chan*
rtcopen(Chan *c, int omode)
{
	omode = openmode(omode);
	switch(c->qid.path){
	case Qrtc:
		if(strcmp(u->p->user, eve)!=0 && omode!=OREAD)
			error(Eperm);
		break;
	case Qnvram:
		if(strcmp(u->p->user, eve)!=0 || !cpuserver)
			error(Eperm);
	}
	return devopen(c, omode, rtcdir, NRTC, devgen);
}

void	 
rtccreate(Chan *c, char *name, int omode, ulong perm)
{
	USED(c, name, omode, perm);
	error(Eperm);
}

void	 
rtcclose(Chan *c)
{
	USED(c);
}

long	 
rtcread(Chan *c, void *buf, long n, ulong offset)
{
	Nvram *nv;
	ulong t, ot;
	int i;

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, rtcdir, NRTC, devgen);

	switch(c->qid.path){

	case Qrtc:
		qlock(&rtclock);
		t = rtctime();
		do{
			ot = t;
			t = rtctime();	/* make sure there's no skew */
		}while(t != ot);
		qunlock(&rtclock);
		n = readnum(offset, buf, n, t, 12);
		return n;

	case Qnvram:
		if(offset > NVRAMLEN)
			return 0;
		if(n > NVRAMLEN - offset)
			n = NVRAMLEN - offset;
		nv = NVRAMADDR;
		qlock(&rtclock);
		for(i = 0; i < n; i++)
			((char*)buf)[i] = nv[i+offset].val;
		qunlock(&rtclock);
		return n;
	}
	error(Egreg);
	return -1;
}

long	 
rtcwrite(Chan *c, void *buf, long n, ulong offset)
{
	Nvram *nv;
	Rtc rtc;
	ulong secs;
	char *cp, *ep;
	int i;

	switch(c->qid.path){

	case Qrtc:
		if(offset)
			error(Ebadarg);
		/*
		 *  read the time
		 */
		cp = ep = buf;
		ep += n;
		while(cp < ep){
			if(*cp>='0' && *cp<='9')
				break;
			cp++;
		}
		secs = strtoul(cp, 0, 0);
		/*
		 *  convert to bcd
		 */
		sec2rtc(secs, &rtc);
		/*
		 * write it
		 */
		qlock(&rtclock);
		setrtc(&rtc);
		qunlock(&rtclock);
		return n;

	case Qnvram:
		if(offset > NVRAMLEN)
			return 0;
		if(n > NVRAMLEN - offset)
			n = NVRAMLEN - offset;
		nv = NVRAMADDR;
		qlock(&rtclock);
		for(i = 0; i < n; i++)
			nv[i+offset].val = ((char*)buf)[i];
		qunlock(&rtclock);
		return n;
	}
	error(Egreg);
	return -1;
}

void	 
rtcremove(Chan *c)
{
	USED(c);
	error(Eperm);
}

void	 
rtcwstat(Chan *c, char *dp)
{
	USED(c, dp);
	error(Eperm);
}
