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

#include "dosbox.h"

#if C_IPX

#include <string.h>
#include <time.h>
#include <stdio.h>
#include "cross.h"
#include "support.h"
#include "cpu.h"
#include "SDL_net.h"
#include "regs.h"
#include "inout.h"
#include "setup.h"
#include "debug.h"
#include "callback.h"
#include "dos_system.h"
#include "mem.h"
#include "ipx.h"
#include "ipxserver.h"
#include "timer.h"
#include "SDL_net.h"
#include "programs.h"

#define SOCKTABLESIZE	150 // DOS IPX driver was limited to 150 open sockets
#define DOS_MEMSEG		0xd000;

Bit32u tcpPort;
bool isIpxServer;
bool isIpxConnected;
IPaddress ipxClientIp;  // IPAddress for client connection to server
IPaddress ipxServConnIp;  // IPAddress for client connection to server
TCPsocket ipxTCPClientSocket;
UDPsocket ipxClientSocket;
int UDPChannel;  // Channel used by UDP connection
Bit8u recvBuffer[IPXBUFFERSIZE]; // Incoming packet buffer
Bitu call_ipx; // Callback of RETF entrypoint
Bitu call_ipxint; // Callback of INT 7A entrypoint
Bitu call_ipxesr1; // Callback of ESR init routine
Bitu call_ipxesr2; // Callback of ESR return routine
Bit16u dospage;
static RealPt ipx_callback;
static RealPt ipx_intcallback;
static RealPt ipx_esrcallback;
static RealPt ipx_esrptraddr;
static RealPt processedECB;

SDLNet_SocketSet clientSocketSet;
static bool inESR;

struct ipxnetaddr {
	Uint8 netnum[4];   // Both are big endian
	Uint8 netnode[6];
} localIpxAddr;

struct fragmentDescriptor {
	Bit16u offset;
	Bit16u segment;
	Bit16u size;
};

packetBuffer incomingPacket;

static Bit16u swapByte(Bit16u sockNum) {
	return (((sockNum>> 8)) | (sockNum << 8));
}

void UnpackIP(PackedIP ipPack, IPaddress * ipAddr) {
	ipAddr->host = ipPack.host;
	ipAddr->port = ipPack.port;
}

void PackIP(IPaddress ipAddr, PackedIP *ipPack) {
	ipPack->host = ipAddr.host;
	ipPack->port = ipAddr.port;
}

class ECBClass {
public:
	RealPt ECBAddr;
    
	ECBClass *prevECB;
	ECBClass *nextECB;

	Bit16u getSocket(void) {
		return swapByte(real_readw(RealSeg(ECBAddr), RealOff(ECBAddr) + 0xa));
	}

	Bit8u getInUseFlag(void) {
		return real_readb(RealSeg(ECBAddr), RealOff(ECBAddr) + 0x8);
	}

	void setInUseFlag(Bit8u flagval) {
		real_writeb(RealSeg(ECBAddr), RealOff(ECBAddr) + 0x8, flagval);
	}

	void setCompletionFlag(Bit8u flagval) {
		real_writeb(RealSeg(ECBAddr), RealOff(ECBAddr) + 0x9, flagval);
	}

	Bit16u getFragCount(void) {
		return real_readw(RealSeg(ECBAddr), RealOff(ECBAddr) + 34);
	}

	void getFragDesc(Bit16u descNum, fragmentDescriptor *fragDesc) {
		Bit16u memoff = RealOff(ECBAddr) + 30 + ((descNum+1) * 6);
		fragDesc->offset = real_readw(RealSeg(ECBAddr), memoff);
		memoff+=2;
		fragDesc->segment = real_readw(RealSeg(ECBAddr), memoff);
		memoff+=2;
		fragDesc->size = real_readw(RealSeg(ECBAddr), memoff);
	}

	RealPt getESRAddr(void) {
		return RealMake(real_readw(RealSeg(ECBAddr), RealOff(ECBAddr)+6), real_readw(RealSeg(ECBAddr), RealOff(ECBAddr)+4));
	}

	void NotifyESR(void) {
		RealPt tmpAddr = getESRAddr();
		if(tmpAddr == 0) return;
		if(!inESR) {
			//LOG_MSG("Calling ESR");
			processedECB = ECBAddr;
			inESR = true;
			//LOG_MSG("Write %x to %x", real_readd(RealSeg(ECBAddr), RealOff(ECBAddr)+4), RealMake(RealSeg(ipx_esrptraddr), RealOff(ipx_esrptraddr)));
			real_writed(RealSeg(ipx_esrptraddr), RealOff(ipx_esrptraddr),real_readd(RealSeg(ECBAddr), RealOff(ECBAddr)+4));
			CPU_CALL(false, RealSeg(ipx_esrcallback), RealOff(ipx_esrcallback),0);
		}
	}

	void setImmAddress(Bit8u *immAddr) {
		Bits i;
		for(i=0;i<6;i++) {
			real_writeb(RealSeg(ECBAddr), RealOff(ECBAddr)+28, immAddr[i]);
		}
	}

};

ECBClass *ECBList;  // Linked list of ECB's

ECBClass * CreateECB(RealPt useAddr) {
	ECBClass *tmpECB;
	tmpECB = new ECBClass();
	
	tmpECB->ECBAddr = useAddr;
	tmpECB->prevECB = NULL;
	tmpECB->nextECB = NULL;
	
	if (ECBList == NULL) {
		ECBList = tmpECB;
	} else {
		// Transverse the list until we hit the end
		ECBClass *useECB;
		useECB = ECBList;
		while(useECB->nextECB != NULL) {
			useECB = useECB->nextECB;
		}
		useECB->nextECB = tmpECB;
		tmpECB->prevECB = useECB;
	}

	return tmpECB;
}

