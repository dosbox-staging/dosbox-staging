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

#define ENET_IMPLEMENTATION
#include "../../libs/enet/include/enet.h" // Must be included before misc_util.h

#include "misc_util.h"

#include <cassert>

// --- GENERIC NET INTERFACE -------------------------------------------------

NETClientSocket::NETClientSocket()
{}

NETClientSocket::~NETClientSocket()
{
	delete[] sendbuffer;
}

NETClientSocket *NETClientSocket::NETClientFactory(SocketTypesE socketType,
                                                   const char *destination,
                                                   uint16_t port)
{
	switch (socketType) {
	case SOCKET_TYPE_TCP: return new TCPClientSocket(destination, port);

	case SOCKET_TYPE_ENET: return new ENETClientSocket(destination, port);

	default: return NULL;
	}
	return NULL;
}

void NETClientSocket::FlushBuffer()
{
	if (sendbufferindex) {
		if (!SendArray(sendbuffer, sendbufferindex))
			return;
		sendbufferindex = 0;
	}
}

void NETClientSocket::SetSendBufferSize(size_t n)
{
	// Only resize the buffer if needed
	if (!sendbuffer || sendbuffersize != n) {
		delete[] sendbuffer;
		sendbuffer = new uint8_t[n];
		sendbuffersize = n;
	}
	sendbufferindex = 0;
}

bool NETClientSocket::SendByteBuffered(uint8_t val)
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

NETServerSocket::NETServerSocket()
{}

NETServerSocket::~NETServerSocket()
{}

NETServerSocket *NETServerSocket::NETServerFactory(SocketTypesE socketType,
                                                   uint16_t port)
{
	switch (socketType) {
	case SOCKET_TYPE_TCP: return new TCPServerSocket(port);

	case SOCKET_TYPE_ENET: return new ENETServerSocket(port);

	default: return NULL;
	}
	return NULL;
}

// --- ENET UDP NET INTERFACE ------------------------------------------------

class enet_manager_t {
public:
	enet_manager_t()
	{
		if (already_tried_once)
			return;
		already_tried_once = true;

		is_initialized = enet_initialize() == 0;
		if (is_initialized)
			LOG_INFO("NET: Initialized ENET network subsystem");
		else
			LOG_WARNING("NET: failed to initialize ENET network subsystem\n");
	}

	~enet_manager_t()
	{
		if (!is_initialized)
			return;

		assert(already_tried_once);
		enet_deinitialize();
		LOG_INFO("NET: Shutdown ENET network subsystem");
	}

	bool IsInitialized() const { return is_initialized; }

private:
	bool already_tried_once = false;
	bool is_initialized = false;
};

bool NetWrapper_InitializeENET()
{
	static enet_manager_t enet_manager;
	return enet_manager.IsInitialized();
}

ENETServerSocket::ENETServerSocket(uint16_t port)
{
	if (!NetWrapper_InitializeENET())
		return;

	address.host = ENET_HOST_ANY;
	address.port = port;

	host = enet_host_create(&address, // create a host
	                        1, // only allow 1 client to connect
	                        1, // allow 1 channel to be used, 0
	                        0, // assume any amount of incoming bandwidth
	                        0  // assume any amount of outgoing bandwidth
	);
	if (host == NULL) {
		LOG_INFO("Unable to create server ENET listening socket");
		return;
	}

	isopen = true;
}

ENETServerSocket::~ENETServerSocket()
{
	// We don't destroy 'host' after passing it to a client, it needs to live.
	if (host && !nowClient) {
		enet_host_destroy(host);
		LOG_INFO("Closed server ENET listening socket");
	}
	isopen = false;
}

NETClientSocket *ENETServerSocket::Accept()
{
	ENetEvent event;
	while (enet_host_service(host, &event, 0) > 0) {
		switch (event.type) {
		case ENET_EVENT_TYPE_CONNECT:
			LOG_INFO("NET:  ENET client connect");
			nowClient = true;
			return new ENETClientSocket(host);
			break;

		case ENET_EVENT_TYPE_RECEIVE:
			enet_packet_destroy(event.packet);
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: isopen = false; break;

		default: break;
		}
	}

	return NULL;
}

ENETClientSocket::ENETClientSocket(const char *destination, uint16_t port)
{
	if (!NetWrapper_InitializeENET())
		return;

	client = enet_host_create(NULL, // create a client host
	                          1, // only allow 1 outgoing connection
	                          1, // allow 1 channel to be used, 0
	                          0, // assume any amount of incoming bandwidth
	                          0  // assume any amount of outgoing bandwidth
	);
	if (client == NULL) {
		LOG_INFO("Unable to create client ENET socket");
		return;
	}

	enet_address_set_host(&address, destination);
	address.port = port;
	peer = enet_host_connect(client, &address, 1, 0);
	if (peer == NULL) {
		enet_host_destroy(client);
		LOG_INFO("Unable to create client ENET peer");
		return;
	}

#ifndef ENET_BLOCKING_CONNECT
	// Start connection timeout clock.
	connectStart = std::clock();
	connecting   = true;
#else
	ENetEvent event;
	// Wait up to 5 seconds for the connection attempt to succeed.
	if (enet_host_service(client, &event, 5000) > 0 &&
	    event.type == ENET_EVENT_TYPE_CONNECT) {
		LOG_INFO("NET:  ENET connect");
	} else {
		LOG_INFO("NET:  ENET connected failed");
		enet_peer_reset(peer);
		enet_host_destroy(client);
		return;
	}
#endif

	isopen = true;
}

