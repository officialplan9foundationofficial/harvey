#include	"l.h"

static struct {
	ulong	start;
	ulong	size;
} pool;

void	checkpool(Prog*);
void 	flushpool(Prog*, int);

void
span(void)
{
	Prog *p;
	Sym *setext;
	Optab *o;
	int m, bflag;
	long c, otxt;

	if(debug['v'])
		Bprint(&bso, "%5.2f span\n", cputime());
	Bflush(&bso);

	bflag = 0;
	c = INITTEXT;
	otxt = c;
	for(p = firstp; p != P; p = p->link) {
		p->pc = c;
		o = oplook(p);
		m = o->size;
		if(m == 0) {
			if(p->as == ATEXT) {
				curtext = p;
				autosize = p->to.offset + 4;
				if(p->from.sym != S)
					p->from.sym->value = c;
				/* need passes to resolve branches */
				if(c-otxt >= 1L<<17)
					bflag = 1;
				otxt = c;
				continue;
			}
			diag("zero-width instruction\n%P\n", p);
			continue;
		}
		if(o->lit) {
			switch(o->lit) {
			case LFROM:
				addpool(p, &p->from);
				break;
			case LTO:
				addpool(p, &p->to);
				break;
			case LPOOL:
				flushpool(p, 0);
				break;
			}
		}
		c += m;
		if(blitrl)
			checkpool(p);
	}

	/*
	 * if any procedure is large enough to
	 * generate a large SBRA branch, then
	 * generate extra passes putting branches
	 * around jmps to fix. this is rare.
	 */
	while(bflag) {
		if(debug['v'])
			Bprint(&bso, "%5.2f span1\n", cputime());
		bflag = 0;
		c = INITTEXT;
		for(p = firstp; p != P; p = p->link) {
			p->pc = c;
			o = oplook(p);
/* very larg branches
			if(o->type == 6 && p->cond) {
				otxt = p->cond->pc - c;
				if(otxt < 0)
					otxt = -otxt;
				if(otxt >= (1L<<17) - 10) {
					q = prg();
					q->link = p->link;
					p->link = q;
					q->as = AB;
					q->to.type = D_BRANCH;
					q->cond = p->cond;
					p->cond = q;
					q = prg();
					q->link = p->link;
					p->link = q;
					q->as = AB;
					q->to.type = D_BRANCH;
					q->cond = q->link->link;
					bflag = 1;
				}
			}
 */
			m = o->size;
			if(m == 0) {
				if(p->as == ATEXT) {
					curtext = p;
					autosize = p->to.offset + 4;
					if(p->from.sym != S)
						p->from.sym->value = c;
					continue;
				}
				diag("zero-width instruction\n%P\n", p);
				continue;
			}
			c += m;
		}
	}
	c = rnd(c, 8);

	setext = lookup("etext", 0);
	if(setext != S) {
		setext->value = c;
		textsize = c - INITTEXT;
	}
	if(INITRND)
		INITDAT = rnd(c, INITRND);
	if(debug['v'])
		Bprint(&bso, "tsize = %lux\n", textsize);
	Bflush(&bso);
}

/*
 * when the first reference to the literal pool threatens
 * to go out of range of a 12-bit PC-relative offset,
 * drop the pool now, and branch round it.
 * this happens only in extended basic blocks that exceed 4k.
 */
void
checkpool(Prog *p)
{
	if(pool.size >= 0xffc || immaddr((p->pc+4)+4+pool.size - pool.start+8) == 0)
		flushpool(p, 1);
}

void
flushpool(Prog *p, int skip)
{
	Prog *q;

	if(blitrl) {
		if(skip){
			print("note: flush literal pool at %lux: len=%ld ref=%lux\n", p->pc+4, pool.size, pool.start);
			q = prg();
			q->as = AB;
			q->to.type = D_BRANCH;
			q->cond = p->link;
			q->link = blitrl;
			blitrl = q;
		}
		elitrl->link = p->link;
		p->link = blitrl;
		blitrl = 0;	/* BUG: should refer back to values until out-of-range */
		elitrl = 0;
		pool.size = 0;
		pool.start = 0;
	}
}

