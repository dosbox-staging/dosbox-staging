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

#ifndef DOSBOX_MISC_UTIL_H
#define DOSBOX_MISC_UTIL_H

#include "dosbox.h"

#if C_MODEM

#include "support.h"

#if defined WIN32
 #define NATIVESOCKETS
 #include <winsock2.h>
 #include <ws2tcpip.h> //for socklen_t
 //typedef int  socklen_t;

//Tests for BSD/LINUX
#elif defined HAVE_STDLIB_H && defined HAVE_SYS_TYPES_H && defined HAVE_SYS_SOCKET_H && defined HAVE_NETINET_IN_H
 #define NATIVESOCKETS
 #define SOCKET int
 #include <stdio.h> //darwin
 #include <stdlib.h> //darwin
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 //socklen_t should be handled by configure
#endif

#include <queue>
#include <SDL_net.h>

#include "enet.h"

enum SocketTypesE { SOCKET_TYPE_TCP = 0, SOCKET_TYPE_ENET, SOCKET_TYPE_COUNT };

// helper functions
bool NetWrapper_InitializeSDLNet();
bool NetWrapper_InitializeENET();

enum class SocketState {
	Good,  // had data and socket is open
	Empty, // didn't have data but socket is open
	Closed // didn't have data and socket is closed
};

// --- GENERIC NET INTERFACE -------------------------------------------------

class NETClientSocket {
public:
	NETClientSocket();
	virtual ~NETClientSocket();

	NETClientSocket(const NETClientSocket &) = delete; // prevent copying
	NETClientSocket &operator=(const NETClientSocket &) = delete; // prevent assignment

	static NETClientSocket *NETClientFactory(SocketTypesE socketType,
	                                         const char *destination,
	                                         uint16_t port);

	virtual SocketState GetcharNonBlock(uint8_t &val) = 0;
	virtual bool Putchar(uint8_t val) = 0;
	virtual bool SendArray(uint8_t *data, size_t n) = 0;
	virtual bool ReceiveArray(uint8_t *data, size_t &n) = 0;
	virtual bool GetRemoteAddressString(uint8_t *buffer) = 0;

	void FlushBuffer();
	void SetSendBufferSize(size_t n);
	bool SendByteBuffered(uint8_t val);

	bool isopen = false;

private:
	size_t sendbuffersize = 0;
	size_t sendbufferindex = 0;
	uint8_t *sendbuffer = nullptr;
};

class NETServerSocket {
public:
	NETServerSocket();
	virtual ~NETServerSocket();

	NETServerSocket(const NETServerSocket &) = delete; // prevent copying
	NETServerSocket &operator=(const NETServerSocket &) = delete; // prevent assignment

	static NETServerSocket *NETServerFactory(SocketTypesE socketType,
	                                         uint16_t port);

	virtual NETClientSocket *Accept() = 0;

	bool isopen = false;
};

// --- ENET UDP NET INTERFACE ------------------------------------------------

class ENETServerSocket : public NETServerSocket {
public:
	ENETServerSocket(uint16_t port);
	ENETServerSocket(const ENETServerSocket &) = delete; // prevent copying
	ENETServerSocket &operator=(const ENETServerSocket &) = delete; // prevent assignment

	~ENETServerSocket();

	NETClientSocket *Accept();

	bool nowClient = false;
	ENetHost *host = NULL;
	ENetAddress address;
};

class ENETClientSocket : public NETClientSocket {
public:
	ENETClientSocket(const char *destination, uint16_t port);
	ENETClientSocket(ENetHost *host);
	~ENETClientSocket();

	SocketState GetcharNonBlock(uint8_t &val);
	bool Putchar(uint8_t val);
	bool SendArray(uint8_t *data, size_t n);
	bool ReceiveArray(uint8_t *data, size_t &n);
	bool GetRemoteAddressString(uint8_t *buffer);

private:
	void updateState();

	ENetHost *client = NULL;
	ENetPeer *peer = NULL;
	ENetAddress address;
	std::queue<uint8_t> receiveBuffer;
};

// --- TCP NET INTERFACE -----------------------------------------------------

struct _TCPsocketX {
	int ready = 0;
#ifdef NATIVESOCKETS
	SOCKET channel = 0;
#endif
	IPaddress remoteAddress = {0, 0};
	IPaddress localAddress = {0, 0};
	int sflag = 0;
};

class TCPClientSocket : public NETClientSocket {
public:
	TCPClientSocket(TCPsocket source);
	TCPClientSocket(const char *destination, uint16_t port);
#ifdef NATIVESOCKETS
	TCPClientSocket(int platformsocket);
#endif
	TCPClientSocket(const TCPClientSocket&) = delete; // prevent copying
	TCPClientSocket& operator=(const TCPClientSocket&) = delete; // prevent assignment

	~TCPClientSocket();

	SocketState GetcharNonBlock(uint8_t &val);
	bool Putchar(uint8_t val);
	bool SendArray(uint8_t *data, size_t n);
	bool ReceiveArray(uint8_t *data, size_t &n);
	bool GetRemoteAddressString(uint8_t *buffer);

private:

#ifdef NATIVESOCKETS
	_TCPsocketX *nativetcpstruct = nullptr;
#endif

	TCPsocket mysock = nullptr;
	SDLNet_SocketSet listensocketset = nullptr;
};

class TCPServerSocket : public NETServerSocket {
public:
	TCPsocket mysock = nullptr;

	TCPServerSocket(uint16_t port);
	TCPServerSocket(const TCPServerSocket&) = delete; // prevent copying
	TCPServerSocket& operator=(const TCPServerSocket&) = delete; // prevent assignment

	~TCPServerSocket();

	NETClientSocket *Accept();
};

#endif // C_MODEM

#endif
