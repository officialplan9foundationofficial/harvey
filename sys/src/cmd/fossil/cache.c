#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#include "9.h"	/* for cacheFlush */

typedef struct FreeList FreeList;
typedef struct BAddr BAddr;

enum {
	BadHeap = ~0,
};

/*
 * Store data to the memory cache in c->size blocks
 * with the block zero extended to fill it out.  When writing to
 * Venti, the block will be zero truncated.  The walker will also check
 * that the block fits within psize or dsize as the case may be.
 */

struct Cache
{
	VtLock	*lk;
	int 	ref;
	int	mode;

	Disk 	*disk;
	int	size;			/* block size */
	int	ndmap;		/* size of per-block dirty pointer map used in blockWrite */
	VtSession *z;
	u32int	now;			/* ticks for usage timestamps */
	Block	**heads;		/* hash table for finding address */
	int	nheap;			/* number of available victims */
	Block	**heap;			/* heap for locating victims */
	long	nblocks;		/* number of blocks allocated */
	Block	*blocks;		/* array of block descriptors */
	u8int	*mem;			/* memory for all block data & blists */

	BList	*blfree;
	VtRendez *blrend;

	int 	ndirty;			/* number of dirty blocks in the cache */
	int 	maxdirty;		/* max number of dirty blocks */
	u32int	vers;

	long hashSize;

	FreeList *fl;

	VtRendez *die;			/* daemon threads should die when != nil */

	VtRendez *flush;
	VtRendez *flushwait;
	BAddr *baddr;
	int bw, br, be;
	int nflush;

	Periodic *sync;

	/* unlink daemon */
	BList *uhead;
	BList *utail;
	VtRendez *unlink;
};

struct BList {
	int part;
	u32int addr;
	uchar type;
	u32int tag;
	u32int epoch;
	u32int vers;
	
	/* for roll back */
	int index;			/* -1 indicates not valid */
	union {
		uchar score[VtScoreSize];
		uchar entry[VtEntrySize];
	} old;
	BList *next;
};

struct BAddr {
	int part;
	u32int addr;
	u32int vers;
};

struct FreeList {
	VtLock *lk;
	u32int last;	/* last block allocated */
	u32int end;	/* end of data partition */
};

static FreeList *flAlloc(u32int end);
static void flFree(FreeList *fl);

static Block *cacheBumpBlock(Cache *c);
static void heapDel(Block*);
static void heapIns(Block*);
static void cacheCheck(Cache*);
static int readLabel(Cache*, Label*, u32int addr);
static void unlinkThread(void *a);
static void flushThread(void *a);
static void flushBody(Cache *c);
static void unlinkBody(Cache *c);
static int cacheFlushBlock(Cache *c);
static void cacheSync(void*);
/*
 * Mapping from local block type to Venti type
 */
int vtType[BtMax] = {
	VtDataType,		/* BtData | 0  */
	VtPointerType0,		/* BtData | 1  */
	VtPointerType1,		/* BtData | 2  */
	VtPointerType2,		/* BtData | 3  */
	VtPointerType3,		/* BtData | 4  */
	VtPointerType4,		/* BtData | 5  */
	VtPointerType5,		/* BtData | 6  */
	VtPointerType6,		/* BtData | 7  */
	VtDirType,		/* BtDir | 0  */
	VtPointerType0,		/* BtDir | 1  */
	VtPointerType1,		/* BtDir | 2  */
	VtPointerType2,		/* BtDir | 3  */
	VtPointerType3,		/* BtDir | 4  */
	VtPointerType4,		/* BtDir | 5  */
	VtPointerType5,		/* BtDir | 6  */
	VtPointerType6,		/* BtDir | 7  */
};

/*
 * Allocate the memory cache. 
 */
Cache *
cacheAlloc(Disk *disk, VtSession *z, ulong nblocks, int mode)
{
	int i;
	Cache *c;
	Block *b;
	BList *bl;
	u8int *p;
	int nbl;

	c = vtMemAllocZ(sizeof(Cache));

	/* reasonable number of BList elements */
	nbl = nblocks * 4;

	c->lk = vtLockAlloc();
	c->ref = 1;
	c->disk = disk;
	c->z = z;
	c->size = diskBlockSize(disk);
bwatchSetBlockSize(c->size);
	/* round c->size up to be a nice multiple */
	c->size = (c->size + 127) & ~127;
	c->ndmap = (c->size/20 + 7) / 8;
	c->nblocks = nblocks;
	c->hashSize = nblocks;
	c->heads = vtMemAllocZ(c->hashSize*sizeof(Block*));
	c->heap = vtMemAllocZ(nblocks*sizeof(Block*));
	c->blocks = vtMemAllocZ(nblocks*sizeof(Block));
	c->mem = vtMemAllocZ(nblocks * (c->size + c->ndmap) + nbl * sizeof(BList));
	c->baddr = vtMemAllocZ(nblocks * sizeof(BAddr));
	c->mode = mode;
	c->vers++;
	p = c->mem;
	for(i = 0; i < nblocks; i++){
		b = &c->blocks[i];
		b->lk = vtLockAlloc();
		b->c = c;
		b->data = p;
		b->heap = i;
		b->ioready = vtRendezAlloc(b->lk);
		c->heap[i] = b;
		p += c->size;
	}
	c->nheap = nblocks;
	for(i = 0; i < nbl; i++){
		bl = (BList*)p;
		bl->next = c->blfree;
		c->blfree = bl;
		p += sizeof(BList);
	}
	/* separate loop to keep blocks and blists reasonably aligned */
	for(i = 0; i < nblocks; i++){
		b = &c->blocks[i];
		b->dmap = p;
		p += c->ndmap;
	}

	c->blrend = vtRendezAlloc(c->lk);

	c->maxdirty = nblocks*(DirtyPercentage*0.01);

	c->fl = flAlloc(diskSize(disk, PartData));
	
	c->unlink = vtRendezAlloc(c->lk);
	c->flush = vtRendezAlloc(c->lk);
	c->flushwait = vtRendezAlloc(c->lk);
	c->sync = periodicAlloc(cacheSync, c, 30*1000);

	if(mode == OReadWrite){
		c->ref += 2;
		vtThread(unlinkThread, c);
		vtThread(flushThread, c);
	}
	cacheCheck(c);

	return c;
}

/*
 * Free the whole memory cache, flushing all dirty blocks to the disk.
 */
