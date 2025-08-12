// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#if C_IPX

#include "ipx.h"

#include <atomic>
#include <thread>

#include "hardware/ipxserver.h"
#include "hardware/timer.h"

static constexpr int UDP_UNICAST = -1; // SDLNet magic number

static IPaddress ipxServerIp;     // IPAddress for server's listening port
static UDPsocket ipxServerSocket; // Listening server socket
static SDLNet_SocketSet socket_set = nullptr;

static packetBuffer connBuffer[SOCKETTABLESIZE];

static uint8_t inBuffer[IPXBUFFERSIZE];
static IPaddress ipconn[SOCKETTABLESIZE]; // Active TCP/IP connection

static std::thread ipx_server_thread;
static std::atomic_bool ipx_server_running = false;

uint8_t packetCRC(uint8_t* buffer, uint16_t bufSize)
{
	uint8_t tmpCRC = 0;
	uint16_t i;
	for(i=0;i<bufSize;i++) {
		tmpCRC ^= *buffer;
		buffer++;
	}
	return tmpCRC;
}

/*
static void closeSocket(uint16_t sockidx) {
	uint32_t host;

	host = ipconn[sockidx].host;
	LOG_MSG("IPXSERVER: %d.%d.%d.%d disconnected", CONVIP(host));

	SDLNet_TCP_DelSocket(serverSocketSet,tcpconn[sockidx]);
	SDLNet_TCP_Close(tcpconn[sockidx]);
	connBuffer[sockidx].connected = false;
	connBuffer[sockidx].waitsize = false;
}
*/

static void sendIPXPacket(uint8_t *buffer, int16_t bufSize) {
	uint16_t srcport, destport;
	uint32_t srchost, desthost;
	UDPpacket outPacket;
	outPacket.channel = -1;
	outPacket.data = buffer;
	outPacket.len = bufSize;
	outPacket.maxlen = bufSize;
	IPXHeader *tmpHeader;
	tmpHeader = (IPXHeader *)buffer;

	srchost = tmpHeader->src.addr.byIP.host;
	desthost = tmpHeader->dest.addr.byIP.host;

	srcport = tmpHeader->src.addr.byIP.port;
	destport = tmpHeader->dest.addr.byIP.port;

	if(desthost == 0xffffffff) {
		// Broadcast
		for (uint16_t i = 0; i < SOCKETTABLESIZE; ++i) {
			if(connBuffer[i].connected && ((ipconn[i].host != srchost)||(ipconn[i].port!=srcport))) {
				outPacket.address = ipconn[i];
				const int result = SDLNet_UDP_Send(ipxServerSocket,
				                                   UDP_UNICAST,
				                                   &outPacket);
				if (result == 0) {
					LOG_MSG("IPXSERVER: %s", SDLNet_GetError());
					continue;
				}
				//LOG_MSG("IPXSERVER: Packet of %d bytes sent from %d.%d.%d.%d to %d.%d.%d.%d (BROADCAST) (%x CRC)", bufSize, CONVIP(srchost), CONVIP(ipconn[i].host), packetCRC(&buffer[30], bufSize-30));
			}
		}
	} else {
		// Specific address
		for (uint16_t i = 0; i < SOCKETTABLESIZE; ++i) {
			if((connBuffer[i].connected) && (ipconn[i].host == desthost) && (ipconn[i].port == destport)) {
				outPacket.address = ipconn[i];
				const int result = SDLNet_UDP_Send(ipxServerSocket,
				                                   UDP_UNICAST,
				                                   &outPacket);
				if (result == 0) {
					LOG_MSG("IPXSERVER: %s", SDLNet_GetError());
					continue;
				}
				//LOG_MSG("IPXSERVER: Packet sent from %d.%d.%d.%d to %d.%d.%d.%d", CONVIP(srchost), CONVIP(desthost));
			}
		}
	}
}

bool IPX_isConnectedToServer(Bits tableNum, IPaddress ** ptrAddr) {
	if(tableNum >= SOCKETTABLESIZE) return false;
	*ptrAddr = &ipconn[tableNum];
	return connBuffer[tableNum].connected;
}

