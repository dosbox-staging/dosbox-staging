// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "hardware/network/ipxserver.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>

#include "hardware/network/ipx.h"
#include "hardware/timer.h"

static IPaddress ipxServerIp; // IPAddress for server's listening port

static asio::io_context ipx_io;
static std::unique_ptr<asio::ip::udp::socket> ipx_server_socket = nullptr;
static std::optional<asio::executor_work_guard<asio::io_context::executor_type>> ipx_work_guard;

static packetBuffer connBuffer[SOCKETTABLESIZE];

static uint8_t inBuffer[IPXBUFFERSIZE];
static IPaddress ipconn[SOCKETTABLESIZE]; // Active TCP/IP connection

static std::thread ipx_server_thread;
static std::atomic_bool ipx_server_running = false;

static void ipx_server_arm_receive();

static asio::ip::udp::endpoint to_endpoint(const IPaddress& addr)
{
	const asio::ip::address_v4::bytes_type bytes = {
	        static_cast<unsigned char>(addr.host & 0xff),
	        static_cast<unsigned char>((addr.host >> 8) & 0xff),
	        static_cast<unsigned char>((addr.host >> 16) & 0xff),
	        static_cast<unsigned char>((addr.host >> 24) & 0xff),
	};
	const uint16_t port = Net_PortToHost(addr.port);
	return {asio::ip::address_v4(bytes), port};
}

static IPaddress from_endpoint(const asio::ip::udp::endpoint& ep)
{
	IPaddress addr   = {};
	const auto bytes = ep.address().to_v4().to_bytes();
	addr.host        = static_cast<uint32_t>(bytes[0]) |
	            (static_cast<uint32_t>(bytes[1]) << 8) |
	            (static_cast<uint32_t>(bytes[2]) << 16) |
	            (static_cast<uint32_t>(bytes[3]) << 24);
	Net_SetPort(addr, ep.port());
	return addr;
}

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