void DeleteECB(ECBClass * useECB) {
	if(useECB == NULL) return;

	if(useECB->prevECB == NULL) {
		ECBList = useECB->nextECB;
		if(ECBList != NULL) ECBList->prevECB = NULL;
	} else {
		useECB->prevECB->nextECB = useECB->nextECB;
		if(useECB->nextECB != NULL) useECB->nextECB->prevECB = useECB->prevECB;
	}
	delete useECB;
}

Bit16u socketCount;
Bit16u opensockets[SOCKTABLESIZE]; 

static bool sockInUse(Bit16u sockNum) {
	Bit16u i;
	for(i=0;i<socketCount;i++) {
		if (opensockets[i] == sockNum) return true;
	}
	return false;
}

static void OpenSocket(void) {
	Bit16u sockNum, sockAlloc;

	sockNum = swapByte(reg_dx);

	if(socketCount >= SOCKTABLESIZE) {
		reg_al = 0xfe; // Socket table full
		return;
	}

	if(sockNum == 0x0000) {
		// Dynamic socket allocation
		sockAlloc = 0x4002;
		while(sockInUse(sockAlloc) && (sockAlloc < 0x7fff)) sockAlloc++;
		if(sockAlloc > 0x7fff) {
			// I have no idea how this could happen if the IPX driver is limited to 150 open sockets at a time
			LOG_MSG("IPX: Out of dynamic sockets");
		}
		sockNum = sockAlloc;
	} else {
		if(sockInUse(sockNum)) {
			reg_al = 0xff; // Socket already open
			return;
		} 
	}

	opensockets[socketCount] = sockNum;
	socketCount++;

	reg_al = 0x00; // Success
	reg_dx = swapByte(sockNum);  // Convert back to big-endian

}

static void CloseSocket(void) {
	Bit16u sockNum, i;

	sockNum = swapByte(reg_dx);
	if(!sockInUse(sockNum)) return;

	for(i=0;i<socketCount-1;i++) {
		if (opensockets[i] == sockNum) {
			// Realign list of open sockets
			memcpy(&opensockets[i], &opensockets[i+1], SOCKTABLESIZE - (i + 1));
			break;
		}
	}
	--socketCount;
}

static bool IPX_Multiplex(void)
{
	if(reg_ax != 0x7a00) return false;
	reg_al = 0xff;
	SegSet16(es,RealSeg(ipx_callback));
	reg_di = RealOff(ipx_callback);
	return true;
}

static void handleIpxRequest(void) {
	ECBClass *tmpECB;

	switch (reg_bx) {
		case 0x0000:  // Open socket
			OpenSocket();
			LOG_MSG("IPX: Open socket %4x", swapByte(reg_dx));
			break;
		case 0x0001:  // Close socket
			LOG_MSG("IPX: Close socket %4x", swapByte(reg_dx));
			CloseSocket();
			break;
		case 0x0003:  // Send packet
			if(!incomingPacket.connected) {
				tmpECB = CreateECB(RealMake(SegValue(es), reg_si));
				tmpECB->setInUseFlag(USEFLAG_AVAILABLE);
				tmpECB->setCompletionFlag(COMP_UNDELIVERABLE);
				DeleteECB(tmpECB);
				reg_al = 0xff; // Failure
			} else {
				tmpECB = CreateECB(RealMake(SegValue(es), reg_si));
				tmpECB->setInUseFlag(USEFLAG_SENDING);
				reg_al = 0x00; // Success
			}
			//LOG_MSG("IPX: Sending packet on %4x", tmpECB->getSocket());
			break;
		case 0x0004:  // Listen for packet
			tmpECB = CreateECB(RealMake(SegValue(es), reg_si));
			if(!sockInUse(tmpECB->getSocket())) {
				reg_al = 0xff;  // Socket is not open
				tmpECB->setInUseFlag(USEFLAG_AVAILABLE);
				tmpECB->setCompletionFlag(COMP_HARDWAREERROR);
				DeleteECB(tmpECB);
			} else {
				reg_al = 0x00;  // Success
				tmpECB->setInUseFlag(USEFLAG_LISTENING);
				//LOG_MSG("IPX: Listen for packet on 0x%4x - ESR address %4x:%4x", tmpECB->getSocket(), RealSeg(tmpECB->getESRAddr()), RealOff(tmpECB->getESRAddr()));
			}
			
			
			break;
		case 0x0008:  // Get interval marker
			// ????
			break;
		case 0x0009:  // Get internetwork address
			{
				LOG_MSG("IPX: Get internetwork address %2x:%2x:%2x:%2x:%2x:%2x", localIpxAddr.netnode[5], localIpxAddr.netnode[4], localIpxAddr.netnode[3], localIpxAddr.netnode[2], localIpxAddr.netnode[1], localIpxAddr.netnode[0]);
				Bit8u * addrptr;
				Bits i;

				addrptr = (Bit8u *)&localIpxAddr;
				for(i=0;i<10;i++) {
                    real_writeb(SegValue(es),reg_si+i,addrptr[i]);
				}
				break;
			}
		case 0x000a:  // Relinquish control
			// Idle thingy
			break;
		default:
			LOG_MSG("Unhandled IPX function: %4x", reg_bx);
			break;
	}
	
}

