// SPDX-FileCopyrightText:  2002-2025 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_HARDWARE_NETWORK_NET_DEFS_H
#define DOSBOX_HARDWARE_NETWORK_NET_DEFS_H

#include <cstdint>
#include <cstring>

// A minimal replacement for SDL_net's IPaddress.
//
// Semantics: the underlying bytes are stored in network order (big-endian).
// On little-endian hosts, this means the numeric values appear byte-swapped,
// matching the historic SDL_net behavior that the current code expects.
#pragma pack(push, 1)
struct IPaddress {
	uint32_t host = 0; // network-order bytes
	uint16_t port = 0; // network-order bytes
};
#pragma pack(pop)

static_assert(sizeof(IPaddress) == 6, "IPaddress must match legacy layout");

// Big-endian read/write helpers (replacement for SDLNet_Read/WriteXX).
inline uint16_t Net_Read16(const void* p)
{
	const auto* b = static_cast<const uint8_t*>(p);
	return static_cast<uint16_t>((static_cast<uint16_t>(b[0]) << 8) | b[1]);
}

inline uint32_t Net_Read32(const void* p)
{
	const auto* b = static_cast<const uint8_t*>(p);
	return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
	       (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}

inline void Net_Write16(const uint16_t value, void* p)
{
	auto* b = static_cast<uint8_t*>(p);
	b[0] = static_cast<uint8_t>(value >> 8);
	b[1] = static_cast<uint8_t>(value & 0xff);
}

inline void Net_Write32(const uint32_t value, void* p)
{
	auto* b = static_cast<uint8_t*>(p);
	b[0] = static_cast<uint8_t>(value >> 24);
	b[1] = static_cast<uint8_t>((value >> 16) & 0xff);
	b[2] = static_cast<uint8_t>((value >> 8) & 0xff);
	b[3] = static_cast<uint8_t>(value & 0xff);
}

inline uint16_t Net_PortToHost(const uint16_t net_port)
{
	return Net_Read16(&net_port);
}

inline void Net_SetPort(IPaddress& addr, const uint16_t host_port)
{
	Net_Write16(host_port, &addr.port);
}

#endif // DOSBOX_HARDWARE_NETWORK_NET_DEFS_H
