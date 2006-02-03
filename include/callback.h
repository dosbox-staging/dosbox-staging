/*
 *  Copyright (C) 2002-2005  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: callback.h,v 1.15 2006-02-03 17:07:41 harekiet Exp $ */

#ifndef DOSBOX_CALLBACK_H
#define DOSBOX_CALLBACK_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif 

typedef Bitu (*CallBack_Handler)(void);
extern CallBack_Handler CallBack_Handlers[];

enum { CB_RETN, CB_RETF,CB_IRET,CB_IRET_STI };

#define CB_MAX 144
#define CB_SEG 0xC800
#define CB_BASE (CB_SEG << 4)

enum {	
	CBRET_NONE=0,CBRET_STOP=1
};

extern Bit8u lastint;
INLINE RealPt CALLBACK_RealPointer(Bitu callback) {
	return RealMake(CB_SEG,callback << 4);
}

Bitu CALLBACK_Allocate();

void CALLBACK_Idle(void);


void CALLBACK_RunRealInt(Bit8u intnum);
void CALLBACK_RunRealFar(Bit16u seg,Bit16u off);

bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,const char* description=0);
/* Returns with the size of the extra callback */
Bitu CALLBACK_SetupExtra(Bitu callback, Bitu type, PhysPt physAddress);

const char* CALLBACK_GetDescription(Bitu callback);
bool CALLBACK_Free(Bitu callback);

void CALLBACK_SCF(bool val);
void CALLBACK_SZF(bool val);

extern Bitu call_priv_io;


class CALLBACK_HandlerObject{
private:
	bool installed;
	Bit16u m_callback;
	enum {NONE,SETUP,SETUPAT} m_type;
    struct {	
		RealPt old_vector;
		Bit8u interrupt;
		bool installed;
	} vectorhandler;
public:
	CALLBACK_HandlerObject():installed(false),m_type(NONE){vectorhandler.installed=false;}
	~CALLBACK_HandlerObject();
	//Install and allocate a callback.
	void Install(CallBack_Handler handler,Bitu type,const char* description=0);
	//Only allocate a callback number
	void Allocate(CallBack_Handler handler,const char* description=0);
	Bit16u Get_callback(){return m_callback;}
	RealPt Get_RealPointer(){ return RealMake(CB_SEG,m_callback << 4);}
	void Set_RealVec(Bit8u vec);
};
#endif
