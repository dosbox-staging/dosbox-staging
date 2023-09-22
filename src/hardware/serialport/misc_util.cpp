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

/* $Id $ */


#include "config.h"

#if C_MODEM

/*****************************************************************************/
// C++ SDLnet wrapper

#include "misc_util.h"

struct _TCPsocketX {
	int ready;
#ifdef NATIVESOCKETS
	SOCKET channel;
#endif
	IPaddress remoteAddress;
	IPaddress localAddress;
	int sflag;
};

Bit32u Netwrapper_GetCapabilities()
{
	Bit32u retval=0;
	retval = CAPWORD;
	return retval;
}

#ifdef NATIVESOCKETS
TCPClientSocket::TCPClientSocket(int platformsocket) {
	sendbuffer=0;
	nativetcpstruct = new Bit8u[sizeof(struct _TCPsocketX)];
	
	mysock = (TCPsocket)nativetcpstruct;
	isopen = false;
	if(!SDLNetInited) {
        if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}
	// fill the SDL socket manually
	((struct _TCPsocketX*)nativetcpstruct)->ready=0;
	((struct _TCPsocketX*)nativetcpstruct)->sflag=0;
	((struct _TCPsocketX*)nativetcpstruct)->channel=(SOCKET) platformsocket;
	sockaddr_in		sa;
	socklen_t		sz;
	sz=sizeof(sa);
	if(getpeername(platformsocket, (sockaddr *)(&sa), &sz)==0) {
		((struct _TCPsocketX*)nativetcpstruct)->
			remoteAddress.host=/*ntohl(*/sa.sin_addr.s_addr;//);
		((struct _TCPsocketX*)nativetcpstruct)->
			remoteAddress.port=/*ntohs(*/sa.sin_port;//);
	}
	else {
		mysock=0;
		return;
	}
	sz=sizeof(sa);
	if(getsockname(platformsocket, (sockaddr *)(&sa), &sz)==0) {
		((struct _TCPsocketX*)nativetcpstruct)->
			localAddress.host=/*ntohl(*/sa.sin_addr.s_addr;//);
		((struct _TCPsocketX*)nativetcpstruct)->
			localAddress.port=/*ntohs(*/sa.sin_port;//);
	}
	else {
		mysock=0;
		return;
	}
	if(mysock!=0) {
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		SDLNet_TCP_AddSocket(listensocketset, mysock);
		isopen=true;
		return;
	}
	mysock=0;
	return;
}
#endif // NATIVESOCKETS

TCPClientSocket::TCPClientSocket(TCPsocket source) {
#ifdef NATIVESOCKETS
	nativetcpstruct=0;
#endif
	sendbuffer=0;
	isopen = false;
	if(!SDLNetInited) {
        if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}	
	
	mysock=0;
	listensocketset=0;
	if(source!=0) {
		mysock = source;
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		SDLNet_TCP_AddSocket(listensocketset, source);

		isopen=true;
	}
}
TCPClientSocket::TCPClientSocket(const char* destination, Bit16u port) {
#ifdef NATIVESOCKETS
	nativetcpstruct=0;
#endif
	sendbuffer=0;
	isopen = false;
	if(!SDLNetInited) {
        if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}	
	mysock=0;
	listensocketset=0;
	
	IPaddress openip;
	//Ancient versions of SDL_net had this as char*. People still appear to be using this one.
	if (!SDLNet_ResolveHost(&openip,const_cast<char*>(destination),port)) {
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		mysock = SDLNet_TCP_Open(&openip);
		if(!mysock) return;
		SDLNet_TCP_AddSocket(listensocketset, mysock);
		isopen=true;
	}
}

TCPClientSocket::~TCPClientSocket() {
	
	if(sendbuffer) delete [] sendbuffer;
#ifdef NATIVESOCKETS
	if(nativetcpstruct) delete [] nativetcpstruct;
	else
#endif
	if(mysock) {
		if(listensocketset) SDLNet_TCP_DelSocket(listensocketset,mysock);
		SDLNet_TCP_Close(mysock);
	}

	if(listensocketset) SDLNet_FreeSocketSet(listensocketset);
}
bool TCPClientSocket::GetRemoteAddressString(Bit8u* buffer) {
	IPaddress* remote_ip;
	Bit8u b1, b2, b3, b4;
	remote_ip=SDLNet_TCP_GetPeerAddress(mysock);
	if(!remote_ip) return false;
	b4=remote_ip->host>>24;
	b3=(remote_ip->host>>16)&0xff;
	b2=(remote_ip->host>>8)&0xff;
	b1=remote_ip->host&0xff;
	sprintf((char*)buffer,"%u.%u.%u.%u",b1,b2,b3,b4);
	return true;
}

