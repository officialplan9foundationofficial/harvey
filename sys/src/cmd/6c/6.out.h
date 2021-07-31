#define	NNAME	20
#define	NSYM	50
#define	NSNAME	8

enum	as
{
	AXXX,
	AADJSP,
	ABYTE,
	ADATA,
	AGLOBL,
	AGOK,
	AHISTORY,
	ALONG,
	ANAME,
	ANOP,
	ARTS,
	ATEXT,
	ASYSCALL,
	ASYSCTL,

	AMOV,
	AMOVA,
	AMOVIB,
	AMOVIS,
	AMOVOB,
	AMOVOS,
	AMOVQ,
	AMOVT,
	AMOVV,

	ASYNMOV,
	ASYNMOVV,
	ASYNMOVQ,

	AADDC,
	AADDI,
	AADDO,
	AALTERBIT,
	AAND,
	AANDNOT,
	AATADD,
	AATMOD,
	ABAL,
	AB,
	ABBC,
	ABBS,
	ABE,
	ABNE,
	ABL,
	ABLE,
	ABG,
	ABGE,
	ABO,
	ABNO,
	ACALL,
	ACALLS,
	ACHKBIT,
	ACLRBIT,
	ACMPI,
	ACMPO,
	ACMPDECI,
	ACMPDECO,
	ACMPINCI,
	ACMPINCO,
	ACMPIBE,
	ACMPIBNE,
	ACMPIBL,
	ACMPIBLE,
	ACMPIBG,
	ACMPIBGE,
	ACMPIBO,
	ACMPIBNO,
	ACMPOBE,
	ACMPOBNE,
	ACMPOBL,
	ACMPOBLE,
	ACMPOBG,
	ACMPOBGE,
	ACONCMPI,
	ACONCMPO,
	ADADDC,
	ADIVI,
	ADIVO,
	ADMOVT,
	ADSUBC,
	AEDIV,
	AEMUL,
	AEXTRACT,
	AFAULTE,
	AFAULTNE,
	AFAULTL,
	AFAULTLE,
	AFAULTG,
	AFAULTGE,
	AFAULTO,
	AFAULTNO,
	AFLUSHREG,
	AFMARK,
	AMARK,
	AMODAC,
	AMODI,
	AMODIFY,
	AMODPC,
	AMODTC,
	AMULI,
	AMULO,
	ANAND,
	ANOR,
	ANOT,
	ANOTAND,
	ANOTBIT,
	ANOTOR,
	AOR,
	AORNOT,
	AREMI,
	AREMO,
	ARET,
	AROTATE,
	ASCANBIT,
	ASCANBYTE,
	ASETBIT,
	ASHLO,
	ASHRO,
	ASHLI,
	ASHRI,
	ASHRDI,
	ASPANBIT,
	ASUBC,
	ASUBI,
	ASUBO,
	ASYNCF,
	ATESTE,
	ATESTNE,
	ATESTL,
	ATESTLE,
	ATESTG,
	ATESTGE,
	ATESTO,
	ATESTNO,
	AXNOR,
	AXOR,

	AEND,
};

enum
{
	D_R0		= 0,

	D_NONE		= D_R0+32,

	D_BRANCH,
	D_EXTERN,
	D_STATIC,
	D_AUTO,
	D_PARAM,
	D_CONST,
	D_FCONST,
	D_SCONST,
	D_ADDR,

	D_FILE,
	D_FILE1,

	D_INDIR,	/* additive */

	T_TYPE		= 1<<0,
	T_INDEX		= 1<<1,
	T_OFFSET	= 1<<2,
	T_FCONST	= 1<<3,
	T_SYM		= 1<<4,
	T_SCONST	= 1<<5,

	REGARG		= 0,
	REGZERO		= D_R0+3,
	REGRET		= D_R0+4,
	REGTMP		= D_R0+27,
	REGSB		= D_R0+28,
	REGSP		= D_R0+29,
	REGLINK		= D_R0+30,

	REGBAD1		= D_R0+0,	/* pfp */
	REGBAD2		= D_R0+1,	/* sp */
	REGBAD3		= D_R0+2,	/* rip */
	REGBAD4		= D_R0+31,	/* fp */
};

/*
 * this is the ranlib header
 */
#define	SYMDEF	"__.SYMDEF"

typedef	struct	rlent	Rlent;
struct	rlent
{
	union
	{
		long	offset;		/* for calculation */
		char	coffset[4];	/* in file little endian */
	};
	char	name[NNAME];
	char	type;
	char	pad[3];
};

/*
 * this is the simulated IEEE floating point
 */
typedef	struct	ieee	Ieee;
struct	ieee
{
	long	l;	/* contains ls-man	0xffffffff */
	long	h;	/* contains sign	0x80000000
				    exp		0x7ff00000
				    ms-man	0x000fffff */
};
