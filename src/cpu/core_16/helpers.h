/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#define GetEAa												\
	EAPoint eaa=(*lookupEATable)[rm]();					

#define GetRMEAa											\
	GetRM;													\
	GetEAa;											

#define RMEbGb(inst)														\
	{																		\
		GetRMrb;															\
		if (rm >= 0xc0 ) {GetEArb;inst(*earb,*rmrb,LoadRb,SaveRb);}			\
		else {GetEAa;inst(eaa,*rmrb,LoadMb,SaveMb);}						\
	}

#define RMGbEb(inst)														\
	{																		\
		GetRMrb;															\
		if (rm >= 0xc0 ) {GetEArb;inst(*rmrb,*earb,LoadRb,SaveRb);}			\
		else {GetEAa;inst(*rmrb,LoadMb(eaa),LoadRb,SaveRb);}				\
	}

#define RMEwGw(inst)														\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*earw,*rmrw,LoadRw,SaveRw);}			\
		else {GetEAa;inst(eaa,*rmrw,LoadMw,SaveMw);}						\
	}

#define RMGwEw(inst)														\
	{																		\
		GetRMrw;															\
		if (rm >= 0xc0 ) {GetEArw;inst(*rmrw,*earw,LoadRw,SaveRw);}			\
		else {GetEAa;inst(*rmrw,LoadMw(eaa),LoadRw,SaveRw);}				\
	}																

#define RMEdGd(inst)														\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*eard,*rmrd,LoadRd,SaveRd);}			\
		else {GetEAa;inst(eaa,*rmrd,LoadMd,SaveMd);}						\
	}

#define RMGdEd(inst)														\
	{																		\
		GetRMrd;															\
		if (rm >= 0xc0 ) {GetEArd;inst(*rmrd,*eard,LoadRd,SaveRd);}			\
		else {GetEAa;inst(*rmrd,LoadMd(eaa),LoadRd,SaveRd);}				\
	}																

#define ALIb(inst)															\
	{ inst(reg_al,Fetchb(),LoadRb,SaveRb)}

#define AXIw(inst)															\
	{ inst(reg_ax,Fetchw(),LoadRw,SaveRw);}

#define EAXId(inst)															\
	{ inst(reg_eax,Fetchd(),LoadRd,SaveRd);}