// Entrypoint handler
Bitu IPX_Handler(void) {
	handleIpxRequest();
	return CBRET_NONE;
}

// INT 7A handler
Bitu IPX_IntHandler(void) {
	handleIpxRequest();
	return CBRET_NONE;
}

static void disconnectServer(bool unexpected) {
	
	// There is no Timer remove code, hence this has to be done manually
	incomingPacket.connected = false;

	if(unexpected) LOG_MSG("IPX: Server disconnected unexpectedly");

}

static void pingAck(IPaddress retAddr) {
	IPXHeader regHeader;
	UDPpacket regPacket;
	Bits result;

	SDLNet_Write16(0xffff, regHeader.checkSum);
	SDLNet_Write16(sizeof(regHeader), regHeader.length);
	
	SDLNet_Write32(0, regHeader.dest.network);
	PackIP(retAddr, &regHeader.dest.addr.byIP);
	SDLNet_Write16(0x2, regHeader.dest.socket);

	SDLNet_Write32(0, regHeader.src.network);
	memcpy(regHeader.src.addr.byNode.node, localIpxAddr.netnode, sizeof(localIpxAddr));
	SDLNet_Write16(0x2, regHeader.src.socket);
	regHeader.transControl = 0;
	regHeader.pType = 0x0;

	regPacket.data = (Uint8 *)&regHeader;
	regPacket.len = sizeof(regHeader);
	regPacket.maxlen = sizeof(regHeader);
	regPacket.channel = UDPChannel;
	
	result = SDLNet_UDP_Send(ipxClientSocket, regPacket.channel, &regPacket);

}

static void pingSend(void) {
	IPXHeader regHeader;
	UDPpacket regPacket;
	Bits result;

	SDLNet_Write16(0xffff, regHeader.checkSum);
	SDLNet_Write16(sizeof(regHeader), regHeader.length);
	
	SDLNet_Write32(0, regHeader.dest.network);
	regHeader.dest.addr.byIP.host = 0xffffffff;
	regHeader.dest.addr.byIP.port = 0xffff;
	SDLNet_Write16(0x2, regHeader.dest.socket);

	SDLNet_Write32(0, regHeader.src.network);
	memcpy(regHeader.src.addr.byNode.node, localIpxAddr.netnode, sizeof(localIpxAddr));
	SDLNet_Write16(0x2, regHeader.src.socket);
	regHeader.transControl = 0;
	regHeader.pType = 0x0;

	regPacket.data = (Uint8 *)&regHeader;
	regPacket.len = sizeof(regHeader);
	regPacket.maxlen = sizeof(regHeader);
	regPacket.channel = UDPChannel;
	
	result = SDLNet_UDP_Send(ipxClientSocket, regPacket.channel, &regPacket);
	if(!result) {
		LOG_MSG("IPX: SDLNet_UDP_Send: %s\n", SDLNet_GetError());
	}

}

static void receivePacket(Bit8u *buffer, Bit16s bufSize) {
	ECBClass *useECB;
	ECBClass *nextECB;
	fragmentDescriptor tmpFrag;
	Bit16u i, fragCount,t;
	Bit16s bufoffset;
	Bit16u *bufword = (Bit16u *)buffer;
	Bit16u useSocket = swapByte(bufword[8]);
	Bit32u hostaddr;
	IPXHeader * tmpHeader;
	tmpHeader = (IPXHeader *)buffer;

	// Check to see if ping packet
	if(useSocket == 0x2) {
		// Is this a broadcast?
		if((tmpHeader->dest.addr.byIP.host == 0xffffffff) && (tmpHeader->dest.addr.byIP.port = 0xffff)) {
			// Yes.  We should return the ping back to the sender
			IPaddress tmpAddr;
			UnpackIP(tmpHeader->src.addr.byIP, &tmpAddr);
			pingAck(tmpAddr);
			return;
		}
	}


	useECB = ECBList;
	while(useECB != NULL) {
		nextECB = useECB->nextECB;
		if(useECB->getInUseFlag() == USEFLAG_LISTENING) {
			if(useECB->getSocket() == useSocket) {
				useECB->setInUseFlag(USEFLAG_AVAILABLE);
				fragCount = useECB->getFragCount(); 
				bufoffset = 0;
				for(i=0;i<fragCount;i++) {
					useECB->getFragDesc(i,&tmpFrag);
					for(t=0;t<tmpFrag.size;t++) {
						real_writeb(tmpFrag.segment, tmpFrag.offset+t, buffer[bufoffset]);
						bufoffset++;
						if(bufoffset>=bufSize) {
							useECB->setCompletionFlag(COMP_SUCCESS);
							useECB->setImmAddress(&buffer[22]);  // Write in source node
							hostaddr = *((Bit32u *)&buffer[24]);

							//LOG_MSG("IPX: Received packet of %d bytes from %d.%d.%d.%d (%x CRC)", bufSize, CONVIP(hostaddr), packetCRC(&buffer[30], bufSize-30));
							useECB->NotifyESR();
							DeleteECB(useECB);
							return;
						}
					}
				}
				if(bufoffset < bufSize) {
					useECB->setCompletionFlag(COMP_MALFORMED);
					useECB->NotifyESR();
					DeleteECB(useECB);
					return;
				}
			}
		}
		useECB = nextECB;
	}

}

