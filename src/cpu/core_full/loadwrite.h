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
	reg_sp-=2;
	SaveMw(SegBase(ss)+reg_sp,blah);
};

static INLINE void Push_32(Bit32u blah)	{
	reg_sp-=4;
	SaveMd(SegBase(ss)+reg_sp,blah);
};

static INLINE Bit16u Pop_16() {
	Bit16u temp=LoadMw(SegBase(ss)+reg_sp);
	reg_sp+=2;
	return temp;
};

static INLINE Bit32u Pop_32() {
	Bit32u temp=LoadMd(SegBase(ss)+reg_sp);
	reg_sp+=4;
	return temp;
};


#define Save_Flagsw(FLAGW)											\
{																	\
	flags.type=t_UNKNOWN;											\
	flags.cf	=(FLAGW & 0x001)>0;flags.pf	=(FLAGW & 0x004)>0;		\
	flags.af	=(FLAGW & 0x010)>0;flags.zf	=(FLAGW & 0x040)>0;		\
	flags.sf	=(FLAGW & 0x080)>0;flags.tf	=(FLAGW & 0x100)>0;		\
	flags.intf	=(FLAGW & 0x200)>0;									\
	flags.df	=(FLAGW & 0x400)>0;flags.of	=(FLAGW & 0x800)>0;		\
	flags.io	=(FLAGW >> 12) & 0x03;								\
	flags.nt	=(FLAGW & 0x4000)>0;								\
	if (flags.intf && PIC_IRQCheck) {								\
		SaveIP();													\
		PIC_runIRQs();												\
		LoadIP();													\
	}																\
}

