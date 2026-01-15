// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_HARDWARE_NETWORK_NET_DEFS_H
#define DOSBOX_HARDWARE_NETWORK_NET_DEFS_H

#include <cstdint>
#include <cstring>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

static inline uint16_t net_to_host16(const uint16_t net_value)
{
	return ntohs(net_value);
}

static inline uint32_t net_to_host32(const uint32_t net_value)
{
	return ntohl(net_value);
}

static inline uint16_t host_to_net16(const uint16_t host_value)
{
	return htons(host_value);
}
static inline uint32_t host_to_net32(const uint32_t host_value)
{
	return htonl(host_value);
}

// A minimal replacement for SDL_net's IPaddress.
//
// Semantics: the underlying bytes are stored in network order (big-endian).
// On little-endian hosts, this means the numeric values appear byte-swapped,
// matching the historic SDL_net behavior that the current code expects.
#pragma pack(push, 1)
struct IPaddress {
	uint32_t host = 0; // network-order bytes
	uint16_t port = 0; // network-order bytes
	IPaddress(const uint32_t host, const uint16_t port)
	        : host(host_to_net32(host)),
	          port(host_to_net16(port))
	{}
	IPaddress() = default;
};
#pragma pack(pop)

static_assert(sizeof(IPaddress) == 6, "IPaddress must match legacy layout");

// Big-endian read/write helpers (replacement for SDLNet_Read/WriteXX).
// Safe for unaligned pointers.
static inline uint16_t net_read16(const void* p)
{
	uint16_t tmp = 0;
	std::memcpy(&tmp, p, sizeof(tmp));
	return ntohs(tmp);
}

static inline uint32_t net_read32(const void* p)
{
	uint32_t tmp = 0;
	std::memcpy(&tmp, p, sizeof(tmp));
	return ntohl(tmp);
}

static inline void net_write16(const uint16_t value, void* p)
{
	const uint16_t tmp = htons(value);
	std::memcpy(p, &tmp, sizeof(tmp));
}

static inline void net_write32(const uint32_t value, void* p)
{
	const uint32_t tmp = htonl(value);
	std::memcpy(p, &tmp, sizeof(tmp));
}

#endif // DOSBOX_HARDWARE_NETWORK_NET_DEFS_H
