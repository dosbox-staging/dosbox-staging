struct CacheBlock {
public:
	struct {
		Bit16s start,end;				//Where the page is the original code
		Bitu first,last;
	} page;
	struct {
		Bit8u * start;					//Where in the cache are we
		Bitu size;
	} cache;
	BlockType type;
	CodePageHandler * code_page;		//Page containing this code
	CacheBlock * page_link;				//For code crossing a page boundary
	struct {
		CacheBlock * next;
	} hash;
	CacheBlock * list_next;
	struct {
		CacheBlock * to[2];	
		CacheBlock * from[DYN_LINKS];
		Bitu index;
	} link;
	CacheBlock * links[2];
};

static struct {
	struct {
		CacheBlock * first;
		CacheBlock * active;
		CacheBlock * free;
		CacheBlock * running;
	} block;
	Bit8u * pos;
	CacheBlock linkblocks[2];
} cache;

static Bit8u cache_code_link_blocks[2][16];
static Bit8u cache_code[CACHE_TOTAL+CACHE_MAXSIZE];
static CacheBlock cache_blocks[CACHE_BLOCKS];

static void cache_resetblock(CacheBlock * block);

class CodePageHandler :public PageHandler {
public:
	CodePageHandler(PageHandler * _old_pagehandler) {
		old_pagehandler=_old_pagehandler;
		flags=old_pagehandler->flags|PFLAG_HASCODE;
		flags&=~PFLAG_WRITEABLE;
		memset(&hash_map,0,sizeof(hash_map));
		memset(&write_map,0,sizeof(write_map));
	}
	void InvalidateRange(Bits start,Bits end) {
		Bits maps=start>>DYN_HASH_SHIFT;
		Bits map=maps;
		Bits count=write_map[maps];
		while (map>=0 && count>0) {
			CacheBlock * block=hash_map[map];
			CacheBlock * * where=&hash_map[map];
			while (block) {
				CacheBlock * nextblock=block->hash.next;
				if (start<=block->page.end && end>=block->page.start) {
					for (Bitu i=block->page.first;i<=block->page.last;i++) write_map[i]--;
					block->code_page=0;			//Else resetblock will do double work
					count--;
					if (block==cache.block.running) LOG_MSG("Writing to current block");
					cache_resetblock(block);
					*where=nextblock;
				} else {
					where=&block->hash.next;
				}
				block=nextblock;
			}
			map--;
		}
	}
	void writeb(PhysPt addr,Bitu val){
		if (val!=host_readb(hostmem+(addr&4095))) {
			InvalidateRange(addr&4095,addr&4095);
			host_writeb(hostmem+(addr&4095),val);
		}
	}
	void writew(PhysPt addr,Bitu val){
		if (val!=host_readw(hostmem+(addr&4095))) {
			InvalidateRange(addr&4095,(addr&4095)+1);
			host_writew(hostmem+(addr&4095),val);
		}
	}
	void writed(PhysPt addr,Bitu val){
		if (val!=host_readd(hostmem+(addr&4095))) {
			InvalidateRange(addr&4095,(addr&4095)+3);
			host_writed(hostmem+(addr&4095),val);
		}
	}
    void AddCacheBlock(CacheBlock * block) {
		Bit16u first,last;
		if (block->page.start<0) first=0;
		else first=block->page.start>>DYN_HASH_SHIFT;
		block->hash.next=hash_map[first];
		hash_map[first]=block;
		if (block->page.end>=4096) last=DYN_PAGE_HASH-1;
		else last=block->page.end>>DYN_HASH_SHIFT;
		block->page.first=first;
		block->page.last=last;
		for (;first<=last;first++) {
			write_map[first]++;
		}
		block->code_page=this;
	}
	void DelCacheBlock(CacheBlock * block) {
		CacheBlock * * where=&hash_map[block->page.first];
		while (*where) {
			if (*where==block) {
				*where=block->hash.next;
				break;
			}
			where=&((*where)->hash.next);
		}
		for (Bitu i=block->page.first;i<=block->page.last;i++) {
			write_map[i]--;
		}
	}
	CacheBlock * FindCacheBlock(Bitu start) {
		CacheBlock * block=hash_map[start>>DYN_HASH_SHIFT];
		while (block) {
			if (block->page.start==start) return block;
			block=block->hash.next;
		}
		return 0;
	}
	HostPt GetHostPt(Bitu phys_page) { 
		hostmem=old_pagehandler->GetHostPt(phys_page);
		return hostmem;
	}
private:
	PageHandler * old_pagehandler;
	CacheBlock * hash_map[DYN_PAGE_HASH];
	Bit8u write_map[DYN_PAGE_HASH];
	HostPt hostmem;	
};


static INLINE void cache_addunsedblock(CacheBlock * block) {
	block->list_next=cache.block.free;
	cache.block.free=block;
}

static CacheBlock * cache_getblock(void) {
	CacheBlock * ret=cache.block.free;
	if (!ret) E_Exit("Ran out of CacheBlocks" );
	cache.block.free=ret->list_next;
	return ret;
}

static INLINE void cache_clearlinkfrom(CacheBlock * block,CacheBlock * from) {
	for (Bitu i=0;i<DYN_LINKS;i++) {
		if (block->link.from[i]==from) block->link.from[i]=0;
	}
}

