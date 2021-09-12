/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if C_MODEM

#include "misc_util.h"

#include <cassert>

uint32_t Netwrapper_GetCapabilities()
{
	uint32_t retval = 0;
	retval = CAPWORD;
	return retval;
}

class sdl_net_manager_t {
public:
	sdl_net_manager_t()
	{
		if (already_tried_once)
			return;
		already_tried_once = true;

		is_initialized = SDLNet_Init() != -1;
		if (is_initialized)
			LOG_INFO("NET: Initialized SDL network subsystem");
		else
			LOG_WARNING("NET: failed to initialize SDL network subsystem: %s\n",
			            SDLNet_GetError());
	}

	~sdl_net_manager_t()
	{
		if (!is_initialized)
			return;

		assert(already_tried_once);
		SDLNet_Quit();
		LOG_INFO("NET: Shutdown SDL network subsystem");
	}

	bool IsInitialized() const { return is_initialized; }

private:
	bool already_tried_once = false;
	bool is_initialized = false;
};

bool NetWrapper_InitializeSDLNet()
{
	static sdl_net_manager_t sdl_net_manager;
	return sdl_net_manager.IsInitialized();
}

#ifdef NATIVESOCKETS
TCPClientSocket::TCPClientSocket(int platformsocket)
{
	if (!NetWrapper_InitializeSDLNet())
		return;

	nativetcpstruct = new _TCPsocketX;
	mysock = (TCPsocket)nativetcpstruct;

	// fill the SDL socket manually
	nativetcpstruct->ready = 0;
	nativetcpstruct->sflag = 0;
	nativetcpstruct->channel = platformsocket;
	sockaddr_in		sa;
	socklen_t		sz;
	sz=sizeof(sa);
	if(getpeername(platformsocket, (sockaddr *)(&sa), &sz)==0) {
		nativetcpstruct->remoteAddress.host = /*ntohl(*/sa.sin_addr.s_addr;//);
		nativetcpstruct->remoteAddress.port = /*ntohs(*/sa.sin_port;//);
	}
	else {
		mysock = nullptr;
		return;
	}
	sz=sizeof(sa);
	if(getsockname(platformsocket, (sockaddr *)(&sa), &sz)==0) {
		(nativetcpstruct)->localAddress.host = /*ntohl(*/ sa.sin_addr.s_addr; //);
		(nativetcpstruct)->localAddress.port = /*ntohs(*/ sa.sin_port; //);
	}
	else {
		mysock = nullptr;
		return;
	}
	if(mysock) {
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		SDLNet_TCP_AddSocket(listensocketset, mysock);
		isopen=true;
		return;
	}
	return;
}
#endif // NATIVESOCKETS

TCPClientSocket::TCPClientSocket(TCPsocket source)
{
	if (!NetWrapper_InitializeSDLNet())
		return;

	if(source!=0) {
		mysock = source;
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		SDLNet_TCP_AddSocket(listensocketset, source);

		isopen=true;
	}
}

TCPClientSocket::TCPClientSocket(const char *destination, uint16_t port)
{
	if (!NetWrapper_InitializeSDLNet())
		return;

	IPaddress openip;
	//Ancient versions of SDL_net had this as char*. People still appear to be using this one.
	if (!SDLNet_ResolveHost(&openip,const_cast<char*>(destination),port)) {
		listensocketset = SDLNet_AllocSocketSet(1);
		if(!listensocketset) return;
		mysock = SDLNet_TCP_Open(&openip);
		if (!mysock)
			return;
		SDLNet_TCP_AddSocket(listensocketset, mysock);
		isopen=true;
	}
}

TCPClientSocket::~TCPClientSocket()
{
	delete [] sendbuffer;
#ifdef NATIVESOCKETS
	delete nativetcpstruct;
#endif
	if(mysock) {
		if(listensocketset)
			SDLNet_TCP_DelSocket(listensocketset, mysock);
		SDLNet_TCP_Close(mysock);
	}

	if(listensocketset) SDLNet_FreeSocketSet(listensocketset);
}