void
cacheFree(Cache *c)
{
	int i;

	/* kill off daemon threads */
	vtLock(c->lk);
	c->die = vtRendezAlloc(c->lk);
	periodicKill(c->sync);
	vtWakeup(c->flush);
	vtWakeup(c->unlink);
	while(c->ref > 1)
		vtSleep(c->die);

	/* flush everything out */
	do {
		unlinkBody(c);
		vtUnlock(c->lk);
		while(cacheFlushBlock(c))
			;
		diskFlush(c->disk);
		vtLock(c->lk);
	} while(c->uhead || c->ndirty);
	vtUnlock(c->lk);

	cacheCheck(c);

	for(i = 0; i < c->nblocks; i++){
		assert(c->blocks[i].ref == 0);
		vtRendezFree(c->blocks[i].ioready);
		vtLockFree(c->blocks[i].lk);
	}
	flFree(c->fl);
	vtMemFree(c->baddr);
	vtMemFree(c->heads);
	vtMemFree(c->blocks);
	vtMemFree(c->mem);
	vtLockFree(c->lk);
	diskFree(c->disk);
	vtRendezFree(c->blrend);
	/* don't close vtSession */
	vtMemFree(c);
}

static void
cacheDump(Cache *c)
{	
	int i;
	Block *b;

	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		fprint(2, "p=%d a=%ud %V t=%d ref=%d state=%s io=%s\n",
			b->part, b->addr, b->score, b->l.type, b->ref,
			bsStr(b->l.state), bioStr(b->iostate));
	}
}

static void
cacheCheck(Cache *c)
{
	u32int size, now;
	int i, k, refed;
	static uchar zero[VtScoreSize];
	Block *b;

	size = c->size;
	now = c->now;

	for(i = 0; i < c->nheap; i++){
		if(c->heap[i]->heap != i)
			vtFatal("mis-heaped at %d: %d", i, c->heap[i]->heap);
		if(i > 0 && c->heap[(i - 1) >> 1]->used - now > c->heap[i]->used - now)
			vtFatal("bad heap ordering");
		k = (i << 1) + 1;
		if(k < c->nheap && c->heap[i]->used - now > c->heap[k]->used - now)
			vtFatal("bad heap ordering");
		k++;
		if(k < c->nheap && c->heap[i]->used - now > c->heap[k]->used - now)
			vtFatal("bad heap ordering");
	}

	refed = 0;
	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		if(b->data != &c->mem[i * size])
			vtFatal("mis-blocked at %d", i);
		if(b->ref && b->heap == BadHeap){
			refed++;
		}
	}
if(c->nheap + refed != c->nblocks){
fprint(2, "cacheCheck: nheap %d refed %d nblocks %ld\n", c->nheap, refed, c->nblocks);
cacheDump(c);
}
	assert(c->nheap + refed == c->nblocks);
	refed = 0;
	for(i = 0; i < c->nblocks; i++){
		b = &c->blocks[i];
		if(b->ref){
if(1)fprint(2, "p=%d a=%ud %V ref=%d %L\n", b->part, b->addr, b->score, b->ref, &b->l);
			refed++;
		}
	}
if(refed > 0)fprint(2, "cacheCheck: in used %d\n", refed);
}


/*
 * locate the block with the oldest second to last use.
 * remove it from the heap, and fix up the heap.
 */
/* called with c->lk held */
static Block *
cacheBumpBlock(Cache *c)
{
	Block *b;

	/*
	 * locate the block with the oldest second to last use.
	 * remove it from the heap, and fix up the heap.
	 */
	if(c->nheap == 0)
		vtFatal("cacheBumpBlock: no free blocks in cache");
	b = c->heap[0];
	heapDel(b);

	assert(b->heap == BadHeap);
	assert(b->ref == 0);
	assert(b->iostate == BioEmpty || b->iostate == BioLabel || b->iostate == BioClean);
	assert(b->prior == nil);
	assert(b->uhead == nil);

	/*
	 * unchain the block from hash chain
	 */
	if(b->prev){
		*(b->prev) = b->next;
		if(b->next)
			b->next->prev = b->prev;
		b->prev = nil;
	}

 	
if(0)fprint(2, "droping %d:%x:%V\n", b->part, b->addr, b->score);
	/* set block to a reasonable state */
	b->ref = 1;
	b->part = PartError;
	memset(&b->l, 0, sizeof(b->l));
	b->iostate = BioEmpty;

	return b;
}

/*
 * look for a particular version of the block in the memory cache.
 */
static Block *
_cacheLocalLookup(Cache *c, int part, u32int addr, u32int vers,
	int waitlock, int *lockfailure)
{
	Block *b;
	ulong h;

	h = addr % c->hashSize;

	if(lockfailure)
		*lockfailure = 0;

	/*
	 * look for the block in the cache
	 */
	vtLock(c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part == part && b->addr == addr)
			break;
	}
	if(b == nil || b->vers != vers){
		vtUnlock(c->lk);
		return nil;
	}
	if(!waitlock && !vtCanLock(b->lk)){
		*lockfailure = 1;
		vtUnlock(c->lk);
		return nil;
	}
	heapDel(b);
	b->ref++;
	vtUnlock(c->lk);

	bwatchLock(b);
	if(waitlock)
		vtLock(b->lk);
	b->nlock = 1;

	for(;;){
		switch(b->iostate){
		default:
			abort();
		case BioEmpty:
		case BioLabel:
		case BioClean:	
		case BioDirty:
			if(b->vers != vers){
				blockPut(b);
				return nil;
			}
			return b;
		case BioReading:
		case BioWriting:
			vtSleep(b->ioready);
			break;
		case BioVentiError:
		case BioReadError:
			blockSetIOState(b, BioEmpty);
			blockPut(b);
			vtSetError(EIO);
			return nil;
		}
	}
	/* NOT REACHED */
}
static Block*
cacheLocalLookup(Cache *c, int part, u32int addr, u32int vers)
{
	return _cacheLocalLookup(c, part, addr, vers, 1, 0);
}


/*
 * fetch a local (on-disk) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 */
Block *
_cacheLocal(Cache *c, int part, u32int addr, int mode, u32int epoch)
{
	Block *b;
	ulong h;

	assert(part != PartVenti);

	h = addr % c->hashSize;

	/*
	 * look for the block in the cache
	 */
	vtLock(c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part != part || b->addr != addr)
			continue;
		if(epoch && b->l.epoch != epoch){
fprint(2, "_cacheLocal want epoch %ud got %ud\n", epoch, b->l.epoch);
			vtUnlock(c->lk);
			vtSetError(ELabelMismatch);
			return nil;
		}
		heapDel(b);
		b->ref++;
		break;
	}
			
	if(b == nil){
		b = cacheBumpBlock(c);

		b->part = part;
		b->addr = addr;
		localToGlobal(addr, b->score);

		/* chain onto correct hash */
		b->next = c->heads[h];
		c->heads[h] = b;
		if(b->next != nil)
			b->next->prev = &b->next;
		b->prev = &c->heads[h];
	}

	vtUnlock(c->lk);

	/*
	 * BUG: what if the epoch changes right here?
	 * In the worst case, we could end up in some weird
	 * lock loop, because the block we want no longer exists,
	 * and instead we're trying to lock a block we have no 
	 * business grabbing.
	 *
	 * For now, I'm not going to worry about it.
	 */

