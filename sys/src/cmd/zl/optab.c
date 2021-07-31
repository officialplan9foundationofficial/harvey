#include	"l.h"

#define	X	0
/*
 *	encodings:
 *		n	$-16	$-1
 *		c	$0	$15
 *		u	$16	$31	!%4
 *		x	$16	$31	%4
 *		w	$32	$124	%4
 *		s	R0	R124	%4
 *		i	*R0	*R124	%4
 *	imm5	nc
 *	uimm5	cux
 *	wai5	xw	(always goes with imm5)
 */
Optab	optab[] =
/*	   as,		type,  genop, decod	*/
{
	{ AXXX,		TPSUD, 0xff, 0 },
	{ AADD,		TDYAD, 0x23, "cs14->ns14->ss16>>" },
	{ AADD3,	TDYAD, 0x33, "cs15->ns15->ss17>>xs0d>>ws0d>>" },
	{ AADDI,	TDYAD, 0x03, 0 },
	{ AAND,		TDYAD, 0x22, "ss0f>>" },
	{ AAND3,	TDYAD, 0x32, "cs0e->ns0e->" },
	{ AANDI,	TDYAD, 0x02, 0 },
	{ ACALL,	TPCTO, 0x01, "1" },
	{ ACALL1,	TDMON, 0x01, 0 },
	{ ACATCH,	TDMON, 0x08, "1" },
	{ ACMPEQ,	TDYAD, 0x1f, "cs10->ns10->ss13>>" },
	{ ACMPGT,	TDYAD, 0x1d, "cs12->ns12->ss11>>" },
	{ ACMPHI,	TDYAD, 0x1e, 0 },
	{ ACPU,		TNNAD, 0x00, 0 },
	{ ACRET,	TNNAD, 0x05, 0 },
	{ ADATA,	TPSUD, 0xff, 0 },
	{ ADIV,		TDYAD, 0x27, 0 },
	{ ADIV3,	TDYAD, 0x37, 0 },
	{ ADQM,		TDYAD, 0x07, 0 },
	{ AENTER,	TDMON, 0x09, "0" },
	{ AFADD,	TFLOT, 0x2b, 0 },
	{ AFADD3,	TFLOT, 0x3b, 0 },
	{ AFCLASS,	TFLOT, 0x13, 0 },
	{ AFCMPEQ,	TFLOT, 0x1a, 0 },
	{ AFCMPEQN,	TFLOT, 0x1b, 0 },
	{ AFCMPGE,	TFLOT, 0x18, 0 },
	{ AFCMPGT,	TFLOT, 0x19, 0 },
	{ AFCMPN,	TFLOT, 0x1c, 0 },
	{ AFDIV,	TFLOT, 0x2a, 0 },
	{ AFDIV3,	TFLOT, 0x3a, 0 },
	{ AFLOGB,	TFLOT, 0x12, 0 },
	{ AFLUSHD,	TNNAD, 0x06, 0 },
	{ AFLUSHDCE,	TDMON, 0x0d, 0 },
	{ AFLUSHI,	TNNAD, 0x03, 0 },
	{ AFLUSHP,	TNNAD, 0x04, 0 },
	{ AFLUSHPBE,	TDMON, 0x0c, 0 },
	{ AFLUSHPTE,	TDMON, 0x0b, 0 },
	{ AFMOV,	TFLOT, 0x11, 0 },
	{ AFMUL,	TFLOT, 0x29, 0 },
	{ AFMUL3,	TFLOT, 0x39, 0 },
	{ AFNEXT,	TFLOT, 0x08, 0 },
	{ AFREM,	TFLOT, 0x0b, 0 },
	{ AFSCALB,	TFLOT, 0x09, 0 },
	{ AFSQRT,	TFLOT, 0x10, 0 },
	{ AFSUB,	TFLOT, 0x28, 0 },
	{ AFSUB3,	TFLOT, 0x38, 0 },
	{ AGLOBL,	TPSUD, 0xff, 0 },
	{ AGOK,		TXXXX, 0xff, 0 },
	{ AHISTORY,	TXXXX, 0xff, 0 },
	{ AJMP,		TPCTO, 0x03, 0 },
	{ AJMP1,	TDMON, 0x03, 0 },
	{ AJMPF,	TPCTO, 0x04, 0 },
	{ AJMPF1,	TDMON, 0x04, 0 },
	{ AJMPFN,	TPCTO, 0x04, 0 },
	{ AJMPFY,	TPCTO, 0x05, 0 },
	{ AJMPT,	TPCTO, 0x06, 0 },
	{ AJMPT1,	TDMON, 0x06, 0 },
	{ AJMPTN,	TPCTO, 0x06, 0 },
	{ AJMPTY,	TPCTO, 0x07, 0 },
	{ AKCALL,	TDMON, 0x00, "0" },
	{ AKRET,	TNNAD, 0x01, 0 },
	{ ALDRAA,	TDMON, 0x0a, 0 },
	{ ALONG,	TLONG, 0xff, 0 },
	{ AMOV,		TDYAD, 0x06, "ss18>>is19>>si1a>>ii1b>>cs1c->ns1c->xs0a>>ws0a>>" },
	{ AMOVA,	TDYAD, 0x04, "ss1d>>" },
	{ AMUL,		TDYAD, 0x26, 0 },
	{ AMUL3,	TDYAD, 0x36, 0 },
	{ ANAME,	TPSUD, 0xff, 0 },
	{ ANOP,		TNNAD, 0x02, 0 },
	{ AOR,		TDYAD, 0x21, 0 },
	{ AOR3,		TDYAD, 0x31, 0 },
	{ AORI,		TDYAD, 0x01, 0 },
	{ APOPN,	TDMON, 0x0f, "3" },
	{ AREM,		TDYAD, 0x25, 0 },
	{ AREM3,	TDYAD, 0x35, 0 },
	{ ARETURN,	TDMON, 0x02, "2" },
	{ ASHL,		TDYAD, 0x2e, 0 },
	{ ASHL3,	TDYAD, 0x3e, "cs1e->us1e->xs1e->" },
	{ ASHR,		TDYAD, 0x2c, 0 },
	{ ASHR3,	TDYAD, 0x3c, "cs1f->us1f->xs1f->" },
	{ ASUB,		TDYAD, 0x20, 0 },
	{ ASUB3,	TDYAD, 0x30, 0 },
	{ ATADD,	TDYAD, 0x0c, 0 },
	{ ATESTC,	TNNAD, 0x09, 0 },
	{ ATESTV,	TNNAD, 0x08, 0 },
	{ ATEXT,	TTEXT, 0xff, 0 },
	{ ATSUB,	TDYAD, 0x0d, 0 },
	{ AUDIV,	TDYAD, 0x2f, 0 },
	{ AUREM,	TDYAD, 0x05, 0 },
	{ AUSHR,	TDYAD, 0x2d, 0 },
	{ AUSHR3,	TDYAD, 0x3d, 0 },
	{ AWORD,	TWORD, 0xff, 0 },
	{ AXOR,		TDYAD, 0x24, 0 },
	{ AXOR3,	TDYAD, 0x34, 0 },
	{ AEND,		TPSUD, 0xff, 0 },
	{ AXXX }
};

char	codc1[] =
{
	[S_NEG5]	'n',
	[S_IMM5]	'c',
	[S_UNS5]	'u',
	[S_XOR5]	'x',
	[S_WAI5]	'w',
	[S_STK5]	's',
	[S_ISTK5]	'i',
	[S_IMM16]	'1',
	[S_STK16]	'1',
	[S_ABS16]	'1',
	[S_IMM32]	'2',
	[S_STK32]	'2',
	[S_ABS32]	'2',
	[S_ISTK16]	'1',
	[S_ISTK32]	'2',
};
char	codc2[] =
{
	[S_NEG5]	'0',
	[S_IMM5]	'0',
	[S_UNS5]	'0',
	[S_XOR5]	'0',
	[S_WAI5]	'0',
	[S_STK5]	'1',
	[S_ISTK5]	'3',
	[S_IMM16]	'0',
	[S_STK16]	'1',
	[S_ABS16]	'2',
	[S_IMM32]	'0',
	[S_STK32]	'1',
	[S_ABS32]	'2',
	[S_ISTK16]	'3',
	[S_ISTK32]	'3',
};
