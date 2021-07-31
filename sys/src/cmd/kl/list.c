#include "l.h"

void
listinit(void)
{

	fmtinstall('A', Aconv);
	fmtinstall('D', Dconv);
	fmtinstall('P', Pconv);
	fmtinstall('S', Sconv);
	fmtinstall('N', Nconv);
}

void
prasm(Prog *p)
{
	print("%P\n", p);
}

int
Pconv(void *o, int f1, int f2, int f3, int chr)
{
	char str[STRINGSZ];
	Prog *p;
	int a;

	USED(chr);
	p = *(Prog**)o;
	curp = p;
	a = p->as;
	if(a == ADATA)
		sprint(str, "	%A	%D/%d,%D", a, &p->from, p->reg, &p->to);
	else
	if(p->reg == NREG)
		sprint(str, "	%A	%D,%D", a, &p->from, &p->to);
	else
	if(p->from.type == D_OREG) {
		sprint(str, "	%A	%ld(R%d+R%d),%D", a,
			p->from.offset, p->from.reg, p->reg, &p->to);
	} else
	if(p->to.type == D_OREG) {
		sprint(str, "	%A	%D,%ld(R%d+R%d)", a,
				&p->from, p->to.offset, p->to.reg, p->reg);
	} else
	if(p->from.type == D_FREG)
		sprint(str, "	%A	%D,F%d,%D", a, &p->from, p->reg, &p->to);
	else
		sprint(str, "	%A	%D,R%d,%D", a, &p->from, p->reg, &p->to);
	strconv(str, f1, f2, f3);
	return sizeof(p);
}

int
Aconv(void *o, int f1, int f2, int f3, int chr)
{
	char *s;
	int a;

	USED(chr);
	a = *(int*)o;
	s = "???";
	if(a >= AXXX && a <= AEND)
		s = anames[a];
	strconv(s, f1, f2, f3);
	return sizeof(a);
}

int
Dconv(void *o, int f1, int f2, int f3, int chr)
{
	char str[STRINGSZ];
	Adr *a;
	long v;

	USED(chr);
	a = *(Adr**)o;
	switch(a->type) {

	default:
		sprint(str, "GOK-type(%d)", a->type);
		break;

	case D_NONE:
		str[0] = 0;
		if(a->name != D_NONE || a->reg != NREG || a->sym != S)
			sprint(str, "%N(R%d)(NONE)", a, a->reg);
		break;

	case D_CONST:
		if(a->reg != NREG)
			sprint(str, "$%N(R%d)", a, a->reg);
		else
			sprint(str, "$%N", a);
		break;

	case D_ASI:
		if(a->reg != NREG)
			sprint(str, "(R%d,%ld)", a->reg, a->offset);
		else
			sprint(str, "(R%d,%ld)", 0, a->offset);
		break;

	case D_OREG:
		if(a->reg != NREG)
			sprint(str, "%N(R%d)", a, a->reg);
		else
			sprint(str, "%N", a);
		break;

	case D_REG:
		sprint(str, "R%d", a->reg);
		if(a->name != D_NONE || a->sym != S)
			sprint(str, "%N(R%d)(REG)", a, a->reg);
		break;

	case D_FREG:
		sprint(str, "F%d", a->reg);
		if(a->name != D_NONE || a->sym != S)
			sprint(str, "%N(F%d)(REG)", a, a->reg);
		break;

	case D_CREG:
		sprint(str, "C%d", a->reg);
		if(a->name != D_NONE || a->sym != S)
			sprint(str, "%N(C%d)(REG)", a, a->reg);
		break;

	case D_PREG:
		sprint(str, "P%d", a->reg);
		if(a->name != D_NONE || a->sym != S)
			sprint(str, "%N(P%d)(REG)", a, a->reg);
		break;

	case D_BRANCH:
		if(curp->cond != P) {
			v = curp->cond->pc;
			if(v >= INITTEXT)
				v -= INITTEXT-HEADR;
			if(a->sym != S)
				sprint(str, "%s+%.5lux(BRANCH)", a->sym->name, v);
			else
				sprint(str, "%.5lux(BRANCH)", v);
		} else
			if(a->sym != S)
				sprint(str, "%s+%ld(APC)", a->sym->name, a->offset);
			else
				sprint(str, "%ld(APC)", a->offset);
		break;

	case D_FCONST:
		sprint(str, "$%lux-%lux", a->ieee.h, a->ieee.l);
		break;

	case D_SCONST:
		sprint(str, "$\"%S\"", a->sval);
		break;
	}
	strconv(str, f1, f2, f3);
	return sizeof(a);
}

int
Nconv(void *o, int f1, int f2, int f3, int chr)
{
	char str[STRINGSZ];
	Adr *a;
	Sym *s;

	USED(chr);
	a = *(Adr**)o;
	s = a->sym;
	if(s == S) {
		sprint(str, "%ld", a->offset);
		goto out;
	}
	switch(a->name) {
	default:
		sprint(str, "GOK-name(%d)", a->name);
		break;

	case D_EXTERN:
		sprint(str, "%s+%ld(SB)", s->name, a->offset);
		break;

	case D_STATIC:
		sprint(str, "%s<>+%ld(SB)", s->name, a->offset);
		break;

	case D_AUTO:
		sprint(str, "%s-%ld(SP)", s->name, -a->offset);
		break;

	case D_PARAM:
		sprint(str, "%s+%ld(FP)", s->name, a->offset);
		break;
	}
out:
	strconv(str, f1, f2, f3);
	return sizeof(a);
}

int
Sconv(void *o, int f1, int f2, int f3, int chr)
{
	int i, c;
	char str[STRINGSZ], *p, *a;

	USED(chr);
	a = *(char**)o;
	p = str;
	for(i=0; i<sizeof(long); i++) {
		c = a[i] & 0xff;
		if(c >= 'a' && c <= 'z' ||
		   c >= 'A' && c <= 'Z' ||
		   c >= '0' && c <= '9' ||
		   c == ' ' || c == '%') {
			*p++ = c;
			continue;
		}
		*p++ = '\\';
		switch(c) {
		case 0:
			*p++ = 'z';
			continue;
		case '\\':
		case '"':
			*p++ = c;
			continue;
		case '\n':
			*p++ = 'n';
			continue;
		case '\t':
			*p++ = 't';
			continue;
		}
		*p++ = (c>>6) + '0';
		*p++ = ((c>>3) & 7) + '0';
		*p++ = (c & 7) + '0';
	}
	*p = 0;
	strconv(str, f1, f2, f3);
	return sizeof(a);
}

void
diag(char *a, ...)
{
	char buf[STRINGSZ], *tn;

	tn = "??none??";
	if(curtext != P && curtext->from.sym != S)
		tn = curtext->from.sym->name;
	doprint(buf, buf+sizeof(buf), a, &(&a)[1]);	/* ugly */
	print("%s: %s\n", tn, buf);

	nerrors++;
	if(nerrors > 10) {
		print("too many errors\n");
		errorexit();
	}
}