static void sendPacketsTCP(void) {
	ECBClass *useECB;
	ECBClass *nextECB;
	char outbuffer[IPXBUFFERSIZE];
	fragmentDescriptor tmpFrag; 
	Bit16u i, fragCount,t;
	Bit16s packetsize;
	Bit16u *wordptr;
	Bits result;

	useECB = ECBList;
	while(useECB != NULL) {
		nextECB = useECB->nextECB;
		if(useECB->getInUseFlag() == USEFLAG_SENDING) {
			useECB->setInUseFlag(USEFLAG_AVAILABLE);
			packetsize = 0;
			fragCount = useECB->getFragCount(); 
			for(i=0;i<fragCount;i++) {
				useECB->getFragDesc(i,&tmpFrag);
				if(i==0) {
					// Fragment containing IPX header
					// Must put source address into header
					Bit8u * addrptr;
					Bits m;

					addrptr = (Bit8u *)&localIpxAddr.netnode;
					for(m=0;m<6;m++) {
						real_writeb(tmpFrag.segment,tmpFrag.offset+m+22,addrptr[m]);
					}
				}
				for(t=0;t<tmpFrag.size;t++) {
					outbuffer[packetsize] = real_readb(tmpFrag.segment, tmpFrag.offset + t);
					packetsize++;
					if(packetsize>=IPXBUFFERSIZE) {
						LOG_MSG("IPX: Packet size to be sent greater than %d bytes.", IPXBUFFERSIZE);
						useECB->setCompletionFlag(COMP_MALFORMED);
						useECB->NotifyESR();
						DeleteECB(useECB);
						goto nextECB;
					}
				}
			}
			result = SDLNet_TCP_Send(ipxTCPClientSocket, &packetsize, 2);
			if(result != 2) {
				useECB->setCompletionFlag(COMP_UNDELIVERABLE);
				useECB->NotifyESR();
				DeleteECB(useECB);
				disconnectServer(true); 
				return;
			}
			
			// Add length and source socket to IPX header
			wordptr = (Bit16u *)&outbuffer[0];
			// Blank CRC
            wordptr[0] = 0xffff;
			// Length
			wordptr[1] = swapByte(packetsize);
			// Source socket
			wordptr[14] = swapByte(useECB->getSocket());

			result = SDLNet_TCP_Send(ipxTCPClientSocket, &outbuffer[0], packetsize);
			if(result != packetsize) {
				useECB->setCompletionFlag(COMP_UNDELIVERABLE);
				useECB->NotifyESR();
				DeleteECB(useECB);
				disconnectServer(true);
				return;
			}
			useECB->setInUseFlag(USEFLAG_AVAILABLE);
			useECB->setCompletionFlag(COMP_SUCCESS);
			useECB->NotifyESR();
			DeleteECB(useECB);

		}
nextECB:

		useECB = nextECB;
	}

}


static void IPX_TCPClientLoop(void) {
	Bits result;

	// Check for incoming packets
	SDLNet_CheckSockets(clientSocketSet,0);
	
	if(SDLNet_SocketReady(ipxClientSocket)) {
		if(!incomingPacket.inPacket) {
			if(!incomingPacket.waitsize) {
				result = SDLNet_TCP_Recv(ipxTCPClientSocket, &incomingPacket.packetSize, 2);
				if(result!=2) {
					if(result>0) {
						incomingPacket.waitsize = true;
						goto finishReceive;
					} else {
						disconnectServer(true);
						goto finishReceive;
					}
				}
				incomingPacket.packetRead = 0;
			} else {
				Bit8u * nextchar;
				nextchar = (Bit8u *)&incomingPacket.packetSize;
				nextchar++;
				result = SDLNet_TCP_Recv(ipxTCPClientSocket, nextchar, 1);
				if(result!=1) {
					if(result>0) {
						LOG_MSG("IPX: Packet overrun");
					} else {
						disconnectServer(true);
						goto finishReceive;
					}
				}
				incomingPacket.waitsize = false;
				incomingPacket.packetRead = 0;
			}
			incomingPacket.inPacket = true;
		}
		result = SDLNet_TCP_Recv(ipxTCPClientSocket, &incomingPacket.buffer[incomingPacket.packetRead], incomingPacket.packetSize);
		if (result>0) {
			incomingPacket.packetRead+=result;
			incomingPacket.packetSize-=result;
			if(incomingPacket.packetSize<=0) {
				// IPX packet is complete.  Now interpret IPX header and try to match to listening ECB
				receivePacket(&incomingPacket.buffer[0], incomingPacket.packetRead);
				incomingPacket.inPacket = false;
			}
		} else {
			// Clost active socket
			disconnectServer(true);
		}

	}

finishReceive:;


}

static void IPX_UDPClientLoop(void) {
	int numrecv;
	UDPpacket inPacket;
	inPacket.data = (Uint8 *)recvBuffer;
	inPacket.maxlen = IPXBUFFERSIZE;
	inPacket.channel = UDPChannel;

	// Its amazing how much simpler UDP is than TCP
	numrecv = SDLNet_UDP_Recv(ipxClientSocket, &inPacket);
	if(numrecv) {
		receivePacket(inPacket.data, inPacket.len);
	}
	
}

