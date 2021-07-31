/*
 * mips definition
 */
#include <u.h>
#include <bio.h>
#include "/mips/include/ureg.h"
#include <mach.h>


#define USER_REG(x)	(0x1000-4-0xA0+(ulong)(x))
#define	FP_REG(x)	(0x0000+4+(x))
#define	SCALLOFF	(0x0000+4+132)

#define	REGOFF(x)	(USER_REG(&((struct Ureg *) 0)->x))

#define SP		REGOFF(sp)
#define PC		REGOFF(pc)
#define	R1		REGOFF(r1)
#define	R31		REGOFF(r31)

#define	MINREG	1
#define	MAXREG	64

struct reglist mipsreglist[] = {
	{"STATUS",	REGOFF(status),	0, 'X'},
	{"CAUSE",	REGOFF(cause),	0, 'X'},
	{"BADVADDR",	REGOFF(badvaddr), 0, 'X'},
	{"TLBVIRT",	REGOFF(tlbvirt), 0, 'X'},
	{"HI",		REGOFF(hi),	0, 'X'},
	{"LO",		REGOFF(lo),	0, 'X'},
#define	FW	5	/* first register we may write */
	{"PC",		PC, 0, 'X'},
	{"SP",		SP, 0, 'X'},
	{"R31",		R31, 0, 'X'},
	{"R30",		REGOFF(r30), 0, 'X'},
	{"R28",		REGOFF(r28), 0, 'X'},
	{"R27",		REGOFF(r27), 0, 'X'},
	{"R26",		REGOFF(r26), 0, 'X'},
	{"R25",		REGOFF(r25), 0, 'X'},
	{"R24",		REGOFF(r24), 0, 'X'},
	{"R23",		REGOFF(r23), 0, 'X'},
	{"R22",		REGOFF(r22), 0, 'X'},
	{"R21",		REGOFF(r21), 0, 'X'},
	{"R20",		REGOFF(r20), 0, 'X'},
	{"R19",		REGOFF(r19), 0, 'X'},
	{"R18",		REGOFF(r18), 0, 'X'},
	{"R17",		REGOFF(r17), 0, 'X'},
	{"R16",		REGOFF(r16), 0, 'X'},
	{"R15",		REGOFF(r15), 0, 'X'},
	{"R14",		REGOFF(r14), 0, 'X'},
	{"R13",		REGOFF(r13), 0, 'X'},
	{"R12",		REGOFF(r12), 0, 'X'},
	{"R11",		REGOFF(r11), 0, 'X'},
	{"R10",		REGOFF(r10), 0, 'X'},
	{"R9",		REGOFF(r9), 0, 'X'},
	{"R8",		REGOFF(r8), 0, 'X'},
	{"R7",		REGOFF(r7), 0, 'X'},
	{"R6",		REGOFF(r6), 0, 'X'},
	{"R5",		REGOFF(r5), 0, 'X'},
	{"R4",		REGOFF(r4), 0, 'X'},
	{"R3",		REGOFF(r3), 0, 'X'},
	{"R2",		REGOFF(r2), 0, 'X'},
	{"R1",		REGOFF(r1), 0, 'X'},
	{"F0",		FP_REG(0x00), 1, 'f'},
	{"F1",		FP_REG(0x04), 1, 'f'},
	{"F2",		FP_REG(0x08), 1, 'f'},
	{"F3",		FP_REG(0x0C), 1, 'f'},
	{"F4",		FP_REG(0x10), 1, 'f'},
	{"F5",		FP_REG(0x14), 1, 'f'},
	{"F6",		FP_REG(0x18), 1, 'f'},
	{"F7",		FP_REG(0x1C), 1, 'f'},
	{"F8",		FP_REG(0x20), 1, 'f'},
	{"F9",		FP_REG(0x24), 1, 'f'},
	{"F10",		FP_REG(0x28), 1, 'f'},
	{"F11",		FP_REG(0x2C), 1, 'f'},
	{"F12",		FP_REG(0x30), 1, 'f'},
	{"F13",		FP_REG(0x34), 1, 'f'},
	{"F14",		FP_REG(0x38), 1, 'f'},
	{"F15",		FP_REG(0x3C), 1, 'f'},
	{"F16",		FP_REG(0x40), 1, 'f'},
	{"F17",		FP_REG(0x44), 1, 'f'},
	{"F18",		FP_REG(0x48), 1, 'f'},
	{"F19",		FP_REG(0x4C), 1, 'f'},
	{"F20",		FP_REG(0x50), 1, 'f'},
	{"F21",		FP_REG(0x54), 1, 'f'},
	{"F22",		FP_REG(0x58), 1, 'f'},
	{"F23",		FP_REG(0x5C), 1, 'f'},
	{"F24",		FP_REG(0x60), 1, 'f'},
	{"F25",		FP_REG(0x64), 1, 'f'},
	{"F26",		FP_REG(0x68), 1, 'f'},
	{"F27",		FP_REG(0x6C), 1, 'f'},
	{"F28",		FP_REG(0x70), 1, 'f'},
	{"F29",		FP_REG(0x74), 1, 'f'},
	{"F30",		FP_REG(0x78), 1, 'f'},
	{"F31",		FP_REG(0x7C), 1, 'f'},
	{"FPCR",	FP_REG(0x80), 1, 'x'},
	{  0 }
};

	/* the machine description */
Mach mmips =
{
	"mips",
	mipsreglist,	/* register set */
	MINREG,		/* minimum register */
	MAXREG,		/* maximum register */
	PC,
	SP,
	R31,
	R1,		/* return reg */
	5,		/* first writable register */
	0x1000,		/* page size */
	0xC0000000,	/* kernel base */
	0x40000000,	/* kernel text mask */
	0,		/* offset of ksp in /proc/proc */
	0,		/* correction to ksp value */
	4,		/* offset of kpc in /proc/proc */
	0,		/* correction to kpc value */
	SCALLOFF,	/* offset to sys call # in ublk */
	4,		/* quantization of pc */
	"setR30",	/* static base register name */
	0,		/* value */
	4,		/* szaddr */
	4,		/* szreg */
	4,		/* szfloat */
	8,		/* szdouble */
};