if(0)fprint(2, "cacheLocal: %d: %d %x\n", getpid(), b->part, b->addr);
	bwatchLock(b);
	vtLock(b->lk);
	b->nlock = 1;

	if(part == PartData && b->iostate == BioEmpty){
		if(!readLabel(c, &b->l, addr)){
			blockPut(b);
			return nil;
		}
		blockSetIOState(b, BioLabel);
	}
	if(epoch && b->l.epoch != epoch){
		blockPut(b);
fprint(2, "_cacheLocal want epoch %ud got %ud\n", epoch, b->l.epoch);
		vtSetError(ELabelMismatch);
		return nil;
	}

	b->pc = getcallerpc(&c);
	for(;;){
		switch(b->iostate){
		default:
			abort();
		case BioEmpty:
		case BioLabel:
			if(mode == OOverWrite){
				blockSetIOState(b, BioClean);
				return b;
			}
			diskRead(c->disk, b);
			vtSleep(b->ioready);
			break;
		case BioClean:
		case BioDirty:
			return b;
		case BioReading:
		case BioWriting:
			vtSleep(b->ioready);
			break;
		case BioReadError:
			blockSetIOState(b, BioEmpty);
			blockPut(b);
			vtSetError(EIO);
			return nil;
		}
	}
	/* NOT REACHED */
}

Block *
cacheLocal(Cache *c, int part, u32int addr, int mode)
{
	return _cacheLocal(c, part, addr, mode, 0);
}

/*
 * fetch a local (on-disk) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 * check tag and type.
 */
Block *
cacheLocalData(Cache *c, u32int addr, int type, u32int tag, int mode, u32int epoch)
{
	Block *b;

	b = _cacheLocal(c, PartData, addr, mode, epoch);
	if(b == nil)
		return nil;
	if(b->l.type != type || b->l.tag != tag){
		fprint(2, "cacheLocalData: addr=%d type got %d exp %d: tag got %x exp %x\n",
			addr, b->l.type, type, b->l.tag, tag);
abort();
		vtSetError(ELabelMismatch);
		blockPut(b);
		return nil;
	}
	b->pc = getcallerpc(&c);
	return b;
}

/*
 * fetch a global (Venti) block from the memory cache.
 * if it's not there, load it, bumping some other block.
 * check tag and type if it's really a local block in disguise.
 */
Block *
cacheGlobal(Cache *c, uchar score[VtScoreSize], int type, u32int tag, int mode)
{
	int n;
	Block *b;
	ulong h;
	u32int addr;

	addr = globalToLocal(score);
	if(addr != NilBlock){
		b = cacheLocalData(c, addr, type, tag, mode, 0);
		if(b)
			b->pc = getcallerpc(&c);
		return b;
	}

	h = (u32int)(score[0]|(score[1]<<8)|(score[2]<<16)|(score[3]<<24)) % c->hashSize;

	/*
	 * look for the block in the cache
	 */
	vtLock(c->lk);
	for(b = c->heads[h]; b != nil; b = b->next){
		if(b->part != PartVenti || memcmp(b->score, score, VtScoreSize) != 0 || b->l.type != type)
			continue;
		heapDel(b);
		b->ref++;
		break;
	}

	if(b == nil){
if(0)fprint(2, "cacheGlobal %V %d\n", score, type);

		b = cacheBumpBlock(c);

		b->part = PartVenti;
		b->addr = NilBlock;
		b->l.type = type;
		memmove(b->score, score, VtScoreSize);

		/* chain onto correct hash */
		b->next = c->heads[h];
		c->heads[h] = b;
		if(b->next != nil)
			b->next->prev = &b->next;
		b->prev = &c->heads[h];
	}
	vtUnlock(c->lk);

	bwatchLock(b);
	vtLock(b->lk);
	b->nlock = 1;
	b->pc = getcallerpc(&c);

	switch(b->iostate){
	default:
		abort();
	case BioEmpty:
		n = vtRead(c->z, score, vtType[type], b->data, c->size);
		if(n < 0 || !vtSha1Check(score, b->data, n)){
			blockSetIOState(b, BioVentiError);
			blockPut(b);
			return nil;
		}
		vtZeroExtend(vtType[type], b->data, n, c->size);
		blockSetIOState(b, BioClean);
		return b;	
	case BioClean:
		return b;
	case BioVentiError:
	case BioReadError:
		blockPut(b);
		vtSetError(EIO);
		blockSetIOState(b, BioEmpty);
		return nil;
	}
	/* NOT REACHED */
}

/*
 * allocate a new on-disk block and load it into the memory cache.
 * BUG: if the disk is full, should we flush some of it to Venti?
 */
static u32int lastAlloc;

Block *
cacheAllocBlock(Cache *c, int type, u32int tag, u32int epoch, u32int epochLow)
{
	FreeList *fl;
	u32int addr;
	Block *b;
	int n, nwrap;
	Label lab;

	n = c->size / LabelSize;
	fl = c->fl;

	vtLock(fl->lk);
	addr = fl->last;
	b = cacheLocal(c, PartLabel, addr/n, OReadOnly);
	if(b == nil){
		fprint(2, "cacheAllocBlock: xxx %R\n");
		vtUnlock(fl->lk);
		return nil;
	}
	nwrap = 0;
	for(;;){
		if(++addr >= fl->end){
			addr = 0;
			fprint(2, "cacheAllocBlock wrap %d\n", fl->end);
			if(++nwrap >= 2){
				blockPut(b);
				fl->last = 0;
				vtSetError("disk is full");
				fprint(2, "cacheAllocBlock: xxx1 %R\n");
				vtUnlock(fl->lk);
				return nil;
			}
		}
		if(addr%n == 0){
			blockPut(b);
			b = cacheLocal(c, PartLabel, addr/n, OReadOnly);
			if(b == nil){
				fl->last = addr;
				fprint(2, "cacheAllocBlock: xxx2 %R\n");
				vtUnlock(fl->lk);
				return nil;
			}
		}
		if(!labelUnpack(&lab, b->data, addr%n))
			continue;
		if(lab.state == BsFree)
			goto Found;
		if((lab.state&BsClosed) && lab.epochClose <= epochLow)
			goto Found;
	}
Found:
	blockPut(b);
	b = cacheLocal(c, PartData, addr, OOverWrite);
	if(b == nil){
		fprint(2, "cacheAllocBlock: xxx3 %R\n");
		return nil;
	}
assert(b->iostate == BioLabel || b->iostate == BioClean);
	fl->last = addr;
	lab.type = type;
	lab.tag = tag;
	lab.state = BsAlloc;
	lab.epoch = epoch;
	lab.epochClose = ~(u32int)0;
	if(!blockSetLabel(b, &lab)){
		fprint(2, "cacheAllocBlock: xxx4 %R\n");
		blockPut(b);
		return nil;
	}
	vtZeroExtend(vtType[type], b->data, 0, c->size);
if(0)diskWrite(c->disk, b);

if(0)fprint(2, "fsAlloc %ud type=%d tag = %ux\n", addr, type, tag);
	lastAlloc = addr;
	vtUnlock(fl->lk);
	b->pc = getcallerpc(&c);
	return b;
}