static void sendPackets() {
	ECBClass *useECB;
	ECBClass *nextECB;
	char outbuffer[IPXBUFFERSIZE];
	fragmentDescriptor tmpFrag; 
	Bit16u i, fragCount,t;
	Bit16s packetsize;
	Bit16u *wordptr;
	Bits result;
	UDPpacket outPacket;

	useECB = ECBList;
	while(useECB != NULL) {
		nextECB = useECB->nextECB;
		if(useECB->getInUseFlag() == USEFLAG_SENDING) {
			useECB->setInUseFlag(USEFLAG_AVAILABLE);
			packetsize = 0;
			fragCount = useECB->getFragCount(); 
			for(i=0;i<fragCount;i++) {
				useECB->getFragDesc(i,&tmpFrag);
				if(i==0) {
					// Fragment containing IPX header
					// Must put source address into header
					Bit8u * addrptr;
					Bits m;

					addrptr = (Bit8u *)&localIpxAddr.netnode;
					for(m=0;m<6;m++) {
						real_writeb(tmpFrag.segment,tmpFrag.offset+m+22,addrptr[m]);
					}
				}
				for(t=0;t<tmpFrag.size;t++) {
					outbuffer[packetsize] = real_readb(tmpFrag.segment, tmpFrag.offset + t);
					packetsize++;
					if(packetsize>=IPXBUFFERSIZE) {
						LOG_MSG("IPX: Packet size to be sent greater than %d bytes.", IPXBUFFERSIZE);
						useECB->setCompletionFlag(COMP_UNDELIVERABLE);
						useECB->NotifyESR();
						DeleteECB(useECB);
						goto nextECB;
					}
				}
			}
			
			// Add length and source socket to IPX header
			wordptr = (Bit16u *)&outbuffer[0];
			// Blank CRC
            wordptr[0] = 0xffff;
			// Length
			wordptr[1] = swapByte(packetsize);
			// Source socket
			wordptr[14] = swapByte(useECB->getSocket());

			outPacket.channel = UDPChannel;
			outPacket.data = (Uint8 *)&outbuffer[0];
			outPacket.len = packetsize;
			outPacket.maxlen = packetsize;
			// Since we're using a channel, we won't send the IP address again
			result = SDLNet_UDP_Send(ipxClientSocket, UDPChannel, &outPacket);
			if(result == 0) {
				LOG_MSG("IPX: Could not send packet: %s", SDLNet_GetError());
				useECB->setCompletionFlag(COMP_UNDELIVERABLE);
				useECB->NotifyESR();
				DeleteECB(useECB);
				disconnectServer(true);
				return;
			}
			useECB->setInUseFlag(USEFLAG_AVAILABLE);
			useECB->setCompletionFlag(COMP_SUCCESS);
			useECB->NotifyESR();
			DeleteECB(useECB);

		}
nextECB:

		useECB = nextECB;
	}

}

static void IPX_ClientLoop(void) {
	IPX_UDPClientLoop();

	// Send outgoing packets
	sendPackets();
}


static bool pingCheck(IPXHeader * outHeader) {
	char buffer[1024];
	Bits result;
	UDPpacket regPacket;

	IPXHeader *regHeader;
	regPacket.data = (Uint8 *)buffer;
	regPacket.maxlen = sizeof(buffer);
	regPacket.channel = UDPChannel;
	regHeader = (IPXHeader *)buffer;
	
	result = SDLNet_UDP_Recv(ipxClientSocket, &regPacket);
	if (result != 0) {
		memcpy(outHeader, regHeader, sizeof(IPXHeader));
		return true;
	}
	return false;


}

bool ConnectToServer(char *strAddr) {
	int numsent;
	UDPpacket regPacket;
	IPXHeader regHeader;

	if(!SDLNet_ResolveHost(&ipxServConnIp, strAddr, (Bit16u)tcpPort)) {
		
		// Select an anonymous UDP port
		ipxClientSocket = SDLNet_UDP_Open(0);
		if(ipxClientSocket) {
			// Bind UDP port to address to channel
			UDPChannel = SDLNet_UDP_Bind(ipxClientSocket,-1,&ipxServConnIp);
			//ipxClientSocket = SDLNet_TCP_Open(&ipxServConnIp);
			SDLNet_Write16(0xffff, regHeader.checkSum);
			SDLNet_Write16(sizeof(regHeader), regHeader.length);

			// Echo packet with zeroed dest and src is a server registration packet
			SDLNet_Write32(0, regHeader.dest.network);
			regHeader.dest.addr.byIP.host = 0x0;
			regHeader.dest.addr.byIP.port = 0x0;
			SDLNet_Write16(0x2, regHeader.dest.socket);

			SDLNet_Write32(0, regHeader.src.network);
			regHeader.src.addr.byIP.host = 0x0;
			regHeader.src.addr.byIP.port = 0x0;
			SDLNet_Write16(0x2, regHeader.src.socket);
			regHeader.transControl = 0;

			regPacket.data = (Uint8 *)&regHeader;
			regPacket.len = sizeof(regHeader);
			regPacket.maxlen = sizeof(regHeader);
			regPacket.channel = UDPChannel;
			// Send registration string to server.  If server doesn't get this, client will not be registered
			numsent = SDLNet_UDP_Send(ipxClientSocket, regPacket.channel, &regPacket);
			
			if(!numsent) {
				LOG_MSG("IPX: Unable to connect to server: %s", SDLNet_GetError());
				SDLNet_UDP_Close(ipxClientSocket);
				return false;
			} else {
				// Wait for return packet from server.  This will contain our IPX address and port num
				Bits result;
				Bit32u ticks, elapsed;
				ticks = GetTicks();

				while(true) {
					elapsed = GetTicks() - ticks;
					if(elapsed > 5000) {
						LOG_MSG("Timeout connecting to server at %s", strAddr);
						SDLNet_UDP_Close(ipxClientSocket);

						return false;
					}
					CALLBACK_Idle();
					result = SDLNet_UDP_Recv(ipxClientSocket, &regPacket);
					if (result != 0) {
						memcpy(localIpxAddr.netnode, regHeader.dest.addr.byNode.node, sizeof(localIpxAddr.netnode));
						memcpy(localIpxAddr.netnum, regHeader.dest.network, sizeof(localIpxAddr.netnum));
						break;
					}
					
				}

				LOG_MSG("IPX: Connected to server.  IPX address is %d:%d:%d:%d:%d:%d", CONVIPX(localIpxAddr.netnode));


				incomingPacket.connected = true;
				TIMER_AddTickHandler(&IPX_ClientLoop);
				return true;
			}
		} else {
			LOG_MSG("IPX: Unable to open socket");

		}
	} else {
		LOG_MSG("IPX: Unable resolve connection to server");
	}
	return false;
}