void
addpool(Prog *p, Adr *a)
{
	Prog *q, t;
	int c;

	c = aclass(a);

	t = zprg;
	t.as = AWORD;

	switch(c) {
	default:
		t.to = *a;
		break;
	case C_LOREG:
	case C_ROREG:
	case C_LAUTO:
	case C_LACON:
		t.to.type = D_CONST;
		t.to.offset = instoffset;
		break;
	}

	for(q = blitrl; q != P; q = q->link)	/* could hash on t.t0.offset */
		if(memcmp(&q->to, &t.to, sizeof(t.to)) == 0) {
			p->cond = q;
			return;
		}

	q = prg();
	*q = t;
	q->pc = pool.size;

	if(blitrl == P) {
		blitrl = q;
		pool.start = p->pc;
	} else
		elitrl->link = q;
	elitrl = q;
	pool.size += 4;

	p->cond = q;
}

void
xdefine(char *p, int t, long v)
{
	Sym *s;

	s = lookup(p, 0);
	if(s->type == 0 || s->type == SXREF) {
		s->type = t;
		s->value = v;
	}
}

long
regoff(Adr *a)
{

	instoffset = 0;
	aclass(a);
	return instoffset;
}

long
immrot(ulong v)
{
	int i;

	for(i=0; i<16; i++) {
		if((v & ~0xff) == 0)
			return (i<<8) | v | (1<<25);
		v = (v<<2) | (v>>30);
	}
	return 0;
}

long
immaddr(long v)
{

	if(v >= 0 && v <= 0xfff)
		return (v & 0xfff) |
			(1<<24) |	/* pre indexing */
			(1<<23);	/* pre indexing, up */
	if(v >= -0xfff && v < 0)
		return (-v & 0xfff) |
			(1<<24);	/* pre indexing */
	return 0;
}

int
immfloat(long v)
{
	return (v & 0xC03) == 0;	/* offset will fit in floating-point load/store */
}

int
aclass(Adr *a)
{
	Sym *s;
	int t;

	switch(a->type) {
	case D_NONE:
		return C_NONE;

	case D_REG:
		return C_REG;

	case D_SHIFT:
		return C_SHIFT;

	case D_FREG:
		return C_FREG;

	case D_FPCR:
		return C_FCR;

	case D_OREG:
		switch(a->name) {
		case D_EXTERN:
		case D_STATIC:
			if(a->sym == 0 || a->sym->name == 0) {
				print("null sym external\n");
				print("%D\n", a);
				return C_GOK;
			}
			t = a->sym->type;
			if(t == 0 || t == SXREF) {
				diag("undefined external: %s in %s\n",
					a->sym->name, TNAME);
				a->sym->type = SDATA;
			}
			instoffset = a->sym->value + a->offset - BIG;
			t = immaddr(instoffset);
			if(t) {
				if(immfloat(t))
					return C_FEXT;
				return C_SEXT;
			}
			return C_LEXT;
		case D_AUTO:
			instoffset = autosize + a->offset;
			t = immaddr(instoffset);
			if(t){
				if(immfloat(t))
					return C_FAUTO;
				return C_SAUTO;
			}
			return C_LAUTO;

		case D_PARAM:
			instoffset = autosize + a->offset + 4L;
			t = immaddr(instoffset);
			if(t){
				if(immfloat(t))
					return C_FAUTO;
				return C_SAUTO;
			}
			return C_LAUTO;
		case D_NONE:
			instoffset = a->offset;
			t = immaddr(instoffset);
			if(t) {
				if(immfloat(t))
					return C_FOREG; /* n.b. that it will also satisfy immrot */
				t = immrot(instoffset);
				if(t)
					return C_BOREG;
				return C_SOREG;
			}
			t = immrot(instoffset);
			if(t)
				return C_ROREG;
			return C_LOREG;
		}
		return C_GOK;

	case D_PSR:
		return C_PSR;

	case D_OCONST:
		switch(a->name) {
		case D_EXTERN:
		case D_STATIC:
			s = a->sym;
			t = s->type;
			if(t == 0 || t == SXREF) {
				diag("undefined external: %s in %s\n",
					s->name, TNAME);
				s->type = SDATA;
			}
			instoffset = s->value + a->offset + INITDAT;
			if(s->type == STEXT || s->type == SLEAF)
				instoffset = s->value + a->offset;
			return C_LCON;
		}
		return C_GOK;

	case D_FCONST:
		return C_FCON;

	case D_CONST:
		switch(a->name) {

		case D_NONE:
			instoffset = a->offset;
			if(a->reg != NREG)
				goto aconsize;

			t = immrot(instoffset);
			if(t)
				return C_RCON;
			t = immrot(~instoffset);
			if(t)
				return C_NCON;
			return C_LCON;

		case D_EXTERN:
		case D_STATIC:
			s = a->sym;
			if(s == S)
				break;
			t = s->type;
			switch(t) {
			case 0:
			case SXREF:
				diag("undefined external: %s in %s\n",
					s->name, TNAME);
				s->type = SDATA;
				break;
			case SCONST:
			case STEXT:
			case SLEAF:
				instoffset = s->value + a->offset;
				return C_LCON;
			}
			instoffset = s->value + a->offset - BIG;
			t = immrot(instoffset);
			if(t && instoffset != 0)
				return C_RECON;
			instoffset = s->value + a->offset + INITDAT;
			return C_LCON;

		case D_AUTO:
			instoffset = autosize + a->offset;
			goto aconsize;

		case D_PARAM:
			instoffset = autosize + a->offset + 4L;
		aconsize:
			t = immrot(instoffset);
			if(t)
				return C_RACON;
			return C_LACON;
		}
		return C_GOK;

	case D_BRANCH:
		return C_SBRA;
	}
	return C_GOK;
}