bool TCPClientSocket::ReceiveArray(Bit8u* data, Bitu* size) {
	if(SDLNet_CheckSockets(listensocketset,0))
	{
		Bits retval = SDLNet_TCP_Recv(mysock, data, *size);
		if(retval<1) {
			isopen=false;
			*size=0;
			return false;
		} else {
			*size=retval;
			return true;
		}
	}
	else {
		*size=0;
		return true;
	}
}


Bits TCPClientSocket::GetcharNonBlock() {
// return:
// -1: no data
// -2: socket closed
// 0..255: data
	if(SDLNet_CheckSockets(listensocketset,0))
	{
		Bitu retval =0;
		if(SDLNet_TCP_Recv(mysock, &retval, 1)!=1) {
			isopen=false;
			return -2;
		} else return retval;
	}
	else return -1;
}
bool TCPClientSocket::Putchar(Bit8u data) {
	if(SDLNet_TCP_Send(mysock, &data, 1)!=1) {
		isopen=false;
		return false;
	}
	return true;
}

bool TCPClientSocket::SendArray(Bit8u* data, Bitu bufsize) {
	if(SDLNet_TCP_Send(mysock, data, bufsize)!=bufsize) {
		isopen=false;
		return false;
	}
	return true;
}

bool TCPClientSocket::SendByteBuffered(Bit8u data) {
	
	if(sendbufferindex==(sendbuffersize-1)) {
		// buffer is full, get rid of it
		sendbuffer[sendbufferindex]=data;
		sendbufferindex=0;
		
		if(SDLNet_TCP_Send(mysock, sendbuffer, sendbuffersize)!=sendbuffersize) {
			isopen=false;
			return false;
		}
	} else {
		sendbuffer[sendbufferindex]=data;
		sendbufferindex++;
	}
	return true;
}
/*
bool TCPClientSocket::SendArrayBuffered(Bit8u* data, Bitu bufsize) {
	
	Bitu bytes
	while(
	
	// first case, buffer already full
	if(sendbufferindex==(sendbuffersize-1)) {
		// buffer is full, get rid of it
		sendbuffer[sendbufferindex]=data;
		sendbufferindex=0;
		
		if(SDLNet_TCP_Send(mysock, sendbuffer, sendbuffersize)!=sendbuffersize) {
			isopen=false;
			return false;
		}
	}
}
*/
void TCPClientSocket::FlushBuffer() {
	if(sendbufferindex) {
		if(SDLNet_TCP_Send(mysock, sendbuffer,
			sendbufferindex)!=sendbufferindex) {
			isopen=false;
			return;
		}
		sendbufferindex=0;
	}
}

void TCPClientSocket::SetSendBufferSize(Bitu bufsize) {
	if(sendbuffer) delete [] sendbuffer;
	sendbuffer = new Bit8u[bufsize];
	sendbuffersize=bufsize;
	sendbufferindex=0;
}


TCPServerSocket::TCPServerSocket(Bit16u port)
{
	isopen = false;
	mysock = 0;
	if(!SDLNetInited) {
        if(SDLNet_Init()==-1) {
			LOG_MSG("SDLNet_Init failed: %s\n", SDLNet_GetError());
			return;
		}
		SDLNetInited = true;
	}
	if (port) {
		IPaddress listen_ip;
		SDLNet_ResolveHost(&listen_ip, NULL, port);
		mysock=SDLNet_TCP_Open(&listen_ip);
		if(!mysock) return;
	}
	else return;
	isopen = true;
}

TCPServerSocket::~TCPServerSocket() {
	if(mysock) SDLNet_TCP_Close(mysock);
}

TCPClientSocket* TCPServerSocket::Accept() {

	TCPsocket new_tcpsock;

	new_tcpsock=SDLNet_TCP_Accept(mysock);
	if(!new_tcpsock) {
		//printf("SDLNet_TCP_Accept: %s\n", SDLNet_GetError());
		return 0;
	}
	
	return new TCPClientSocket(new_tcpsock);
}
#endif // #if C_MODEM