static INLINE void cache_clearlinkto(CacheBlock * block,CacheBlock * to) {
	if (block->link.to[0]==to) block->link.to[0]=&cache.linkblocks[0];
	if (block->link.to[1]==to) block->link.to[1]=&cache.linkblocks[1];
}

static void cache_linkblocks(CacheBlock * from,CacheBlock * to,Bitu link) {
	from->link.to[link]=to;
	CacheBlock * clear=to->link.from[to->link.index];
	if (clear) {
		DYN_LOG("backlink buffer full");
		cache_clearlinkto(to->link.from[to->link.index],to);
	}
	to->link.from[to->link.index]=from;
	to->link.index++;
	if (to->link.index>=DYN_LINKS) to->link.index=0;
}

static void cache_resetblock(CacheBlock * block) {
	Bits i;
	DYN_LOG("Resetted block");
	block->type=BT_Free;
	/* Clear all links to this block from other blocks */
	for (i=0;i<DYN_LINKS;i++) {
		if (block->link.from[i]) cache_clearlinkto(block->link.from[i],block);
		block->link.from[i]=0;
	}
	/* Clear all links from this block to other blocks */
	if (block->link.to[0]!=&cache.linkblocks[0]) {
		cache_clearlinkfrom(block->link.to[0],block);
		block->link.to[0]=&cache.linkblocks[0];
	}
	if (block->link.to[1]!=&cache.linkblocks[1]) {
		cache_clearlinkfrom(block->link.to[1],block);
		block->link.to[1]=&cache.linkblocks[1];
	}
	block->link.index=0;
	if (block->code_page) block->code_page->DelCacheBlock(block);
}

static CacheBlock * cache_openblock(void) {
	CacheBlock * block=cache.block.active;
	/* check for enough space in this block */
	Bitu size=block->cache.size;
	CacheBlock * nextblock=block->list_next;
	while (size<CACHE_MAXSIZE) {
		if (!nextblock) goto skipresize;
		size+=nextblock->cache.size;
		CacheBlock * tempblock=nextblock->list_next;
		if (nextblock->type!=BT_Free) cache_resetblock(nextblock);
		cache_addunsedblock(nextblock);
		nextblock=tempblock;
	}
skipresize:
	block->cache.size=size;
	block->list_next=nextblock;
	cache.pos=block->cache.start;
	return block;
}

static void cache_closeblock(BlockType type) {
	CacheBlock * block=cache.block.active;
	/* Setup some structures in the block determined by type */
	block->type=type;
	switch (type) {
	case BT_Normal:
		break;
	case BT_SingleLink:
		block->link.to[0]=&cache.linkblocks[0];
		break;
	case BT_DualLink:
		block->link.to[0]=&cache.linkblocks[0];
		block->link.to[1]=&cache.linkblocks[1];
		break;
	}
	/* Close the block with correct alignments */
	Bitu written=cache.pos-block->cache.start;
	if (written>block->cache.size) {
		if (!block->list_next) {
			if (written>block->cache.size+CACHE_MAXSIZE) E_Exit("CacheBlock overrun");	
		} else E_Exit("CacheBlock overrun");	
	} else {
		Bitu new_size;
		Bitu left=block->cache.size-written;
		/* Smaller than cache align then don't bother to resize */
		if (left>CACHE_ALIGN) {
			new_size=((written-1)|(CACHE_ALIGN-1))+1;
		} else new_size=block->cache.size;
		CacheBlock * newblock=cache_getblock();
		newblock->cache.start=block->cache.start+new_size;
		newblock->cache.size=block->cache.size-new_size;
		newblock->list_next=block->list_next;
		newblock->type=BT_Free;
		block->cache.size=new_size;
		block->list_next=newblock;
	}
	/* Advance the active block pointer */
	if (!block->list_next) {
		DYN_LOG("Cache full restarting");
		cache.block.active=cache.block.first;
	} else {
		cache.block.active=block->list_next;
	}
}

static INLINE void cache_addb(Bit8u val) {
	*cache.pos++=val;
}

static INLINE void cache_addw(Bit16u val) {
	*(Bit16u*)cache.pos=val;
	cache.pos+=2;
}

static INLINE void cache_addd(Bit32u val) {
	*(Bit32u*)cache.pos=val;
	cache.pos+=4;
}


static void gen_return(BlockReturn retcode);

static void cache_init(void) {
	Bits i;
	memset(&cache_blocks,0,sizeof(cache_blocks));
	cache.block.free=&cache_blocks[0];
	for (i=0;i<CACHE_BLOCKS-1;i++) {
		cache_blocks[i].list_next=&cache_blocks[i+1];
		cache_blocks[i].link.to[0]=&cache.linkblocks[0];
		cache_blocks[i].link.to[1]=&cache.linkblocks[1];
	}
	CacheBlock * block=cache_getblock();
	cache.block.first=block;
	cache.block.active=block;
	block->cache.start=&cache_code[0];
	block->cache.size=CACHE_TOTAL;
	block->list_next=0;					//Last block in the list
	cache.pos=&cache_code_link_blocks[0][0];
	cache.linkblocks[0].cache.start=cache.pos;
	gen_return(BR_Link1);
	cache.pos=&cache_code_link_blocks[1][0];
	cache.linkblocks[1].cache.start=cache.pos;
	gen_return(BR_Link2);
}