void DisconnectFromServer(void) {

	if(incomingPacket.connected) {
		incomingPacket.connected = false;
		TIMER_DelTickHandler(&IPX_ClientLoop);
		SDLNet_UDP_Close(ipxClientSocket);
	}
}

bool IPX_NetworkInit() {

	localIpxAddr.netnum[0] = 0x0; localIpxAddr.netnum[1] = 0x0; localIpxAddr.netnum[2] = 0x0; localIpxAddr.netnum[3] = 0x1;

	/*
	if(SDLNet_ResolveHost(&ipxClientIp, localhostname, tcpPort)) {
		LOG_MSG("IPX: Unable to resolve localname: \"%s\".  IPX disabled.", localhostname);
		return false;
	} else {
		LOG_MSG("IPX: Using localname: %s IP is: %d.%d.%d.%d", localhostname, CONVIP(ipxClientIp.host));
	}
	*/

	// Generate the MAC address.  This is made by zeroing out the first two octets and then using the actual IP address for
	// the last 4 octets.  This idea is from the IPX over IP implementation as specified in RFC 1234:
	// http://www.faqs.org/rfcs/rfc1234.html
	localIpxAddr.netnode[0] = 0x00;
	localIpxAddr.netnode[1] = 0x00;
	//localIpxAddr.netnode[5] = (ipxClientIp.host >> 24) & 0xff;
	//localIpxAddr.netnode[4] = (ipxClientIp.host >> 16) & 0xff;
	//localIpxAddr.netnode[3] = (ipxClientIp.host >> 8) & 0xff;
	//localIpxAddr.netnode[2] = (ipxClientIp.host & 0xff);
	//To be filled in on response from server
	localIpxAddr.netnode[2] = 0x00;
	localIpxAddr.netnode[3] = 0x00;
	localIpxAddr.netnode[4] = 0x00;
	localIpxAddr.netnode[5] = 0x00;

	socketCount = 0;
	return true;
}

class IPXNET : public Program {
public:
	void HelpCommand(const char *helpStr) {
		// Help on connect command
		if(strcasecmp("connect", helpStr) == 0) {
			WriteOut("IPXNET CONNECT opens a connection to an IPX tunneling server running on another\n");
			WriteOut("DosBox session.  The \"address\" parameter specifies the IP address or host name\n");
			WriteOut("of the server computer.  One can also specify the UDP port to use.  By default\n");
			WriteOut("IPXNET uses port 213, the assigned IANA port for IPX tunneling, for its\nconnection.\n\n");
			WriteOut("The syntax for IPXNET CONNECT is:\n\n");
			WriteOut("IPXNET CONNECT address <port>\n\n");
			return;
		}
		// Help on the disconnect command
		if(strcasecmp("disconnect", helpStr) == 0) {
			WriteOut("IPXNET DISCONNECT closes the connection to the IPX tunneling server.\n\n");
			WriteOut("The syntax for IPXNET DISCONNECT is:\n\n");
			WriteOut("IPXNET DISCONNECT\n\n");
			return;
		}
		// Help on the startserver command
		if(strcasecmp("startserver", helpStr) == 0) {
			WriteOut("IPXNET STARTSERVER starts and IPX tunneling server on this DosBox session.  By\n");
			WriteOut("default, the server will accept connections on UDP port 213, though this can be\n");
			WriteOut("changed.  Once the server is started, DosBox will automatically start a client\n");
			WriteOut("connection to the IPX tunneling server.\n\n");
			WriteOut("The syntax for IPXNET STARTSERVER is:\n\n");
			WriteOut("IPXNET STARTSERVER <port>\n\n");
			return;
		}
		// Help on the stop server command
		if(strcasecmp("stopserver", helpStr) == 0) {
			WriteOut("IPXNET STOPSERVER stops the IPX tunneling server running on this DosBox\nsession.");
			WriteOut("  Care should be taken to ensure that all other connections have\nterminated ");
			WriteOut("as well sinnce stoping the server may cause lockups on other\nmachines still using ");
			WriteOut("the IPX tunneling server.\n\n");
			WriteOut("The syntax for IPXNET STOPSERVER is:\n\n");
			WriteOut("IPXNET STOPSERVER\n\n");
			return;
		}
		// Help on the ping command
		if(strcasecmp("ping", helpStr) == 0) {
			WriteOut("IPXNET PING broadcasts a ping request through the IPX tunneled network.  In    \n");
			WriteOut("response, all other connected computers will respond to the ping and report\n");
			WriteOut("the time it took to receive and send the ping message.\n\n");
			WriteOut("The syntax for IPXNET PING is:\n\n");
			WriteOut("IPXNET PING\n\n");
			return;
		}
		// Help on the status command
		if(strcasecmp("status", helpStr) == 0) {
			WriteOut("IPXNET STATUS reports the current state of this DosBox's sessions IPX tunneling\n");
			WriteOut("network.  For a list of the computers connected to the network use the IPXNET \n");
			WriteOut("PING command.\n\n");
			WriteOut("The syntax for IPXNET STATUS is:\n\n");
			WriteOut("IPXNET STATUS\n\n");
			return;
		}
	}