static FreeList *
flAlloc(u32int end)
{
	FreeList *fl;

	fl = vtMemAllocZ(sizeof(*fl));
	fl->lk = vtLockAlloc();
	fl->last = 0;
	fl->end = end;
	return fl;
}

static void
flFree(FreeList *fl)
{
	vtLockFree(fl->lk);
	vtMemFree(fl);
}

u32int
cacheLocalSize(Cache *c, int part)
{
	return diskSize(c->disk, part);
}

/*
 * Copy on write.  Copied blocks have to be marked BaCopy.
 * See the big comment near blockRemoveLink.
 */
Block*
blockCopy(Block *b, u32int tag, u32int ehi, u32int elo)
{
	Block *bb, *lb;
	Label l;

	assert((b->l.state&BsClosed)==0 && b->l.epoch < ehi);
	bb = cacheAllocBlock(b->c, b->l.type, tag, ehi, elo);
	if(bb == nil){
		blockPut(b);
		return nil;
	}

	/*
	 * Change label on b to mark that we've copied it.
	 * This has to come after cacheAllocBlock, since we
	 * can't hold any labels blocks (lb) while we try to
	 * fetch others (in cacheAllocBlock).
	 */
	if(!(b->l.state&BsCopied) && b->part==PartData){
		l = b->l;
		l.state |= BsCopied;
		lb = _blockSetLabel(b, &l);
		if(lb == nil){
			/* can't set label => can't copy block */
			blockPut(b);
			l.type = BtMax;
			l.state = BsFree;
			l.epoch = 0;
			l.epochClose = 0;
			l.tag = 0;
			/* ignore error: block gets lost on error */
			blockSetLabel(bb, &l);
			blockPut(bb);
			return nil;
		}
		blockDependency(bb, lb, -1, nil, nil);
		blockPut(lb);
	}

	if(0){
		if(b->addr != NilBlock)
			fprint(2, "blockCopy %#ux/%ud => %#ux/%ud\n",
				b->addr, b->l.epoch, bb->addr, bb->l.epoch);
		else if(memcmp(b->score, vtZeroScore, VtScoreSize) != 0)
			fprint(2, "blockCopy %V => %#ux/%ud\n",
				b->score, bb->addr, bb->l.epoch);
	}

	memmove(bb->data, b->data, b->c->size);
	blockDirty(bb);
	blockPut(b);
	return bb;
}

/*
 * The thread that has locked b may refer to it by
 * multiple names.  Nlock counts the number of
 * references the locking thread holds.  It will call
 * blockPut once per reference.
 */
void
blockDupLock(Block *b)
{
	assert(b->nlock > 0);
	b->nlock++;
}

/*
 * we're done with the block.
 * unlock it.  can't use it after calling this.
 */
void
blockPut(Block* b)
{
	Cache *c;

	if(b == nil)
		return;

if(0)fprint(2, "blockPut: %d: %d %x %d %s\n", getpid(), b->part, b->addr, c->nheap, bioStr(b->iostate));

	if(b->iostate == BioDirty)
		bwatchDependency(b);

	if(--b->nlock > 0)
		return;

	/*
	 * b->nlock should probably stay at zero while
	 * the block is unlocked, but diskThread and vtSleep
	 * conspire to assume that they can just vtLock(b->lk); blockPut(b),
	 * so we have to keep b->nlock set to 1 even 
	 * when the block is unlocked.
	 */
	assert(b->nlock == 0);
	b->nlock = 1;
//	b->pc = 0;

	bwatchUnlock(b);
	vtUnlock(b->lk);
	c = b->c;
	vtLock(c->lk);

	if(--b->ref > 0){
		vtUnlock(c->lk);
		return;
	}

	assert(b->ref == 0);
	switch(b->iostate){
	default:
		b->used = c->now++;
		heapIns(b);
		break;
	case BioEmpty:
	case BioLabel:
		if(c->nheap == 0)
			b->used = c->now++;
		else
			b->used = c->heap[0]->used;
		heapIns(b);
		break;
	case BioDirty:
		break;
	}
	vtUnlock(c->lk);
}

/*
 * we're deleting a block; delete all the blocks it points to
 * that are still active (i.e., not needed by snapshots).
 */
static void
blockCleanup(Block *b, u32int epoch)
{
	Cache *c;
	Block *bb;
	int i, n;
	Label l;
	u32int a;
	int type;
	int mode;

	type = b->l.type;
	c = b->c;

	bwatchReset(b->score);

	blockSetIOState(b, BioClean);
	
	/* do not recursively free directories */
	if(type == BtData || type == BtDir)
		return;

	n = c->size / VtScoreSize;
	mode = OReadWrite;
	if(type-1 == BtData || type-1 == BtDir)
		mode = OOverWrite;
	for(i=0; i<n; i++){
		a = globalToLocal(b->data + i*VtScoreSize);
		if(a == NilBlock || !readLabel(c, &l, a))
			continue;
		if((l.state&BsClosed) || l.epoch != epoch)
			continue;
		bb = cacheLocalData(c, a, type-1, b->l.tag, mode, 0);
		if(bb == nil)
			continue;
		if((bb->l.state&BsClosed) || bb->l.epoch != epoch){
			fprint(2, "cleanupBlock: block %ud changed underfoot! expected %L got %L\n",
				a, &l, &bb->l);
			blockPut(bb);
			continue;
		}
		blockCleanup(bb, epoch);
		l.type = BtMax;
		l.epoch = epoch;
		l.epochClose = 0;
		l.state = BsFree;
		l.tag = 0;
		blockSetLabel(bb, &l);
		blockPut(bb);
	}
}

/*
 * We don't need the block at addr anymore for the active file system.
 * If we don't need it for the snapshots, remove it completely.
 * Epoch is the epoch during which we got rid of the block.
 * See blockRemoveLink for more.
 */
