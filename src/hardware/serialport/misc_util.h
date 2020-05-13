/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

// Netwrapper Capabilities
#define NETWRAPPER_TCP 1
#define NETWRAPPER_TCP_NATIVESOCKET 2

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

#ifdef NATIVESOCKETS
 #define CAPWORD (NETWRAPPER_TCP|NETWRAPPER_TCP_NATIVESOCKET)
#else
 #define CAPWORD NETWRAPPER_TCP
#endif

#include <SDL_net.h>

uint32_t Netwrapper_GetCapabilities();

struct _TCPsocketX {
	int ready = 0;
#ifdef NATIVESOCKETS
	SOCKET channel = 0;
#endif
	IPaddress remoteAddress = {0, 0};
	IPaddress localAddress = {0, 0};
	int sflag = 0;
};

enum class SocketState {
	Good, // had data and socket is open
	Empty, // didn't have data but socket is open
	Closed // didn't have data and socket is closed
};

class TCPClientSocket {
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

	bool isopen = false;

	bool GetRemoteAddressString(uint8_t *buffer);

	void FlushBuffer();
	void SetSendBufferSize(size_t n);

	// buffered send functions
	bool SendByteBuffered(uint8_t val);

private:

#ifdef NATIVESOCKETS
	_TCPsocketX *nativetcpstruct = nullptr;
#endif

	TCPsocket mysock = nullptr;
	SDLNet_SocketSet listensocketset = nullptr;

	// Items for send buffering
	size_t sendbuffersize = 0;
	size_t sendbufferindex = 0;
	uint8_t *sendbuffer = nullptr;
};

struct TCPServerSocket {
	bool isopen = false;
	TCPsocket mysock = nullptr;

	TCPServerSocket(uint16_t port);
	TCPServerSocket(const TCPServerSocket&) = delete; // prevent copying
	TCPServerSocket& operator=(const TCPServerSocket&) = delete; // prevent assignment

	~TCPServerSocket();

	TCPClientSocket* Accept();
};

#endif // C_MODEM

#endif