static void sendIPXPacket(uint8_t *buffer, int16_t bufSize) {
	uint16_t srcport, destport;
	uint32_t srchost, desthost;
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
				if (!ipx_server_socket) {
					continue;
				}
				std::error_code ec;
				ipx_server_socket->send_to(asio::buffer(buffer, bufSize),
				                           to_endpoint(ipconn[i]),
				                           0,
				                           ec);
				if (ec) {
					LOG_MSG("IPXSERVER: send failed: %s",
					        ec.message().c_str());
					continue;
				}
				//LOG_MSG("IPXSERVER: Packet of %d bytes sent from %d.%d.%d.%d to %d.%d.%d.%d (BROADCAST) (%x CRC)", bufSize, CONVIP(srchost), CONVIP(ipconn[i].host), packetCRC(&buffer[30], bufSize-30));
			}
		}
	} else {
		// Specific address
		for (uint16_t i = 0; i < SOCKETTABLESIZE; ++i) {
			if((connBuffer[i].connected) && (ipconn[i].host == desthost) && (ipconn[i].port == destport)) {
				if (!ipx_server_socket) {
					continue;
				}
				std::error_code ec;
				ipx_server_socket->send_to(asio::buffer(buffer, bufSize),
				                           to_endpoint(ipconn[i]),
				                           0,
				                           ec);
				if (ec) {
					LOG_MSG("IPXSERVER: send failed: %s",
					        ec.message().c_str());
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

	Net_Write16(0xffff, regHeader.checkSum);
	Net_Write16(static_cast<uint16_t>(sizeof(regHeader)), regHeader.length);

	Net_Write32(0, regHeader.dest.network);
	PackIP(clientAddr, &regHeader.dest.addr.byIP);
	Net_Write16(0x2, regHeader.dest.socket);

	Net_Write32(1, regHeader.src.network);
	PackIP(ipxServerIp, &regHeader.src.addr.byIP);
	Net_Write16(0x2, regHeader.src.socket);
	regHeader.transControl = 0;

	if (!ipx_server_socket) {
		return;
	}
	std::error_code ec;
	ipx_server_socket->send_to(asio::buffer(&regHeader, sizeof(regHeader)),
	                           to_endpoint(clientAddr),
	                           0,
	                           ec);
	if (ec) {
		LOG_MSG("IPXSERVER: Connection response not sent: %s",
		        ec.message().c_str());
	}
}

static void IPX_ServerHandlePacket(const IPaddress& sender_addr, const size_t packet_len)
{
	IPaddress tmpAddr;
	uint32_t host;

	if (packet_len == 0) {
		return;
	}
	if (packet_len > IPXBUFFERSIZE) {
		return;
	}

	// Check to see if incoming packet is a registration packet
	// For this, I just spoofed the echo protocol packet designation 0x02
	auto* tmpHeader = reinterpret_cast<IPXHeader*>(&inBuffer[0]);

	// Check to see if echo packet
	if (Net_Read16(tmpHeader->dest.socket) == 0x2) {
		// Null destination node means its a server registration packet
		if (tmpHeader->dest.addr.byIP.host == 0x0) {
			UnpackIP(tmpHeader->src.addr.byIP, &tmpAddr);
			for (uint16_t i = 0; i < SOCKETTABLESIZE; ++i) {
				if (!connBuffer[i].connected) {
					// Use prefered host IP rather than the
					// reported source IP It may be better
					// to use the reported source
					ipconn[i] = sender_addr;

					connBuffer[i].connected = true;
					host = ipconn[i].host;
					LOG_MSG("IPXSERVER: Connect from %d.%d.%d.%d",
					        CONVIP(host));
					ackClient(sender_addr);
					return;
				}

				if ((ipconn[i].host == tmpAddr.host) &&
				    (ipconn[i].port == tmpAddr.port)) {
					LOG_MSG("IPXSERVER: Reconnect from %d.%d.%d.%d",
					        CONVIP(tmpAddr.host));
					// Update anonymous port number if changed
					ipconn[i].port = sender_addr.port;
					ackClient(sender_addr);
					return;
				}
			}
		}
	}

	// IPX packet is complete. Now interpret IPX header and send to
	// respective IP address
	sendIPXPacket(inBuffer, static_cast<int16_t>(packet_len));
}

static void ipx_server_arm_receive()
{
	if (!ipx_server_socket) {
		return;
	}

	// One outstanding receive at a time, so it's safe to use these statics.
	static asio::ip::udp::endpoint sender;

	ipx_server_socket->async_receive_from(
	        asio::buffer(inBuffer, IPXBUFFERSIZE),
	        sender,
	        [](const std::error_code& ec, const std::size_t len) {
		        if (!ipx_server_running) {
			        return;
		        }
		        if (ec) {
			        // Expected during shutdown
			        if (ec == asio::error::operation_aborted) {
				        return;
			        }
			        LOG_ERR("IPXSERVER: recv failed: %s",
			                ec.message().c_str());
			        ipx_server_arm_receive();
			        return;
		        }

		        IPX_ServerHandlePacket(from_endpoint(sender), len);
		        ipx_server_arm_receive();
	        });
}

void IPX_StopServer()
{
	ipx_server_running = false;

	if (ipx_server_socket) {
		std::error_code ec;
		ipx_server_socket->cancel(ec);
		ipx_server_socket->close(ec);
		ipx_server_socket.reset();
	}

	ipx_work_guard.reset();
	ipx_io.stop();

	if (ipx_server_thread.joinable()) {
		ipx_server_thread.join();
	}
}

bool IPX_StartServer(uint16_t portnum)
{
	// Server identity used in registration replies; historically this was
	// INADDR_ANY + port.
	ipxServerIp.host = 0;
	Net_SetPort(ipxServerIp, portnum);

	std::error_code ec;

	// If previously stopped, make io_context runnable again.
	ipx_io.restart();
	ipx_server_socket = std::make_unique<asio::ip::udp::socket>(ipx_io);
	ipx_server_socket->open(asio::ip::udp::v4(), ec);
	if (ec) {
		return false;
	}
	ipx_server_socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), portnum),
	                        ec);
	if (ec) {
		return false;
	}
	// Async receive blocks efficiently; no non-blocking polling needed.

	for (auto& i : connBuffer) {
		i.connected = false;
	}

	if (!ipx_server_running) {
		ipx_server_running = true;
		ipx_work_guard.emplace(asio::make_work_guard(ipx_io));
		ipx_server_arm_receive();
		ipx_server_thread = std::thread([]() { ipx_io.run(); });
	}

	return true;
}