static int
unlinkBlock(Cache *c, u32int addr, int type, u32int tag, u32int epoch)
{
	Block *b;
	Label l;

	if(addr == NilBlock)
		return 1;

//fprint(2, "unlinkBlock %#ux\n", addr);
	b = cacheLocalData(c, addr, type, tag, OReadOnly, 0);
	if(b == nil)
		return 0;
	if(b->l.epoch > epoch){
fprint(2, "unlinkBlock: strange epoch :%ud %ud\n", b->l.epoch, epoch);
		blockPut(b);
		return 0;
	}

	l = b->l;
	if((b->l.state&BsClosed)==0 && b->l.epoch==epoch){
		l.state = BsFree;
		l.type = BtMax;
		l.tag = 0;
		l.epoch = 0;
		l.epochClose = 0;
		blockCleanup(b, epoch);
	}else{
		l.state |= BsClosed;
		l.epochClose = epoch;
	}
	blockSetLabel(b, &l);
	blockPut(b);
	return 1;
}

/*
 * try to allocate a BList so we can record that b must
 * be written out before some other block.
 * if can't find a BList, write b out instead and return nil.
 */
static BList *
blistAlloc(Block *b)
{
	Cache *c;
	BList *p;

	/*
	 * It's possible that when we marked b dirty, there were
	 * too many dirty blocks so we just wrote b there and then.
	 * So b might not be dirty.  If it's not, no need to worry
	 * about recording the write constraint.
	 *
	 * BlockRemoveLink depends on the fact that if blistAlloc
	 * returns non-nil, b really is dirty.
	 */
	if(b->iostate != BioDirty){
		assert(b->iostate == BioClean);
		return nil;
	}
		
	/*
	 * Easy: maybe there's a free list left.
	 */
	c = b->c;
	vtLock(c->lk);
	if(c->blfree){
	HaveBlFree:
		p = c->blfree;
		c->blfree = p->next;
		vtUnlock(c->lk);
		return p;
	}
	vtUnlock(c->lk);

	/*
	 * No free BLists.  What are our options?
	 */

	/* Block has no priors? Just write it. */
	if(b->prior == nil){
		diskWrite(c->disk, b);
		while(b->iostate != BioClean)
			vtSleep(b->ioready);
		return nil;
	}

	/*
	 * Wake the flush thread, which will hopefully free up
	 * some BLists for us.  We used to flush a block from
	 * our own prior list and reclaim that BList, but this is
	 * a no-no: some of the blocks on our prior list may
	 * be locked by our caller.  Or maybe their label blocks
	 * are locked by our caller.  In any event, it's too hard
	 * to make sure we can do I/O for ourselves.  Instead,
	 * we assume the flush thread will find something.
	 * (The flush thread never blocks waiting for a block,
	 * so it won't deadlock like we will.)
	 */
	vtLock(c->lk);
	while(c->blfree == nil){
		vtWakeup(c->flush);
		vtSleep(c->blrend);
	}
	goto HaveBlFree;
}

void
blistFree(Cache *c, BList *bl)
{
	vtLock(c->lk);
	bl->next = c->blfree;
	c->blfree = bl;
	vtWakeup(c->blrend);
	vtUnlock(c->lk);
}

/*
 * Flush b or one of the blocks it depends on.
 */
void
blockFlush(Block *b)
{
	int first, nlock;
	BList *p, **pp;
	Block *bb;
	Cache *c;

//fprint(2, "blockFlush %p\n", b);

	c = b->c;
	
	first = 1;
	pp = &b->prior;
	for(p=*pp; p; p=*pp){
		bb = cacheLocalLookup(c, p->part, p->addr, p->vers);
		if(bb == nil){
			*pp = p->next;
			blistFree(c, p);
			continue;
		}
		if(!first)
			blockPut(b);
		first = 0;
		b = bb;
		pp = &b->prior;
	}

	/*
	 * If b->nlock > 1, the block is aliased within
	 * a single thread.  That thread is us, and it's
	 * the block that was passed in (rather than a prior).
	 * DiskWrite does some funny stuff with VtLock
	 * and blockPut that basically assumes b->nlock==1.
	 * We humor diskWrite by temporarily setting
	 * nlock to 1.  This needs to be revisited.  (TODO)
	 */
	nlock = b->nlock;
	if(nlock > 1){
		assert(first);
		b->nlock = 1;
	}
	diskWrite(c->disk, b);
	while(b->iostate != BioClean)
		vtSleep(b->ioready);
	b->nlock = nlock;
	if(!first)
		blockPut(b);
}

/*
 * Record that bb must be written out before b.
 * If index is given, we're about to overwrite the score/e
 * at that index in the block.  Save the old value so we
 * can write a safer ``old'' version of the block if pressed.
 */
void
blockDependency(Block *b, Block *bb, int index, uchar *score, Entry *e)
{
	BList *p;

	if(bb->iostate == BioClean)
		return;

	/*
	 * Dependencies for blocks containing Entry structures
	 * or scores must always be explained.  The problem with
	 * only explaining some of them is this.  Suppose we have two 
	 * dependencies for the same field, the first explained
	 * and the second not.  We try to write the block when the first
	 * dependency is not written but the second is.  We will roll back
	 * the first change even though the second trumps it.
	 */
	if(index == -1 && bb->part == PartData)
		assert(b->l.type == BtData);

	assert(bb->iostate == BioDirty);

	p = blistAlloc(bb);
	if(p == nil)
		return;	

if(0)fprint(2, "%d:%x:%d depends on %d:%x:%d\n", b->part, b->addr, b->l.type, bb->part, bb->addr, bb->l.type);

	p->part = bb->part;
	p->addr = bb->addr;
	p->type = bb->l.type;
	p->vers = bb->vers;
	p->index = index;
	if(p->index >= 0){
		/*
		 * This test would just be b->l.type==BtDir except
		 * we need to exclude the super block.
		 */
		if(b->l.type == BtDir && b->part == PartData)
			entryPack(e, p->old.entry, 0);
		else
			memmove(p->old.score, score, VtScoreSize);
	}
	p->next = b->prior;
	b->prior = p;
}

/*
 * Mark an in-memory block as dirty.  If there are too many
 * dirty blocks, start writing some out to disk.  If there are way
 * too many dirty blocks, write this one out too.
 *
 * Note that since we might call blockFlush, which walks
 * the prior list, we can't call blockDirty while holding a lock
 * on any of our priors.  This can be tested by recompiling
 * with flush always set to 1 below.
 */
