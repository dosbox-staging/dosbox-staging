#define SaveIP() reg_eip=(Bit32u)(IPPoint-SegBase(cs));
#define LoadIP() IPPoint=SegBase(cs)+reg_eip;


static INLINE Bit8u the_Fetchb(EAPoint & loc) {
	Bit8u temp=LoadMb(loc);
	loc+=1;
	return temp;
}
	
static INLINE Bit16u the_Fetchw(EAPoint & loc) {
	Bit16u temp=LoadMw(loc);
	loc+=2;
	return temp;
}
static INLINE Bit32u the_Fetchd(EAPoint & loc) {
	Bit32u temp=LoadMd(loc);
	loc+=4;
	return temp;
}

#define Fetchb() the_Fetchb(IPPoint)
#define Fetchw() the_Fetchw(IPPoint)
#define Fetchd() the_Fetchd(IPPoint)

#define Fetchbs() (Bit8s)the_Fetchb(IPPoint)
#define Fetchws() (Bit16s)the_Fetchw(IPPoint)
#define Fetchds() (Bit32s)the_Fetchd(IPPoint)


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