ENETClientSocket::ENETClientSocket(ENetHost *host)
{
	client  = host;
	address = client->address;
	peer    = &client->peers[0];
	isopen  = true;
	LOG_INFO("ENETClientSocket created from server socket");
}

ENETClientSocket::~ENETClientSocket()
{
	if (isopen) {
		enet_peer_reset(peer);
		enet_host_destroy(client);
		isopen = false;
		LOG_INFO("Closed client ENET listening socket");
	}
}

SocketState ENETClientSocket::GetcharNonBlock(uint8_t &val)
{
	updateState();

	if (!receiveBuffer.empty()) {
		val = receiveBuffer.front();
		receiveBuffer.pop();
		return SocketState::Good;
	}

	return SocketState::Empty;
}

bool ENETClientSocket::Putchar(uint8_t val)
{
	ENetPacket *packet;

	updateState();
	packet = enet_packet_create(&val, 1, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(peer, 0, packet);
	updateState();

	return isopen;
}

bool ENETClientSocket::SendArray(uint8_t *data, size_t n)
{
	ENetPacket *packet = NULL;

	updateState();
	packet = enet_packet_create(data, n, ENET_PACKET_FLAG_RELIABLE);
	if (packet) {
		enet_peer_send(peer, 0, packet);
	} else {
		LOG_INFO("ENETClientSocket::SendArray unable to create packet size %ld", n);
	}
	updateState();

	return isopen;
}

bool ENETClientSocket::ReceiveArray(uint8_t *data, size_t &n)
{
	size_t x = 0;

	// Prime the pump.
	updateState();

#if 0
	// This block is how the SDLNet documentation says the TCP code should behave.

	// The SDL TCP code doesn't wait if there is no pending data.
	if (receiveBuffer.empty()) {
	        n = 0;
	        return isopen;
	}

	// SDLNet_TCP_Recv says it blocks until it receives "n" bytes.
	// Based on the softmodem code, I'm not sure this is true.
	while (isopen && x < n) {
	        if (!receiveBuffer.empty()) {
	                data[x++] = receiveBuffer.front();
	                receiveBuffer.pop();
	        }
	        updateState();
	}
#else
	// This block is how softmodem.cpp seems to expect the code to behave.
	// Needless to say, not following the docs seems to work better.  :-)

	while (isopen && x < n && !receiveBuffer.empty()) {
		data[x++] = receiveBuffer.front();
		receiveBuffer.pop();
		updateState();
	}

	n = x;
#endif

	return isopen;
}

bool ENETClientSocket::GetRemoteAddressString(uint8_t *buffer)
{
	updateState();
	enet_address_get_host_ip(&address, (char *)buffer, 16);
	return true;
}

void ENETClientSocket::updateState()
{
	ENetEvent event;

	while (enet_host_service(client, &event, 0) > 0) {
		switch (event.type) {
#ifndef ENET_BLOCKING_CONNECT
		case ENET_EVENT_TYPE_CONNECT:
			connecting = false;
			LOG_INFO("NET:  ENET connect");
			break;
#endif
		case ENET_EVENT_TYPE_RECEIVE:
			for (size_t x = 0; x < event.packet->dataLength; x++) {
				receiveBuffer.push(event.packet->data[x]);
			}
			enet_packet_destroy(event.packet);
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
		case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: isopen = false; break;

		default: break;
		}
	}

#ifndef ENET_BLOCKING_CONNECT
	if (connecting) {
		// Check for timeout.  Five seconds.
		if (((std::clock() - connectStart) / (double)CLOCKS_PER_SEC) > 5) {
			LOG_INFO("NET:  ENET connected failed");
			enet_peer_reset(peer);
			enet_host_destroy(client);
			connecting = false;
			isopen     = false;
		}
	}
#endif
}

// --- TCP NET INTERFACE -----------------------------------------------------

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
#ifdef NATIVESOCKETS
	delete nativetcpstruct;
#endif
	if(mysock) {
		if(listensocketset)
			SDLNet_TCP_DelSocket(listensocketset, mysock);
		SDLNet_TCP_Close(mysock);
		LOG_INFO("Closed client TCP listening socket");
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
	if (mysock) {
		SDLNet_TCP_Close(mysock);
		LOG_INFO("Closed server TCP listening socket");
	}
}

NETClientSocket *TCPServerSocket::Accept()
{
	TCPsocket new_tcpsock;

	new_tcpsock=SDLNet_TCP_Accept(mysock);
	if(!new_tcpsock) {
		//printf("SDLNet_TCP_Accept: %s\n", SDLNet_GetError());
		return 0;
	}
	
	return new TCPClientSocket(new_tcpsock);
}

#endif // C_MODEM
