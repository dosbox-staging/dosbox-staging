/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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


#ifndef DOSBOX_IPX_H
#define DOSBOX_IPX_H

// Uncomment this for a lot of debug messages:
//#define IPX_DEBUGMSG 

#ifdef IPX_DEBUGMSG
#define LOG_IPX LOG_MSG
#else
#if defined (_MSC_VER)
#define LOG_IPX
#else
#define LOG_IPX(...)
#endif
#endif

#ifndef DOSBOX_DOSBOX_H
#include "dosbox.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

// In Use Flag codes
#define USEFLAG_AVAILABLE  0x00
#define USEFLAG_AESTEMP    0xe0
#define USEFLAG_IPXCRIT    0xf8
#define USEFLAG_SPXLISTEN  0xf9
#define USEFLAG_PROCESSING 0xfa
#define USEFLAG_HOLDING    0xfb
#define USEFLAG_AESWAITING 0xfc
#define USEFLAG_AESCOUNT   0xfd
#define USEFLAG_LISTENING  0xfe
#define USEFLAG_SENDING    0xff

// Completion codes
#define COMP_SUCCESS          0x00
#define COMP_REMOTETERM       0xec
#define COMP_DISCONNECT       0xed
#define COMP_INVALIDID        0xee
#define COMP_SPXTABLEFULL     0xef
#define COMP_EVENTNOTCANCELED 0xf9
#define COMP_NOCONNECTION     0xfa
#define COMP_CANCELLED        0xfc
#define COMP_MALFORMED        0xfd
#define COMP_UNDELIVERABLE    0xfe
#define COMP_HARDWAREERROR    0xff

#ifdef _MSC_VER
#pragma pack(1)
#endif

// For Uint8 type
#include "SDL_net.h"

struct PackedIP {
	Uint32 host;
	Uint16 port;
} GCC_ATTRIBUTE(packed);

struct nodeType {
	Uint8 node[6];
} GCC_ATTRIBUTE(packed) ;

struct IPXHeader {
	Uint8 checkSum[2];
	Uint8 length[2];
	Uint8 transControl; // Transport control
	Uint8 pType; // Packet type

	struct transport {
		Uint8 network[4];
		union addrtype {
			nodeType byNode;
			PackedIP byIP ;
		} GCC_ATTRIBUTE(packed) addr;
		Uint8 socket[2];
	} dest, src;
} GCC_ATTRIBUTE(packed);

struct fragmentDescriptor {
	Bit16u offset;
	Bit16u segment;
	Bit16u size;
};

#define IPXBUFFERSIZE 1424

class ECBClass {
public:
	RealPt ECBAddr;
	bool isInESRList;
   	ECBClass *prevECB;	// Linked List
	ECBClass *nextECB;
	
	Bit8u iuflag;		// Need to save data since we are not always in
	Bit16u mysocket;	// real mode

	Bit8u* databuffer;	// received data is stored here until we get called
	Bitu buflen;		// by Interrupt

#ifdef IPX_DEBUGMSG 
	Bitu SerialNumber;
#endif

	ECBClass(Bit16u segment, Bit16u offset);
	Bit16u getSocket(void);

	Bit8u getInUseFlag(void);

	void setInUseFlag(Bit8u flagval);

	void setCompletionFlag(Bit8u flagval);

	Bit16u getFragCount(void);

	bool writeData();
	void writeDataBuffer(Bit8u* buffer, Bit16u length);

	void getFragDesc(Bit16u descNum, fragmentDescriptor *fragDesc);
	RealPt getESRAddr(void);

	void NotifyESR(void);

	void setImmAddress(Bit8u *immAddr);
	void getImmAddress(Bit8u* immAddr);

	~ECBClass();
};

// The following routines may not be needed on all systems.  On my build of SDL the IPaddress structure is 8 octects 
// and therefore screws up my IPXheader structure since it needs to be packed.

void UnpackIP(PackedIP ipPack, IPaddress * ipAddr);
void PackIP(IPaddress ipAddr, PackedIP *ipPack);

#ifdef _MSC_VER
#pragma pack()
#endif

#endif