int
blockDirty(Block *b)
{
	Cache *c;
	int flush;

	c = b->c;

	assert(b->part != PartVenti);

	if(b->iostate == BioDirty)
		return 1;
	assert(b->iostate == BioClean);

	vtLock(c->lk);
	b->iostate = BioDirty;
	c->ndirty++;
	if(c->ndirty > (c->maxdirty>>1))
		vtWakeup(c->flush);
	flush = c->ndirty > c->maxdirty;
	vtUnlock(c->lk);

	if(flush)
		blockFlush(b);

	return 1;
}

/*
 * Block b once pointed at the block bb at addr/type/tag, but no longer does.
 * 
 * The file system maintains the following invariants (i-iv checked by flchk):
 *
 * (i) b.e in [bb.e, bb.eClose)
 * (ii) if b.e==bb.e,  then no other b' in e points at bb.
 * (iii) if !(b.state&Copied) and b.e==bb.e then no other b' points at bb.
 * (iv) if b is active then no other active b' points at bb.
 * (v) if b is a past life of b' then only one of b and b' is active (too hard to check)
 *
 * The file system initially satisfies these invariants, and we can verify that
 * the various file system operations maintain them.  See fossil.invariants.
 *
 * Condition (i) lets us reclaim blocks once the low epoch is greater
 * than epochClose.
 * 
 * If the condition in (iii) is satisfied, then this is the only pointer to bb,
 * so bb can be reclaimed once b has been written to disk.  blockRemoveLink
 * checks !(b.state&Copied) as an optimization.  UnlinkBlock and blockCleanup
 * will check the conditions again for each block they consider.
 */
int
blockRemoveLink(Block *b, u32int addr, int type, u32int tag)
{
	BList *bl;
	BList *p, **pp;
	Cache *c;

	c = b->c;

	/* remove unlinked block from prior list */
	pp = &b->prior;
	for(p=*pp; p; p=*pp){
		if(p->part != PartData || p->addr != addr){
			pp = &p->next;
			continue;
		}
		*pp = p->next;
		blistFree(c, p);
	}

	/* if b has been copied, can't reclaim blocks it points at. */
	if(b->l.state & BsCopied)
		return 0;

	bl = blistAlloc(b);
	if(bl == nil)
		return unlinkBlock(b->c, addr, type, tag, b->l.epoch);

	/*
	 * Because bl != nil, we know b is dirty.
	 * (Linking b->uhead onto a clean block is
	 * counterproductive, since we only look at
	 * b->uhead when a block transitions from 
	 * dirty to clean.)
	 */
	assert(b->iostate == BioDirty);
	
	bl->part = PartData;
	bl->addr = addr;
	bl->type = type;
	bl->tag = tag;
	bl->epoch = b->l.epoch;
	if(b->uhead == nil)
		b->uhead = bl;
	else 
		b->utail->next = bl;
	b->utail = bl;
	bl->next = nil;
	return 1;
}

/*
 * set the label associated with a block.
 */
Block*
_blockSetLabel(Block *b, Label *l)
{
	int lpb;
	Block *bb;
	u32int a;
	Cache *c;

	c = b->c;

	assert(b->part == PartData);
	assert(b->iostate == BioLabel || b->iostate == BioClean || b->iostate == BioDirty);
	lpb = c->size / LabelSize;
	a = b->addr / lpb;
	bb = cacheLocal(c, PartLabel, a, OReadWrite);
	if(bb == nil){
		blockPut(b);
		return nil;
	}
	b->l = *l;
	labelPack(l, bb->data, b->addr%lpb);
	blockDirty(bb);
	return bb;
}

int
blockSetLabel(Block *b, Label *l)
{
	Block *lb;
	Label oldl;

	oldl = b->l;
	lb = _blockSetLabel(b, l);
	if(lb == nil)
		return 0;

	/*
	 * If we're allocating the block, make sure the label (bl)
	 * goes to disk before the data block (b) itself.  This is to help
	 * the blocks that in turn depend on b. 
	 *
	 * Suppose bx depends on (must be written out after) b.  
	 * Once we write b we'll think it's safe to write bx.
	 * Bx can't get at b unless it has a valid label, though.
	 *
	 * Allocation is the only case in which having a current label
	 * is vital because:
	 *
	 *	- l.type is set at allocation and never changes.
	 *	- l.tag is set at allocation and never changes.
	 *	- l.state is not checked when we load blocks.
	 *	- the archiver cares deeply about l.state being
	 *		BaActive vs. BaCopied, but that's handled
	 *		by direct calls to _blockSetLabel.
	 */

	if(oldl.state == BsFree)
		blockDependency(b, lb, -1, nil, nil);
	blockPut(lb);
	return 1;
}

/*
 * We've decided to write out b.  Maybe b has some pointers to blocks
 * that haven't yet been written to disk.  If so, construct a slightly out-of-date
 * copy of b that is safe to write out.  (diskThread will make sure the block
 * remains marked as dirty.)
 */
uchar *
blockRollback(Block *b, uchar *buf)
{
	u32int addr;
	BList *p;
	Super super;

	/* easy case */
	if(b->prior == nil)
		return b->data;
	
	memmove(buf, b->data, b->c->size);
	for(p=b->prior; p; p=p->next){
		/*
		 * we know p->index >= 0 because blockWrite has vetted this block for us.
		 */
		assert(p->index >= 0);
		assert(b->part == PartSuper || (b->part == PartData && b->l.type != BtData));
		if(b->part == PartSuper){
			assert(p->index == 0);
			superUnpack(&super, buf);
			addr = globalToLocal(p->old.score);
			if(addr == NilBlock){
				fprint(2, "rolling back super block: bad replacement addr %V\n", p->old.score);
				abort();
			}
			super.active = addr;
			superPack(&super, buf);
			continue;
		}
		if(b->l.type == BtDir)
			memmove(buf+p->index*VtEntrySize, p->old.entry, VtEntrySize);
		else
			memmove(buf+p->index*VtScoreSize, p->old.score, VtScoreSize);
	}
	return buf;
}

/*
 * Try to write block b. 
 * If b depends on other blocks:
 * 
 *	If the block has been written out, remove the dependency.
 *	If we know how to write out an old version of b that doesn't
 *		depend on it, do that.
 *
 *	Otherwise, bail.
 */
