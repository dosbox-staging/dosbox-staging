// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IPXSERVER_H_
#define DOSBOX_IPXSERVER_H_

#if C_IPX

#include <SDL_net.h>

struct packetBuffer {
	uint8_t buffer[1024];
	int16_t packetSize;  // Packet size remaining in read
	int16_t packetRead;  // Bytes read of total packet
	bool inPacket;      // In packet reception flag
	bool connected;		// Connected flag
	bool waitsize;
};

#define SOCKETTABLESIZE 16
#define CONVIP(hostvar) hostvar & 0xff, (hostvar >> 8) & 0xff, (hostvar >> 16) & 0xff, (hostvar >> 24) & 0xff
#define CONVIPX(hostvar) hostvar[0], hostvar[1], hostvar[2], hostvar[3], hostvar[4], hostvar[5]


void IPX_StopServer();
bool IPX_StartServer(uint16_t portnum);
bool IPX_isConnectedToServer(Bits tableNum, IPaddress ** ptrAddr);

uint8_t packetCRC(uint8_t *buffer, uint16_t bufSize);

#endif

#endif