static void ackClient(IPaddress clientAddr) {
	IPXHeader regHeader = {};
	UDPpacket regPacket;

	SDLNet_Write16(0xffff, regHeader.checkSum);
	SDLNet_Write16(sizeof(regHeader), regHeader.length);

	SDLNet_Write32(0, regHeader.dest.network);
	PackIP(clientAddr, &regHeader.dest.addr.byIP);
	SDLNet_Write16(0x2, regHeader.dest.socket);

	SDLNet_Write32(1, regHeader.src.network);
	PackIP(ipxServerIp, &regHeader.src.addr.byIP);
	SDLNet_Write16(0x2, regHeader.src.socket);
	regHeader.transControl = 0;

	regPacket.data = (Uint8 *)&regHeader;
	regPacket.len = sizeof(regHeader);
	regPacket.maxlen = sizeof(regHeader);
	regPacket.address = clientAddr;
	// Send registration string to client.  If client doesn't get this, client will not be registered
	const int result = SDLNet_UDP_Send(ipxServerSocket, UDP_UNICAST, &regPacket);
	if (result == 0)
		LOG_MSG("IPXSERVER: Connection response not sent: %s",
		        SDLNet_GetError());
}

static void IPX_ServerLoop() {
	UDPpacket inPacket;
	IPaddress tmpAddr;

	//char regString[] = "IPX Register\0";

	uint32_t host;

	inPacket.channel = -1;
	inPacket.data = &inBuffer[0];
	inPacket.maxlen = IPXBUFFERSIZE;

	const int result = SDLNet_UDP_Recv(ipxServerSocket, &inPacket);
	if (result != 0) {
		// Check to see if incoming packet is a registration packet
		// For this, I just spoofed the echo protocol packet designation 0x02
		IPXHeader *tmpHeader;
		tmpHeader = (IPXHeader *)&inBuffer[0];

		// Check to see if echo packet
		if(SDLNet_Read16(tmpHeader->dest.socket) == 0x2) {
			// Null destination node means its a server registration packet
			if(tmpHeader->dest.addr.byIP.host == 0x0) {
				UnpackIP(tmpHeader->src.addr.byIP, &tmpAddr);
				for (uint16_t i = 0; i < SOCKETTABLESIZE; ++i) {
					if(!connBuffer[i].connected) {
						// Use prefered host IP rather than the reported source IP
						// It may be better to use the reported source
						ipconn[i] = inPacket.address;

						connBuffer[i].connected = true;
						host = ipconn[i].host;
						LOG_MSG("IPXSERVER: Connect from %d.%d.%d.%d", CONVIP(host));
						ackClient(inPacket.address);
						return;
					} else {
						if((ipconn[i].host == tmpAddr.host) && (ipconn[i].port == tmpAddr.port)) {

							LOG_MSG("IPXSERVER: Reconnect from %d.%d.%d.%d", CONVIP(tmpAddr.host));
							// Update anonymous port number if changed
							ipconn[i].port = inPacket.address.port;
							ackClient(inPacket.address);
							return;
						}
					}
				}
			}
		}

		// IPX packet is complete.  Now interpret IPX header and send to respective IP address
		sendIPXPacket((uint8_t*)inPacket.data,
		              static_cast<int16_t>(inPacket.len));
	}
}

void IPX_StopServer() {
	ipx_server_running = false;

	if (ipx_server_thread.joinable()) {
		ipx_server_thread.join();
	}

	SDLNet_FreeSocketSet(socket_set);
	SDLNet_UDP_Close(ipxServerSocket);
	socket_set = nullptr;
}

bool IPX_StartServer(uint16_t portnum)
{
	if (!SDLNet_ResolveHost(&ipxServerIp, nullptr, portnum)) {
		//serverSocketSet = SDLNet_AllocSocketSet(SOCKETTABLESIZE);
		ipxServerSocket = SDLNet_UDP_Open(portnum);
		if(!ipxServerSocket) return false;

		for (auto& i : connBuffer) {
			i.connected = false;
		}

		if (!socket_set) {
			socket_set = SDLNet_AllocSocketSet(1);
			if (!socket_set) {
				LOG_ERR("IPXSERVER: %s", SDLNet_GetError());
				return false;
			}
			if (SDLNet_UDP_AddSocket(socket_set, ipxServerSocket) == -1) {
				LOG_ERR("IPXSERVER: %s", SDLNet_GetError());
				return false;
			}
		}

		if (!ipx_server_running) {
			ipx_server_running = true;
			ipx_server_thread  = std::thread([]() {
                                while (ipx_server_running) {
                                        const int num_ready = SDLNet_CheckSockets(
                                                socket_set, 100);
                                        if (num_ready == -1) {
                                                LOG_ERR("IPXSERVER: %s",
                                                        SDLNet_GetError());
                                                continue;
                                        }
                                        if (num_ready > 0) {
                                                IPX_ServerLoop();
                                        }
                                }
                        });
		}
		return true;
	}
	return false;
}

#endif