Optab*
oplook(Prog *p)
{
	int a1, a2, a3, r;
	char *c1, *c3;
	Optab *o, *e;

	a1 = p->optab;
	if(a1)
		return optab+(a1-1);
	a1 = p->from.class;
	if(a1 == 0) {
		a1 = aclass(&p->from) + 1;
		p->from.class = a1;
	}
	a1--;
	a3 = p->to.class;
	if(a3 == 0) {
		a3 = aclass(&p->to) + 1;
		p->to.class = a3;
	}
	a3--;
	a2 = C_NONE;
	if(p->reg != NREG)
		a2 = C_REG;
	r = p->as;
	o = oprange[r].start;
	if(o == 0) {
		a1 = opcross[repop[r]][a1][a2][a3];
		if(a1) {
			p->optab = a1+1;
			return optab+a1;
		}
		o = oprange[r].stop; /* just generate an error */
	}
	if(0) {
		print("oplook %A %d %d %d\n",
			(int)p->as, a1, a2, a3);
		print("		%d %d\n", p->from.type, p->to.type);
	}
	e = oprange[r].stop;
	c1 = xcmp[a1];
	c3 = xcmp[a3];
	for(; o<e; o++)
		if(o->a2 == a2)
		if(c1[o->a1])
		if(c3[o->a3]) {
			p->optab = (o-optab)+1;
			return o;
		}
	diag("illegal combination %A %d %d %d\n",
		p->as, a1, a2, a3);
	prasm(p);
	if(o == 0)
		o = optab;
	return o;
}

int
cmp(int a, int b)
{

	if(a == b)
		return 1;
	switch(a) {
	case C_LCON:
		if(b == C_RCON || b == C_NCON)
			return 1;
		break;
	case C_LACON:
		if(b == C_RACON)
			return 1;
		break;
	case C_LECON:
		if(b == C_RECON)
			return 1;
		break;
	case C_SEXT:
		if(b == C_FEXT)
			return 1;
		break;
	case C_LEXT:
		if(b == C_SEXT || b == C_FEXT)
			return 1;
		break;
	case C_SAUTO:
		if(b == C_FAUTO)
			return 1;
		break;
	case C_LAUTO:
		if(b == C_SAUTO || b == C_FAUTO)
			return 1;
		break;
	case C_LOREG:
		if(b == C_BOREG || b == C_ROREG || b == C_SOREG || b == C_FOREG)
			return 1;
		break;
	case C_SOREG:
		if(b == C_BOREG || b == C_FOREG)
			return 1;
		break;
	case C_ROREG:
		if(b == C_BOREG || b == C_FOREG)
			return 1;
		break;
	case C_LBRA:
		if(b == C_SBRA)
			return 1;
		break;
	}
	return 0;
}

int
ocmp(const void *a1, const void *a2)
{
	Optab *p1, *p2;
	int n;

	p1 = (Optab*)a1;
	p2 = (Optab*)a2;
	n = p1->as - p2->as;
	if(n)
		return n;
	n = p1->a1 - p2->a1;
	if(n)
		return n;
	n = p1->a2 - p2->a2;
	if(n)
		return n;
	n = p1->a3 - p2->a3;
	if(n)
		return n;
	return 0;
}

