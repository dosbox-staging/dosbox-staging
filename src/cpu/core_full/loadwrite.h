static INLINE void SaveIP(void) {
	Bitu left=IPPoint-SegBase(cs);
	reg_eip=left;
}

static INLINE void LoadIP(void) {
	IPPoint=SegBase(cs)+reg_eip;
}


static INLINE Bit8u Fetchb() {
	Bit8u temp=LoadMb(IPPoint);
	IPPoint+=1;
	return temp;
}
	
static INLINE Bit16u Fetchw() {
	Bit16u temp=LoadMw(IPPoint);
	IPPoint+=2;
	return temp;
}
static INLINE Bit32u Fetchd() {
	Bit32u temp=LoadMd(IPPoint);
	IPPoint+=4;
	return temp;
}

static INLINE Bit8s Fetchbs() {
	return Fetchb();
}
static INLINE Bit16s Fetchws() {
	return Fetchw();
}

static INLINE Bit32s Fetchds() {
	return Fetchd();
}

static INLINE void Push_16(Bit16u blah)	{
	reg_esp-=2;
	SaveMw(SegBase(ss) + (reg_esp & cpu.stack.mask),blah);
}

static INLINE void Push_32(Bit32u blah)	{
	reg_esp-=4;
	SaveMd(SegBase(ss) + (reg_esp & cpu.stack.mask),blah);
}

static INLINE Bit16u Pop_16(void) {
	Bit16u temp=LoadMw(SegBase(ss) + (reg_esp & cpu.stack.mask));
	reg_esp+=2;
	return temp;
}

static INLINE Bit32u Pop_32(void) {
	Bit32u temp=LoadMd(SegBase(ss) + (reg_esp & cpu.stack.mask));
	reg_esp+=4;
	return temp;
}