	void Run(void)
	{
		WriteOut("IPX Tunneling utility for DosBox\n\n");
		if(!cmd->GetCount()) {
			WriteOut("The syntax of this command is:\n\n");
			WriteOut("IPXNET [ CONNECT | DISCONNECT | STARTSERVER | STOPSERVER | PING | HELP |\n         STATUS ]\n\n");
			return;
		}
		
		if(cmd->FindCommand(1, temp_line)) {
			if(strcasecmp("help", temp_line.c_str()) == 0) {
				if(!cmd->FindCommand(2, temp_line)) {
					WriteOut("The following are valid IPXNET commands:\n\n");
					WriteOut("IPXNET CONNECT        IPXNET DISCONNECT       IPXNET STARTSERVER\n");
					WriteOut("IPXNET STOPSERVER     IPXNET PING             IPXNET STATUS\n\n");
					WriteOut("To get help on a specific command, type:\n\n");
					WriteOut("IPXNET HELP command\n\n");

				} else {
					HelpCommand(temp_line.c_str());
					return;
				}
				return;
			} 
			if(strcasecmp("startserver", temp_line.c_str()) == 0) {
				if(!isIpxServer) {
					if(incomingPacket.connected) {
						WriteOut("IPX Tunneling Client alreadu connected to another server.  Disconnect first.\n");
						return;
					}
					bool startsuccess;
					if(!cmd->FindCommand(2, temp_line)) {
						tcpPort = 213;
					} else {
						tcpPort = strtol(temp_line.c_str(), NULL, 10);
					}
					startsuccess = IPX_StartServer((Bit16u)tcpPort);
					if(startsuccess) {
						WriteOut("IPX Tunneling Server started\n");
						isIpxServer = true;
						ConnectToServer("localhost");
					} else {
						WriteOut("IPX Tunneling Server failed to start\n");
					}
				} else {
					WriteOut("IPX Tunneling Server already started\n");
				}
				return;
			}
			if(strcasecmp("stopserver", temp_line.c_str()) == 0) {
				if(!isIpxServer) {
					WriteOut("IPX Tunneling Server not running in this DosBox session.\n");
				} else {
					isIpxServer = false;
					DisconnectFromServer();
					IPX_StopServer();
					WriteOut("IPX Tunneling Server stopped.");
					// Don't know how to stop the timer just yet.
				}
				return;
			}
			if(strcasecmp("connect", temp_line.c_str()) == 0) {
				char strHost[1024];
				if(incomingPacket.connected) {
					WriteOut("IPX Tunneling Client already connected.\n");
					return;
				}
				if(!cmd->FindCommand(2, temp_line)) {
					WriteOut("IPX Server address not specified.\n");
					return;
				}
				strcpy(strHost, temp_line.c_str());

				if(!cmd->FindCommand(3, temp_line)) {
					tcpPort = 213;
				} else {
					tcpPort = strtol(temp_line.c_str(), NULL, 10);
				}

				if(ConnectToServer(strHost)) {
                	WriteOut("IPX Tunneling Client connected to server at %s.\n", strHost);
				} else {
					WriteOut("IPX Tunneling Client failed to connect to server at %s.\n", strHost);
				}
				return;
			}
			
			if(strcasecmp("disconnect", temp_line.c_str()) == 0) {
				if(!incomingPacket.connected) {
					WriteOut("IPX Tunneling Client not connected.\n");
					return;
				}
				// TODO: Send a packet to the server notifying of disconnect

				WriteOut("IPX Tunneling Client disconnected from server.\n");
				DisconnectFromServer();
				return;
			}

			if(strcasecmp("status", temp_line.c_str()) == 0) {
				WriteOut("IPX Tunneling Status:\n\n");
				WriteOut("Server status: ");
				if(isIpxServer) WriteOut("ACTIVE\n"); else WriteOut("INACTIVE\n");
				WriteOut("Client status: ");
				if(incomingPacket.connected) {
					WriteOut("CONNECTED -- Server at %d.%d.%d.%d port %d\n", CONVIP(ipxServConnIp.host), tcpPort);
				} else {
					WriteOut("DISCONNECTED\n");
				}
				if(isIpxServer) {
					WriteOut("List of active connections:\n\n");
					int i;
					IPaddress *ptrAddr;
					for(i=0;i<SOCKETTABLESIZE;i++) {
						if(IPX_isConnectedToServer(i,&ptrAddr)) {
							WriteOut("     %d.%d.%d.%d from port %d\n", CONVIP(ptrAddr->host), SDLNet_Read16(&ptrAddr->port));
						}
					}
					WriteOut("\n");
				}
				return;
			}

			if(strcasecmp("ping", temp_line.c_str()) == 0) {
				Bit32u ticks;
				IPXHeader pingHead;

				if(!incomingPacket.connected) {
					WriteOut("IPX Tunneling Client not connected.\n");
					return;
				}

				WriteOut("Sending broadcast ping:\n\n");
				pingSend();
				ticks = GetTicks();
				while((GetTicks() - ticks) < 1500) {
					CALLBACK_Idle();
					if(pingCheck(&pingHead)) {
						WriteOut("Response from %d.%d.%d.%d, port %d time=%dms\n", CONVIP(pingHead.src.addr.byIP.host), SDLNet_Read16(&pingHead.src.addr.byIP.port), GetTicks() - ticks);
					}
				}
				return;
			}



		}

		/*
		WriteOut("IPX Status\n\n");
		if(!incomingPacket.connected) {
			WriteOut("IPX tunneling client not presently connected");
			return;
		}
		if(isIpxServer) {
			WriteOut("This DosBox session is an IPX tunneling server running on port %d", tcpPort);
		} else {
			WriteOut("This DosBox session is an IPX tunneling client connected to: %s",SDLNet_ResolveIP(&ipxServConnIp));
		}
		*/
	}
};