void
buildop(void)
{
	int i, n, r;

	for(i=0; i<32; i++)
		for(n=0; n<32; n++)
			xcmp[i][n] = cmp(n, i);
	for(n=0; optab[n].as != AXXX; n++)
		;
	qsort(optab, n, sizeof(optab[0]), ocmp);
	for(i=0; i<n; i++) {
		r = optab[i].as;
		oprange[r].start = optab+i;
		while(optab[i].as == r)
			i++;
		oprange[r].stop = optab+i;
		i--;

		switch(r)
		{
		default:
			diag("unknown op in build: %A\n", r);
			errorexit();
		case AADD:
			oprange[AAND] = oprange[r];
			oprange[AEOR] = oprange[r];
			oprange[ASUB] = oprange[r];
			oprange[ARSB] = oprange[r];
			oprange[AADC] = oprange[r];
			oprange[ASBC] = oprange[r];
			oprange[ARSC] = oprange[r];
			oprange[AORR] = oprange[r];
			oprange[ABIC] = oprange[r];
			break;
		case ACMP:
			oprange[ATST] = oprange[r];
			oprange[ATEQ] = oprange[r];
			oprange[ACMN] = oprange[r];
			break;
		case AMVN:
			break;
		case ABEQ:
			oprange[ABNE] = oprange[r];
			oprange[ABCS] = oprange[r];
			oprange[ABHS] = oprange[r];
			oprange[ABCC] = oprange[r];
			oprange[ABLO] = oprange[r];
			oprange[ABMI] = oprange[r];
			oprange[ABPL] = oprange[r];
			oprange[ABVS] = oprange[r];
			oprange[ABVC] = oprange[r];
			oprange[ABHI] = oprange[r];
			oprange[ABLS] = oprange[r];
			oprange[ABGE] = oprange[r];
			oprange[ABLT] = oprange[r];
			oprange[ABGT] = oprange[r];
			oprange[ABLE] = oprange[r];
			break;
		case ASLL:
			oprange[ASRL] = oprange[r];
			oprange[ASRA] = oprange[r];
			break;
		case AMUL:
			oprange[AMULU] = oprange[r];
			break;
		case ADIV:
			oprange[AMOD] = oprange[r];
			oprange[AMODU] = oprange[r];
			oprange[ADIVU] = oprange[r];
			break;
		case AMOVW:
		case AMOVB:
		case AMOVBU:
		case AMOVH:
		case AMOVHU:
			break;
		case ASWPW:
			oprange[ASWPBU] = oprange[r];
			break;
		case AB:
		case ABL:
		case ASWI:
		case AWORD:
		case AMOVM:
		case ARFE:
		case ATEXT:
		case ACASE:
		case ABCASE:
			break;
		case AADDF:
			oprange[AADDD] = oprange[r];
			oprange[ASUBF] = oprange[r];
			oprange[ASUBD] = oprange[r];
			oprange[AMULF] = oprange[r];
			oprange[AMULD] = oprange[r];
			oprange[ADIVF] = oprange[r];
			oprange[ADIVD] = oprange[r];
			oprange[AMOVFD] = oprange[r];
			oprange[AMOVDF] = oprange[r];
			break;
			
		case ACMPF:
			oprange[ACMPD] = oprange[r];
			break;

		case AMOVF:
			oprange[AMOVD] = oprange[r];
			break;

		case AMOVFW:
			oprange[AMOVWF] = oprange[r];
			oprange[AMOVWD] = oprange[r];
			oprange[AMOVDW] = oprange[r];
			break;
		}
	}
}

void
buildrep(int x, int as)
{
	Opcross *p;
	Optab *e, *s, *o;
	int a1, a2, a3, n;

	if(C_NONE != 0 || C_REG != 1 || C_GOK >= 32 || x >= nelem(opcross)) {
		diag("assumptions fail in buildrep");
		errorexit();
	}
	repop[as] = x;
	p = (opcross + x);
	s = oprange[as].start;
	e = oprange[as].stop;
	for(o=e-1; o>=s; o--) {
		n = o-optab;
		for(a2=0; a2<2; a2++) {
			if(a2) {
				if(o->a2 == C_NONE)
					continue;
			} else
				if(o->a2 != C_NONE)
					continue;
			for(a1=0; a1<32; a1++) {
				if(!xcmp[a1][o->a1])
					continue;
				for(a3=0; a3<32; a3++)
					if(xcmp[a3][o->a3])
						(*p)[a1][a2][a3] = n;
			}
		}
	}
	oprange[as].start = 0;
}