int
blockWrite(Block *b)
{
	uchar *dmap;
	Cache *c;
	BList *p, **pp;
	Block *bb;
	int lockfail;

	c = b->c;

	if(b->iostate != BioDirty)
		return 1;

	dmap = b->dmap;
	memset(dmap, 0, c->ndmap);
	pp = &b->prior;
	for(p=*pp; p; p=*pp){
		if(p->index >= 0){
			/* more recent dependency has succeeded; this one can go */
			if(dmap[p->index/8] & (1<<(p->index%8)))
				goto ignblock;
		}

		lockfail = 0;
		bb = _cacheLocalLookup(c, p->part, p->addr, p->vers, 0, &lockfail);
		if(bb == nil){
			if(lockfail)
				return 0;
			/* block not in cache => was written already */
			dmap[p->index/8] |= 1<<(p->index%8);
			goto ignblock;
		}

		/*
		 * same version of block is still in cache.
		 * 
		 * the assertion is true because the block still has version p->vers,
		 * which means it hasn't been written out since we last saw it.
		 */
		assert(bb->iostate == BioDirty);
		blockPut(bb);

		if(p->index < 0){
			/*
			 * We don't know how to temporarily undo
			 * b's dependency on bb, so just don't write b yet.
			 */
			if(0) fprint(2, "blockWrite skipping %d %x %d %d; need to write %d %x %d\n",
				b->part, b->addr, b->vers, b->l.type, p->part, p->addr, bb->vers);
			return 0;
		}
		/* keep walking down the list */
		pp = &p->next;
		continue;

ignblock:
		*pp = p->next;
		blistFree(c, p);
		continue;
	}

	diskWrite(c->disk, b);
	return 1;
}

/*
 * Change the I/O state of block b.
 * Just an assignment except for magic in
 * switch statement (read comments there).
 */
void
blockSetIOState(Block *b, int iostate)
{
	int dowakeup;
	Cache *c;
	BList *p, *q;

if(0) fprint(2, "iostate part=%d addr=%x %s->%s\n", b->part, b->addr, bioStr(b->iostate), bioStr(iostate));
	
	c = b->c;

	dowakeup = 0;
	switch(iostate){
	default:
		abort();
	case BioEmpty:
		assert(!b->uhead);
		break;
	case BioLabel:
		assert(!b->uhead);
		break;
	case BioClean:
		bwatchDependency(b);
		/*
		 * If b->prior is set, it means a write just finished.
		 * The prior list isn't needed anymore.
		 */
		for(p=b->prior; p; p=q){
			q = p->next;
			blistFree(c, p);
		}
		b->prior = nil;
		/*
		 * Freeing a block or just finished a write.
		 * Move the blocks from the per-block unlink
		 * queue to the cache unlink queue.
		 */
		if(b->iostate == BioDirty || b->iostate == BioWriting){
			vtLock(c->lk);
			c->ndirty--;
			b->iostate = iostate;	/* change here to keep in sync with ndirty */
			b->vers = c->vers++;
			if(b->uhead){
				/* add unlink blocks to unlink queue */
				if(c->uhead == nil){
					c->uhead = b->uhead;
					vtWakeup(c->unlink);
				}else
					c->utail->next = b->uhead;
				c->utail = b->utail;
				b->uhead = nil;
			}
			vtUnlock(c->lk);
		}
		assert(!b->uhead);
		dowakeup = 1;
		break;
	case BioDirty:
		/*
		 * Wrote out an old version of the block (see blockRollback).
		 * Bump a version count, leave it dirty.
		 */
		if(b->iostate == BioWriting){
			vtLock(c->lk);
			b->vers = c->vers++;
			vtUnlock(c->lk);
			dowakeup = 1;
		}
		break;
	case BioReading:
	case BioWriting:
		/*
		 * Adding block to disk queue.  Bump reference count.
		 * diskThread decs the count later by calling blockPut.
		 * This is here because we need to lock c->lk to
		 * manipulate the ref count.
		 */
		vtLock(c->lk);
		b->ref++;
		vtUnlock(c->lk);
		break;
	case BioReadError:
	case BioVentiError:
		/*
		 * Oops.
		 */
		dowakeup = 1;
		break;
	}
	b->iostate = iostate;
	/*
	 * Now that the state has changed, we can wake the waiters.
	 */
	if(dowakeup)
		vtWakeupAll(b->ioready);
}

char*
bsStr(int state)
{
	static char s[100];

	if(state == BsFree)
		return "Free";
	if(state == BsBad)
		return "Bad";

	sprint(s, "%x", state);
	if(!(state&BsAlloc))
		strcat(s, ",Free");	/* should not happen */
	if(state&BsCopied)
		strcat(s, ",Copied");
	if(state&BsVenti)
		strcat(s, ",Venti");
	if(state&BsClosed)
		strcat(s, ",Closed");
	return s;
}

char *
bioStr(int iostate)
{
	switch(iostate){
	default:
		return "Unknown!!";
	case BioEmpty:
		return "Empty";
	case BioLabel:
		return "Label";
	case BioClean:
		return "Clean";
	case BioDirty:
		return "Dirty";
	case BioReading:
		return "Reading";
	case BioWriting:
		return "Writing";
	case BioReadError:
		return "ReadError";
	case BioVentiError:
		return "VentiError";
	case BioMax:
		return "Max";
	}
}

static char *bttab[] = {
	"BtData",
	"BtData+1",
	"BtData+2",
	"BtData+3",
	"BtData+4",
	"BtData+5",
	"BtData+6",
	"BtData+7",
	"BtDir",
	"BtDir+1",
	"BtDir+2",
	"BtDir+3",
	"BtDir+4",
	"BtDir+5",
	"BtDir+6",
	"BtDir+7",
};

char*
btStr(int type)
{
	if(type < nelem(bttab))
		return bttab[type];
	return "unknown";
}

int
labelFmt(Fmt *f)
{
	Label *l;

	l = va_arg(f->args, Label*);
	return fmtprint(f, "%s,%s,e=%ud,%d,tag=%#ux",
		btStr(l->type), bsStr(l->state), l->epoch, (int)l->epochClose, l->tag);
}

int
scoreFmt(Fmt *f)
{
	uchar *v;
	int i;
	u32int addr;

	v = va_arg(f->args, uchar*);
	if(v == nil){
		fmtprint(f, "*");
	}else if((addr = globalToLocal(v)) != NilBlock)
		fmtprint(f, "0x%.8ux", addr);
	else{
		for(i = 0; i < VtScoreSize; i++)
			fmtprint(f, "%2.2ux", v[i]);
	}

	return 0;
}

