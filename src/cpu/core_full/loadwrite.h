// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#define SaveIP() reg_eip = inst.cseip - SegBase(cs);
#define LoadIP() inst.cseip=SegBase(cs)+reg_eip;
#define GetIP()	(inst.cseip-SegBase(cs))

#define RunException() {										\
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

static inline uint8_t the_Fetchb(EAPoint & loc) {
	uint8_t temp=LoadMb(loc);
	loc+=1;
	return temp;
}
	
static inline uint16_t the_Fetchw(EAPoint & loc) {
	uint16_t temp=LoadMw(loc);
	loc+=2;
	return temp;
}
static inline uint32_t the_Fetchd(EAPoint & loc) {
	uint32_t temp=LoadMd(loc);
	loc+=4;
	return temp;
}

#define Fetchb() the_Fetchb(inst.cseip)
#define Fetchw() the_Fetchw(inst.cseip)
#define Fetchd() the_Fetchd(inst.cseip)

#define Fetchbs() (int8_t)the_Fetchb(inst.cseip)
#define Fetchws() (int16_t)the_Fetchw(inst.cseip)
#define Fetchds() (int32_t)the_Fetchd(inst.cseip)

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