static void IPXNET_ProgramStart(Program * * make) {
	*make=new IPXNET;
}

Bitu IPX_ESRHandler1(void) {
	CPU_Push32(reg_flags);
	CPU_Push32(reg_eax);CPU_Push32(reg_ecx);CPU_Push32(reg_edx);CPU_Push32(reg_ebx);
	CPU_Push32(reg_ebp);CPU_Push32(reg_esi);CPU_Push32(reg_edi);
	CPU_Push16(SegValue(ds)); CPU_Push16(SegValue(es)); 

	SegSet16(es, RealSeg(processedECB));
	reg_si = RealOff(processedECB);
	reg_al = 0xff;
	//LOG_MSG("ESR Callback 1");

	return CBRET_NONE;
}

Bitu IPX_ESRHandler2(void) {
	SegSet16(es, CPU_Pop16()); SegSet16(ds, CPU_Pop16());
	reg_edi=CPU_Pop32();reg_esi=CPU_Pop32();reg_ebp=CPU_Pop32();
	reg_ebx=CPU_Pop32();reg_edx=CPU_Pop32();reg_ecx=CPU_Pop32();reg_eax=CPU_Pop32();
	reg_flags=CPU_Pop32();

	//LOG_MSG("Leaving ESR");
	inESR = false;

	return CBRET_NONE;
}

bool IPX_ESRSetupHook(Bitu callback1, CallBack_Handler handler1, Bitu callback2, CallBack_Handler handler2, RealPt *ptrAddr) {
	PhysPt phyDospage;
	phyDospage = PhysMake(dospage,0);

	// Inital callback routine (should save registers, etc.)
	phys_writeb(phyDospage+0,(Bit8u)0xFA);	//CLI
	phys_writeb(phyDospage+1,(Bit8u)0xFE);	//GRP 4
	phys_writeb(phyDospage+2,(Bit8u)0x38);	//Extra Callback instruction
	phys_writew(phyDospage+3,callback1);		//The immediate word
	phys_writeb(phyDospage+5,(Bit8u)0x9a);	//CALL Ap
	// 0x6, 0x7, 0x8, 0x9 = address of called routine
	*ptrAddr = RealMake(dospage, 6);
	phys_writed(phyDospage+6,(Bit32u)0x00000000); // Called address
	phys_writeb(phyDospage+0xa,(Bit8u)0xFE);	//GRP 4
	phys_writeb(phyDospage+0xb,(Bit8u)0x38);	//Extra Callback instruction
	phys_writew(phyDospage+0xc,callback2);	//The immediate word
	phys_writeb(phyDospage+0xe,(Bit8u)0xFB);	//STI
	phys_writeb(phyDospage+0xf,(Bit8u)0xCB);	//A RETF Instruction

	CallBack_Handlers[callback1]=handler1;
	CallBack_Handlers[callback2]=handler2;
	return true;
}

void IPX_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);

	if(!section->Get_bool("ipx")) return;

	if(!SDLNetInited) {
		if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}

	ECBList = NULL;

	isIpxServer = false;
	isIpxConnected = false;

	IPX_NetworkInit();

	inESR = false;

	DOS_AddMultiplexHandler(IPX_Multiplex);
	call_ipx=CALLBACK_Allocate();
	CALLBACK_Setup(call_ipx,&IPX_Handler,CB_RETF);
	ipx_callback=CALLBACK_RealPointer(call_ipx);

	call_ipxint=CALLBACK_Allocate();	
	CALLBACK_Setup(call_ipxint,&IPX_IntHandler,CB_IRET);
	ipx_intcallback=CALLBACK_RealPointer(call_ipxint);

	call_ipxesr1=CALLBACK_Allocate();	
	call_ipxesr2=CALLBACK_Allocate();	
	//CALLBACK_SetupFarCall(call_ipxesr1, &IPX_ESRHandler1, call_ipxesr2, &IPX_ESRHandler2, &ipx_esrptraddr);
	dospage = DOS_GetMemory(1);
	IPX_ESRSetupHook(call_ipxesr1, &IPX_ESRHandler1, call_ipxesr2, &IPX_ESRHandler2, &ipx_esrptraddr);
	// Allocate 16 bytes of memory from the DOS system area at 0xd000
	ipx_esrcallback=RealMake(dospage,0);
	//CALLBACK_RealPointer(call_ipxesr1);

	RealSetVec(0x7a,ipx_intcallback);
	PROGRAMS_MakeFile("IPXNET.COM",IPXNET_ProgramStart);

	//if(isIpxServer) {
		// Auto-connect to server
	//	ConnectToServer("localhost");
	//}

}

#endif