static int
upHeap(int i, Block *b)
{
	Block *bb;
	u32int now;
	int p;
	Cache *c;
	
	c = b->c;
	now = c->now;
	for(; i != 0; i = p){
		p = (i - 1) >> 1;
		bb = c->heap[p];
		if(b->used - now >= bb->used - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;

	return i;
}

static int
downHeap(int i, Block *b)
{
	Block *bb;
	u32int now;
	int k;
	Cache *c;
	
	c = b->c;
	now = c->now;
	for(; ; i = k){
		k = (i << 1) + 1;
		if(k >= c->nheap)
			break;
		if(k + 1 < c->nheap && c->heap[k]->used - now > c->heap[k + 1]->used - now)
			k++;
		bb = c->heap[k];
		if(b->used - now <= bb->used - now)
			break;
		c->heap[i] = bb;
		bb->heap = i;
	}
	c->heap[i] = b;
	b->heap = i;
	return i;
}

/*
 * Delete a block from the heap.
 * Called with c->lk held.
 */
static void
heapDel(Block *b)
{
	int i, si;
	Cache *c;

	c = b->c;

	si = b->heap;
	if(si == BadHeap)
		return;
	b->heap = BadHeap;
	c->nheap--;
	if(si == c->nheap)
		return;
	b = c->heap[c->nheap];
	i = upHeap(si, b);
	if(i == si)
		downHeap(i, b);
}

/*
 * Insert a block into the heap.
 * Called with c->lk held.
 */
static void
heapIns(Block *b)
{
	assert(b->heap == BadHeap);
	upHeap(b->c->nheap++, b);
}

/*
 * Get just the label for a block.
 */
static int
readLabel(Cache *c, Label *l, u32int addr)
{
	int lpb;
	Block *b;
	u32int a;

	lpb = c->size / LabelSize;
	a = addr / lpb;
	b = cacheLocal(c, PartLabel, a, OReadOnly);
	if(b == nil){
		blockPut(b);
		return 0;
	}

	if(!labelUnpack(l, b->data, addr%lpb)){
		blockPut(b);
		return 0;
	}
	blockPut(b);
	return 1;
}

/*
 * Process unlink queue.
 * Called with c->lk held.
 */
static void
unlinkBody(Cache *c)
{
	BList *p;

	while(c->uhead != nil){
		p = c->uhead;
		c->uhead = p->next;
		vtUnlock(c->lk);

		if(!unlinkBlock(c, p->addr, p->type, p->tag, p->epoch))
			fprint(2, "unlinkBlock failed: addr=%x type=%d tag = %ux: %r\n",
				p->addr, p->type, p->tag);

		vtLock(c->lk);
		p->next = c->blfree;
		c->blfree = p;
	}
}

/*
 * Occasionally unlink the blocks on the cache unlink queue.
 */
static void
unlinkThread(void *a)
{
	Cache *c = a;

	vtThreadSetName("unlink");

	vtLock(c->lk);
	for(;;){
		while(c->uhead == nil && c->die == nil)
			vtSleep(c->unlink);
		if(c->die != nil)
			break;
		unlinkBody(c);
	}
	c->ref--;
	vtWakeup(c->die);
	vtUnlock(c->lk);
}

static int
baddrCmp(void *a0, void *a1)
{
	BAddr *b0, *b1;
	b0 = a0;
	b1 = a1;

	if(b0->part < b1->part)
		return -1;
	if(b0->part > b1->part)
		return 1;
	if(b0->addr < b1->addr)
		return -1;
	if(b0->addr > b1->addr)
		return 1;
	return 0;
}

/*
 * Scan the block list for dirty blocks; add them to the list c->baddr.
 */
static void
flushFill(Cache *c)
{
	int i, ndirty;
	BAddr *p;
	Block *b;

	vtLock(c->lk);
//	if(c->ndirty == 0){
//		vtUnlock(c->lk);
//		return;
//	}

	p = c->baddr;
	ndirty = 0;
	for(i=0; i<c->nblocks; i++){
		b = c->blocks + i;
		if(b->part == PartError)
			continue;
		if(b->iostate == BioDirty || b->iostate == BioWriting)
			ndirty++;
		if(b->iostate != BioDirty)
			continue;
		p->part = b->part;
		p->addr = b->addr;
		p->vers = b->vers;
		p++;
	}
	if(ndirty != c->ndirty){
		fprint(2, "ndirty mismatch expected %d found %d\n",
			c->ndirty, ndirty);
		c->ndirty = ndirty;
	}
	vtUnlock(c->lk);
	
	c->bw = p - c->baddr;
	qsort(c->baddr, c->bw, sizeof(BAddr), baddrCmp);
}

/*
 * This is not thread safe, i.e. it can't be called from multiple threads.
 * 
 * It's okay how we use it, because it only gets called in
 * the flushThread.  And cacheFree, but only after
 * cacheFree has killed off the flushThread.
 */
static int
cacheFlushBlock(Cache *c)
{
	Block *b;
	BAddr *p;
	int lockfail, nfail;

	nfail = 0;
	for(;;){
		if(c->br == c->be){
			if(c->bw == 0 || c->bw == c->be)
				flushFill(c);
			c->br = 0;
			c->be = c->bw;
			c->bw = 0;
			c->nflush = 0;
		}

		if(c->br == c->be)
			return 0;
		p = c->baddr + c->br;
		c->br++;
		b = _cacheLocalLookup(c, p->part, p->addr, p->vers, 0, &lockfail);

		if(b && blockWrite(b)){
			c->nflush++;
			blockPut(b);
			return 1;
		}
		if(b)
			blockPut(b);

		/*
		 * Why didn't we write the block?
		 */

		/* Block already written out */
		if(b == nil && !lockfail)
			continue;

		/* Failed to acquire lock; sleep if happens a lot. */
		if(lockfail && ++nfail > 100)
			sleep(500);

		/* Requeue block. */
		if(c->bw < c->be)
			c->baddr[c->bw++] = *p;
	}
	return 0;
}

/*
 * Occasionally flush dirty blocks from memory to the disk.
 */
static void
flushThread(void *a)
{
	Cache *c = a;
	int i;

	vtThreadSetName("flush");
	vtLock(c->lk);
	while(c->die == nil){
		vtSleep(c->flush);
		vtUnlock(c->lk);
		for(i=0; i<FlushSize; i++)
			if(!cacheFlushBlock(c))
				break;
		vtLock(c->lk);
		vtWakeupAll(c->flushwait);
	}
	c->ref--;
	vtWakeup(c->die);
	vtUnlock(c->lk);
}

/*
 * Keep flushing until everything is clean.
 */
void
cacheFlush(Cache *c, int wait)
{
	vtLock(c->lk);
	if(wait){
		while(c->ndirty){
			consPrint("cacheFlush: %d dirty blocks\n", c->ndirty);
			vtWakeup(c->flush);
			vtSleep(c->flushwait);
		}
		consPrint("cacheFlush: done\n", c->ndirty);
	}else
		vtWakeup(c->flush);
	vtUnlock(c->lk);
}

/*
 * Kick the flushThread every 30 seconds.
 */
static void
cacheSync(void *v)
{
	Cache *c;

	c = v;
	cacheFlush(c, 0);
}