bool TCPClientSocket::GetRemoteAddressString(uint8_t *buffer)
{
	IPaddress *remote_ip;
	uint8_t b1, b2, b3, b4;
	remote_ip=SDLNet_TCP_GetPeerAddress(mysock);
	if(!remote_ip) return false;
	b4=remote_ip->host>>24;
	b3=(remote_ip->host>>16)&0xff;
	b2=(remote_ip->host>>8)&0xff;
	b1=remote_ip->host&0xff;
	sprintf((char*)buffer,"%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
	        b1, b2, b3, b4);
	return true;
}

bool TCPClientSocket::ReceiveArray(uint8_t *data, size_t &n)
{
	assertm(n <= static_cast<size_t>(std::numeric_limits<int>::max()),
	        "SDL_net can't handle more bytes at a time.");
	assert(data);
	if (SDLNet_CheckSockets(listensocketset, 0)) {
		const int result = SDLNet_TCP_Recv(mysock, data, static_cast<int>(n));
		if(result < 1) {
			isopen = false;
			n = 0;
			return false;
		} else {
			n = result;
			return true;
		}
	} else {
		n = 0;
		return true;
	}
}

SocketState TCPClientSocket::GetcharNonBlock(uint8_t &val)
{
	SocketState state = SocketState::Empty;
	if(SDLNet_CheckSockets(listensocketset,0))
	{
		if (SDLNet_TCP_Recv(mysock, &val, 1) == 1)
			state = SocketState::Good;
		else {
			isopen = false;
			state = SocketState::Closed;
		}
	}
	return state;
}

bool TCPClientSocket::Putchar(uint8_t val)
{
	return SendArray(&val, 1);
}

bool TCPClientSocket::SendArray(uint8_t *data, const size_t n)
{
	assertm(n <= static_cast<size_t>(std::numeric_limits<int>::max()),
	        "SDL_net can't handle more bytes at a time.");
	assert(data);
	if (SDLNet_TCP_Send(mysock, data, static_cast<int>(n))
	    != static_cast<int>(n)) {
		isopen = false;
		return false;
	}
	return true;
}

bool TCPClientSocket::SendByteBuffered(const uint8_t val)
{
	if (sendbuffersize == 0)
		return false;

	if (sendbufferindex < (sendbuffersize - 1)) {
		sendbuffer[sendbufferindex] = val;
		sendbufferindex++;
		return true;
	}
	// buffer is full, get rid of it
	sendbuffer[sendbufferindex] = val;
	sendbufferindex = 0;
	return SendArray(sendbuffer, sendbuffersize);
}

void TCPClientSocket::FlushBuffer()
{
	if (sendbufferindex) {
		if (!SendArray(sendbuffer, sendbufferindex))
			return;
		sendbufferindex = 0;
	}
}

void TCPClientSocket::SetSendBufferSize(const size_t n)
{
	// Only resize the buffer if needed
	if (!sendbuffer || sendbuffersize != n) {
		delete [] sendbuffer;
		sendbuffer = new uint8_t[n];
		sendbuffersize = n;
	}
	sendbufferindex = 0;
}

TCPServerSocket::TCPServerSocket(const uint16_t port)
{
	isopen = false;
	mysock = nullptr;

	if (!NetWrapper_InitializeSDLNet())
		return;

	if (port) {
		IPaddress listen_ip;
		SDLNet_ResolveHost(&listen_ip, NULL, port);
		mysock = SDLNet_TCP_Open(&listen_ip);
		if (!mysock)
			return;
	} else {
		return;
	}
	isopen = true;
}

TCPServerSocket::~TCPServerSocket()
{
	if (mysock)
		SDLNet_TCP_Close(mysock);
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

#endif // C_MODEM
