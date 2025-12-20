// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MISC_UTIL_H
#define DOSBOX_MISC_UTIL_H

#include "dosbox.h"

#include <vector>

#include "misc/support.h"

#if defined WIN32
 #define NATIVESOCKETS
 #include <winsock2.h>
 #include <ws2tcpip.h> //for socklen_t
 //typedef int  socklen_t;

//Tests for BSD/LINUX
#elif defined HAVE_STDLIB_H && defined HAVE_SYS_TYPES_H && defined HAVE_SYS_SOCKET_H && defined HAVE_NETINET_IN_H
 #define NATIVESOCKETS
 #define SOCKET int
#include <cstdio>  //darwin
#include <cstdlib> //darwin
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
// socklen_t should be handled by configure
#endif

// Using a non-blocking connection routine really should
// require changes to softmodem to prevent bogus CONNECT
// messages.  By default, we use the old blocking one.
// This is basically how TCP behaves anyway.
//#define ENET_BLOCKING_CONNECT

#include <queue>
#ifndef ENET_BLOCKING_CONNECT
#include <ctime>
#endif

#include <SDL_net.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#include "enet/include/enet.h"
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

enum class SocketType {
	Tcp  = 0, // +SOCK0 modem command
	Enet = 1, // +SOCK1 modem command
	Invalid,  // first invalid value
};
const char* to_string(const SocketType socket_type);

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

	static NETClientSocket* NETClientFactory(const SocketType socketType,
	                                         const char* destination,
	                                         const uint16_t port);

	virtual SocketState GetcharNonBlock(uint8_t &val) = 0;
	virtual bool Putchar(uint8_t val) = 0;
	virtual bool SendArray(const uint8_t *data, size_t n) = 0;
	virtual bool ReceiveArray(uint8_t *data, size_t &n) = 0;
	virtual bool GetRemoteAddressString(char *buffer) = 0;

	void FlushBuffer();
	void SetSendBufferSize(size_t n);
	bool SendByteBuffered(uint8_t val);

	bool isopen = false;

private:
	size_t sendbufferindex = 0;
	std::vector<uint8_t> sendbuffer = {};
};

class NETServerSocket {
public:
	NETServerSocket();
	virtual ~NETServerSocket();

	NETServerSocket(const NETServerSocket &) = delete; // prevent copying
	NETServerSocket &operator=(const NETServerSocket &) = delete; // prevent assignment

	virtual void Close();

	static NETServerSocket* NETServerFactory(const SocketType socketType,
	                                         const uint16_t port);

	virtual NETClientSocket *Accept() = 0;

	bool isopen = false;
};

// --- ENET UDP NET INTERFACE ------------------------------------------------

class ENETServerSocket : public NETServerSocket {
public:
	ENETServerSocket(uint16_t port);
	ENETServerSocket(const ENETServerSocket &) = delete; // prevent copying
	ENETServerSocket &operator=(const ENETServerSocket &) = delete; // prevent assignment

	~ENETServerSocket() override;

	NETClientSocket *Accept() override;

private:
	ENetHost    *host      = nullptr;
	ENetAddress  address   = {};
	bool         nowClient = false;
};

class ENETClientSocket : public NETClientSocket {
public:
	ENETClientSocket(ENetHost *host);
	ENETClientSocket(const char *destination, uint16_t port);
	ENETClientSocket(const ENETClientSocket &) = delete; // prevent copying
	ENETClientSocket &operator=(const ENETClientSocket &) = delete; // prevent assignment

	~ENETClientSocket() override;

	SocketState GetcharNonBlock(uint8_t &val) override;
	bool Putchar(uint8_t val) override;
	bool SendArray(const uint8_t *data, size_t n) override;
	bool ReceiveArray(uint8_t *data, size_t &n) override;
	bool GetRemoteAddressString(char *buffer) override;

private:
	void updateState();

#ifndef ENET_BLOCKING_CONNECT
	int64_t              connectStart  = 0;
	bool                 connecting    = false;
#endif
	ENetHost            *client        = nullptr;
	ENetPeer            *peer          = nullptr;
	ENetAddress          address       = {};
	std::queue<uint8_t>  receiveBuffer = {};
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

	~TCPClientSocket() override;

	SocketState GetcharNonBlock(uint8_t &val) override;
	bool Putchar(uint8_t val) override;
	bool SendArray(const uint8_t *data, size_t n) override;
	bool ReceiveArray(uint8_t *data, size_t &n) override;
	bool GetRemoteAddressString(char *buffer) override;

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

	~TCPServerSocket() override;

	NETClientSocket *Accept() override;
};

#endif
