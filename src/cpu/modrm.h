// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

extern uint8_t  * lookupRMregb[];
extern uint16_t * lookupRMregw[];
extern uint32_t * lookupRMregd[];
extern uint8_t  * lookupRMEAregb[256];
extern uint16_t * lookupRMEAregw[256];
extern uint32_t * lookupRMEAregd[256];

#define GetRM												\
	uint8_t rm=Fetchb();

#define Getrb												\
	uint8_t * rmrb;											\
	rmrb=lookupRMregb[rm];			
	
#define Getrw												\
	uint16_t * rmrw;											\
	rmrw=lookupRMregw[rm];			

#define Getrd												\
	uint32_t * rmrd;											\
	rmrd=lookupRMregd[rm];			


#define GetRMrb												\
	GetRM;													\
	Getrb;													

#define GetRMrw												\
	GetRM;													\
	Getrw;													

#define GetRMrd												\
	GetRM;													\
	Getrd;													


#define GetEArb												\
	uint8_t * earb=lookupRMEAregb[rm];

#define GetEArw												\
	uint16_t * earw=lookupRMEAregw[rm];

#define GetEArd												\
	uint32_t * eard=lookupRMEAregd[rm];


