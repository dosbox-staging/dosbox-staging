// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MISC_UTIL_H
#define DOSBOX_MISC_UTIL_H

#include "dosbox.h"

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "misc/support.h"

#if defined WIN32
#define NATIVESOCKETS
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined HAVE_STDLIB_H && defined HAVE_SYS_TYPES_H && \
        defined HAVE_SYS_SOCKET_H && defined HAVE_NETINET_IN_H
#define NATIVESOCKETS
#define SOCKET int
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>

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
	Tcp  = 0,
	Enet = 1,
	Invalid,
};
const char* to_string(const SocketType socket_type);

bool NetWrapper_InitializeENET();

enum class SocketState {
	Good,
	Empty,
	Closed,
};

// --- GENERIC NET INTERFACE -------------------------------------------------

class NETClientSocket {
public:
	NETClientSocket() = default;
	virtual ~NETClientSocket() = default;

	NETClientSocket(const NETClientSocket&)            = delete;
	NETClientSocket& operator=(const NETClientSocket&) = delete;

	static NETClientSocket* NETClientFactory(const SocketType socketType,
	                                         const char* destination,
	                                         const uint16_t port);

	virtual SocketState GetcharNonBlock(uint8_t& val)     = 0;
	virtual bool Putchar(uint8_t val) = 0;
	virtual bool SendArray(const uint8_t* data, size_t n) = 0;
	virtual bool ReceiveArray(uint8_t* data, size_t& n)   = 0;
	virtual bool GetRemoteAddressString(char* buffer)     = 0;

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
	NETServerSocket() = default;
	virtual ~NETServerSocket() = default;

	NETServerSocket(const NETServerSocket&)            = delete;
	NETServerSocket& operator=(const NETServerSocket&) = delete;

	virtual void Close();

	static NETServerSocket* NETServerFactory(const SocketType socketType,
	                                         const uint16_t port);

	virtual NETClientSocket* Accept() = 0;

	bool isopen = false;
};

// --- ENet UDP NET INTERFACE ------------------------------------------------

class ENETServerSocket : public NETServerSocket {
public:
	ENETServerSocket(uint16_t port);
	ENETServerSocket(const ENETServerSocket&)            = delete;
	ENETServerSocket& operator=(const ENETServerSocket&) = delete;

	~ENETServerSocket() override;
	NETClientSocket* Accept() override;

private:
	ENetHost* host      = nullptr;
	ENetAddress address = {};
	bool nowClient      = false;
};

class ENETClientSocket : public NETClientSocket {
public:
	ENETClientSocket(ENetHost* host);
	ENETClientSocket(const char* destination, uint16_t port);
	ENETClientSocket(const ENETClientSocket&)            = delete;
	ENETClientSocket& operator=(const ENETClientSocket&) = delete;

	~ENETClientSocket() override;

	SocketState GetcharNonBlock(uint8_t& val) override;
	bool Putchar(uint8_t val) override;
	bool SendArray(const uint8_t* data, size_t n) override;
	bool ReceiveArray(uint8_t* data, size_t& n) override;
	bool GetRemoteAddressString(char* buffer) override;

private:
	void updateState();

#ifndef ENET_BLOCKING_CONNECT
	int64_t connectStart = 0;
	bool connecting      = false;
#endif
	ENetHost* client                  = nullptr;
	ENetPeer* peer                    = nullptr;
	ENetAddress address               = {};
	std::queue<uint8_t> receiveBuffer = {};
};

// --- TCP NET INTERFACE (Asio) ---------------------------------------------

class TCPClientSocket : public NETClientSocket {
public:
	TCPClientSocket(asio::ip::tcp::socket&& socket,
	                const std::shared_ptr<asio::io_context>& io_context);
	TCPClientSocket(const char* destination, uint16_t port);
#ifdef NATIVESOCKETS
	TCPClientSocket(int platformsocket);
#endif
	TCPClientSocket(const TCPClientSocket&)            = delete;
	TCPClientSocket& operator=(const TCPClientSocket&) = delete;

	~TCPClientSocket() override;

	SocketState GetcharNonBlock(uint8_t& val) override;
	bool Putchar(uint8_t val) override;
	bool SendArray(const uint8_t* data, size_t n) override;
	bool ReceiveArray(uint8_t* data, size_t& n) override;
	bool GetRemoteAddressString(char* buffer) override;

private:
	std::shared_ptr<asio::io_context> io = {};
	asio::ip::tcp::socket socket;
#ifdef NATIVESOCKETS
	bool is_inherited_socket = false;
#endif
};

class TCPServerSocket : public NETServerSocket {
public:
	TCPServerSocket(uint16_t port);
	TCPServerSocket(const TCPServerSocket&)            = delete;
	TCPServerSocket& operator=(const TCPServerSocket&) = delete;

	~TCPServerSocket() override;
	NETClientSocket* Accept() override;

private:
	std::shared_ptr<asio::io_context> io = {};
	asio::ip::tcp::acceptor acceptor;
};

#endif // DOSBOX_MISC_UTIL_H
