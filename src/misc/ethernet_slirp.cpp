/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2022  The DOSBox Staging Team
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

#if C_SLIRP

#include <algorithm>
#include <map>
#include <stdexcept>

#if defined(BSD)
#include <sys/socket.h> // AF_INET
#endif

#include "dosbox.h"
#include "ethernet_slirp.h"
#include "setup.h"
#include "string_utils.h"
#include "timer.h"

/* Begin boilerplate to map libslirp's C-based callbacks to our C++
 * object. The user data is provided inside the 'opaque' pointer.
 */

ssize_t slirp_receive_packet(const void *buf, size_t len, void *opaque)
{
	// sentinels
	if (!len)
		return 0;

	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	const auto bytes_to_receive = check_cast<int>(len);

	if (bytes_to_receive > conn->GetMRU()) {
		LOG_MSG("SLIRP: refusing to receive packet with length %d exceeding MRU %d",
		        bytes_to_receive, conn->GetMRU());
		return -1;
	}
	return conn->ReceivePacket(static_cast<const uint8_t *>(buf),
	                           bytes_to_receive);
}

void slirp_guest_error(const char *msg, [[maybe_unused]] void *opaque)
{
	LOG_MSG("SLIRP: Slirp error: %s", msg);
}

int64_t slirp_clock_get_ns([[maybe_unused]] void *opaque)
{
	return GetTicksUs() * 1000;
}

void *slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque)
{
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	return conn->TimerNew(cb, cb_opaque);
}

void slirp_timer_free(void *timer, void *opaque)
{
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	struct slirp_timer *real_timer = (struct slirp_timer *)timer;
	conn->TimerFree(real_timer);
}

void slirp_timer_mod(void *timer, int64_t expire_time, void *opaque)
{
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	struct slirp_timer *real_timer = (struct slirp_timer *)timer;
	conn->TimerMod(real_timer, expire_time);
}

int slirp_add_poll(int fd, int events, void *opaque)
{
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	return (fd < 0) ? fd : conn->PollAdd(fd, events);
}

int slirp_get_revents(int idx, void *opaque)
{
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	return (idx < 0) ? idx : conn->PollGetSlirpRevents(idx);
}

void slirp_register_poll_fd(int fd, void *opaque)
{
	// sentinel
	if (fd < 0)
		return;
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	conn->PollRegister(fd);
}

void slirp_unregister_poll_fd(int fd, void *opaque)
{
	// sentinel
	if (fd < 0)
		return;
	const auto conn = static_cast<SlirpEthernetConnection *>(opaque);
	conn->PollUnregister(fd);
}

void slirp_notify([[maybe_unused]] void *opaque)
{
	// empty, function is provided for API compliance
}

/* End boilerplate */

SlirpEthernetConnection::SlirpEthernetConnection()
        : EthernetConnection(),
          config(),
          timers(),
          get_packet_callback(),
          registered_fds(),
#ifdef WIN32
          readfds(),
          writefds(),
          exceptfds()
#else /* !WIN32 */
          polls()
#endif
{
	slirp_callbacks.send_packet = slirp_receive_packet;
	slirp_callbacks.guest_error = slirp_guest_error;
	slirp_callbacks.clock_get_ns = slirp_clock_get_ns;
	slirp_callbacks.timer_new = slirp_timer_new;
	slirp_callbacks.timer_free = slirp_timer_free;
	slirp_callbacks.timer_mod = slirp_timer_mod;
	slirp_callbacks.register_poll_fd = slirp_register_poll_fd;
	slirp_callbacks.unregister_poll_fd = slirp_unregister_poll_fd;
	slirp_callbacks.notify = slirp_notify;
}

SlirpEthernetConnection::~SlirpEthernetConnection()
{
	if (slirp)
		slirp_cleanup(slirp);
}

bool SlirpEthernetConnection::Initialize(Section *dosbox_config)
{
	LOG_MSG("SLIRP: Slirp version: %s", slirp_version_string());

	/* Config */
	config.version = 1;

	// If true, prevents the guest from accessing the host, which will cause
	// libslrip's internal DHCP server to fail.
	config.restricted = false;

	// If true, prevent the guest access from accessing the host's loopback
	// interfaces.
	config.disable_host_loopback = false;

	// The maximum transmission and receive unit sizes.
	constexpr auto ethernet_frame_size = 14 + 1500; // header + payload
	config.if_mtu = ethernet_frame_size;
	config.if_mru = ethernet_frame_size;

	config.enable_emu = 0; // buggy - keep this at 0
	config.in_enabled = 1;

	// The IPv4 network the guest and host services are on
	inet_pton(AF_INET, "10.0.2.0", &config.vnetwork);

	// The netmask for the IPv4 network.
	inet_pton(AF_INET, "255.255.255.0", &config.vnetmask);
	inet_pton(AF_INET, "10.0.2.2", &config.vhost);
	inet_pton(AF_INET, "10.0.2.3", &config.vnameserver);
	inet_pton(AF_INET, "10.0.2.15", &config.vdhcp_start);

	/* IPv6 code is left here as reference but disabled as no DOS-era
	 * software supports it and might get confused by it */
	config.in6_enabled = 0;
	inet_pton(AF_INET6, "fec0::", &config.vprefix_addr6);
	config.vprefix_len = 64;
	inet_pton(AF_INET6, "fec0::2", &config.vhost6);
	inet_pton(AF_INET6, "fec0::3", &config.vnameserver6);

	/* DHCPv4, BOOTP, TFTP */
	config.vhostname = CANONICAL_PROJECT_NAME;
	config.vdnssearch = NULL;
	config.vdomainname = NULL;
	config.tftp_server_name = NULL;
	config.tftp_path = NULL;
	config.bootfile = NULL;

	slirp = slirp_new(&config, &slirp_callbacks, this);
	if (slirp) {
		const auto section = static_cast<Section_prop *>(dosbox_config);
		assert(section);

		bool is_udp = false;
		ClearPortForwards(is_udp, forwarded_tcp_ports);
		forwarded_tcp_ports = SetupPortForwards(is_udp, section->Get_string("tcp_port_forwards"));

		is_udp = true;
		ClearPortForwards(is_udp, forwarded_udp_ports);
		forwarded_udp_ports = SetupPortForwards(is_udp, section->Get_string("udp_port_forwards"));

		LOG_MSG("SLIRP: Successfully initialized");
		return true;
	} else {
		LOG_MSG("SLIRP: Failed to initialize");
		return false;
	}
}

void SlirpEthernetConnection::ClearPortForwards(const bool is_udp, std::map<int, int> &existing_port_forwards)
{
	const auto protocol = is_udp ? "UDP" : "TCP";

	in_addr bind_addr;
	inet_pton(AF_INET, "0.0.0.0", &bind_addr);

	for (const auto &[host_port, guest_port] : existing_port_forwards)
		if (slirp_remove_hostfwd(slirp, is_udp, bind_addr, host_port) >= 0)
			LOG_INFO("SLIRP: Removed old %s port %d:%d forward", protocol, host_port, guest_port);
		else
			LOG_WARNING("SLIRP: Failed removing old %s port %d:%d foward", protocol, host_port, guest_port);

	existing_port_forwards.clear();
}

std::map<int, int> SlirpEthernetConnection::SetupPortForwards(const bool is_udp, const std::string &port_forward_rules)
{
	std::map<int, int> forwarded_ports;
	const auto protocol = is_udp ? "UDP" : "TCP";

	in_addr bind_addr;
	inet_pton(AF_INET, "0.0.0.0", &bind_addr);

	// Split the rules first by spaces
	for (auto &forward_rule : split(port_forward_rules, ' ')) {
		if (forward_rule.empty())
			continue;

		// Split the rule into host:guest portions
		auto forward_rule_parts = split(forward_rule, ':');
		// if only one is provided, then the guest port is the same
		if (forward_rule_parts.size() == 1)
			forward_rule_parts.push_back(forward_rule_parts[0]);

		// We should now have two parts, host and guest
		if (forward_rule_parts.size() != 2) {
			LOG_WARNING("SLIRP: Invalid %s port forward rule: %s", protocol, forward_rule.c_str());
			continue;
		}

		// Now going to populate this rule's port extents
		std::vector<int> ports = {};
		ports.reserve(4);

		// Process the host and guest portions separately
		for (const auto &port_range_part : forward_rule_parts) {
			auto port_range = split(port_range_part, '-');

			// If only one value is provided, then the start and end are the same
			if (port_range.size() == 1 && !port_range[0].empty())
				port_range.push_back(port_range[0]);

			// We should now have two parts, start and end
			if (port_range.size() != 2) {
				LOG_WARNING("SLIRP: Invalid %s port range: %s", protocol, port_range_part.c_str());
				break;
			}

			// Now convert the ports to integers and push them into the 'ports' vector
			assert(port_range.size() == 2);
			for (const auto &port : port_range) {
				if (port.empty()) {
					LOG_WARNING("SLIRP: Invalid %s port range: %s", protocol, port_range_part.c_str());
					break;
				}
				try {
					const int port_num = std::stoi(port);
					ports.push_back(port_num);
				} catch ([[maybe_unused]] std::invalid_argument &e) {
					LOG_WARNING("SLIRP: Invalid %s port: %s", protocol, port.c_str());
					break;
				} catch ([[maybe_unused]] std::out_of_range &e) {
					LOG_WARNING("SLIRP: Invalid %s port: %s", protocol, port.c_str());
					break;
				}
			}
		}

		// If both halves of the rule are valid, we will have four ports in the vector
		if (ports.size() != 4) {
			LOG_WARNING("SLIRP: Invalid %s port forward rule: %s", protocol, forward_rule.c_str());
			continue;
		}

		// assign the ports to the host and guest portions
		assert(ports.size() == 4);
		const auto &host_port_start = ports[0];
		const auto &host_port_end = ports[1];
		const auto &guest_port_start = ports[2];
		const auto &guest_port_end = ports[3];

		// Check if the both port ranges are valid
		if (host_port_end < host_port_start || guest_port_end < guest_port_start) {
			LOG_WARNING("SLIRP: Invalid %s port range(s): %s",
			            protocol, forward_rule.c_str());
			continue;
		}

		// Sanity check that the maximum range doesn't go beyond the maximum port number
		constexpr auto min_valid_port = 1;
		constexpr auto max_valid_port = 65535;
		const auto range = std::max(host_port_end - host_port_start, guest_port_end - guest_port_start);
		if (host_port_start < min_valid_port || host_port_start + range > max_valid_port ||
		    guest_port_start < min_valid_port || guest_port_start + range > max_valid_port) {
			LOG_WARNING("SLIRP: Invalid %s port range(s): %s",
			            protocol, forward_rule.c_str());
			continue;
		}

		// Setup the remaining tally and starting port numbers to be incremented
		auto n = range + 1;
		auto host_port = host_port_start;
		auto guest_port = guest_port_start;

		// Start adding the port forwards
		LOG_MSG("SLIRP: Processing %s port forward rule: %s", protocol, forward_rule.c_str());
		while (n--) {
			// Add the port forward rule
			if (slirp_add_hostfwd(slirp, is_udp, bind_addr, host_port, bind_addr, guest_port) == 0) {
				forwarded_ports[host_port] = guest_port;
				LOG_MSG("SLIRP: Setup %s port %d:%d forward", protocol, host_port, guest_port);
			} else {
				LOG_WARNING("SLIRP: Failed setting up %s port %d:%d forward", protocol, host_port, guest_port);
			}
			++host_port;
			++guest_port;
		} // end port addition loop
	} // end forward rule loop

	return forwarded_ports;
}

void SlirpEthernetConnection::SendPacket(const uint8_t *packet, int len)
{
	// sentinels
	if (len <= 0)
		return;
	if (len > GetMTU()) {
		LOG_WARNING("SLIRP: refusing to send packet with length %d exceeding MTU %d",
		            len, GetMTU());
		return;
	}
	slirp_input(slirp, packet, len);
}

void SlirpEthernetConnection::GetPackets(std::function<int(const uint8_t *, int)> callback)
{
	get_packet_callback = callback;
	uint32_t timeout_ms = 0;
	PollsClear();
	PollsAddRegistered();
	slirp_pollfds_fill(slirp, &timeout_ms, slirp_add_poll, this);
	const bool poll_failed = !PollsPoll(timeout_ms);
	slirp_pollfds_poll(slirp, poll_failed, slirp_get_revents, this);
	TimersRun();
}

int SlirpEthernetConnection::ReceivePacket(const uint8_t *packet, int len)
{
	// sentinels
	if (len <= 0)
		return len;
	if (len > GetMRU()) {
		LOG_WARNING("SLIRP: refusing to receive packet with length %d exceeding MRU %d",
		            len, GetMRU());
		return -1;
	}
	return get_packet_callback(packet, len);
}

struct slirp_timer *SlirpEthernetConnection::TimerNew(SlirpTimerCb cb, void *cb_opaque)
{
	struct slirp_timer *timer = new struct slirp_timer;
	timer->expires_ns = 0;
	timer->cb = cb;
	timer->cb_opaque = cb_opaque;
	timers.push_back(timer);
	return timer;
}

void SlirpEthernetConnection::TimerFree(struct slirp_timer *timer)
{
	timers.erase(std::remove(timers.begin(), timers.end(), timer), timers.end());
	delete timer;
}

void SlirpEthernetConnection::TimerMod(struct slirp_timer *timer, int64_t expire_time_ms)
{
	/* expire_time is in milliseconds despite slirp wanting a nanosecond
	 * clock */
	timer->expires_ns = expire_time_ms * 1'000'000;
}

void SlirpEthernetConnection::TimersRun()
{
	int64_t now = slirp_clock_get_ns(NULL);
	for (struct slirp_timer *timer : timers) {
		if (timer->expires_ns && timer->expires_ns < now) {
			timer->expires_ns = 0;
			timer->cb(timer->cb_opaque);
		}
	}
}

void SlirpEthernetConnection::TimersClear()
{
	for (auto *timer : timers)
		delete timer;
	timers.clear();
}

void SlirpEthernetConnection::PollRegister(const int fd)
{
	// sentinel
	if (fd < 0)
		return;
#ifdef WIN32
	/* BUG: Skip this entirely on Win32 as libslirp gives us invalid fds. */
	return;
#endif
	PollUnregister(fd);
	registered_fds.push_back(fd);
}

void SlirpEthernetConnection::PollUnregister(const int fd)
{
	// sentinels
	if (fd < 0 || registered_fds.empty())
		return;
	registered_fds.erase(std::remove(registered_fds.begin(), registered_fds.end(), fd), registered_fds.end());
}

void SlirpEthernetConnection::PollsAddRegistered()
{
	for (const auto fd : registered_fds)
		if (fd >= 0)
			PollAdd(fd, SLIRP_POLL_IN | SLIRP_POLL_OUT);
}

/* Begin the bulk of the platform-specific code.
 * This mostly involves handling data structures and mapping
 * libslirp's view of our polling system to whatever we use
 * internally.
 * libslirp really wants poll() as it gives information about
 * out of band TCP data and connection hang-ups.
 * This is easy to do on Unix, but on other systems it needs
 * custom implementations that give this data. */

#ifndef WIN32

void SlirpEthernetConnection::PollsClear()
{
	polls.clear();
}

int SlirpEthernetConnection::PollAdd(const int fd, int slirp_events)
{
	// sentinel
	if (fd < 0)
		return fd;
	int16_t real_events = 0;
	if (slirp_events & SLIRP_POLL_IN)
		real_events |= POLLIN;
	if (slirp_events & SLIRP_POLL_OUT)
		real_events |= POLLOUT;
	if (slirp_events & SLIRP_POLL_PRI)
		real_events |= POLLPRI;
	struct pollfd new_poll;
	new_poll.fd = fd;
	new_poll.events = real_events;
	polls.push_back(new_poll);
	return (check_cast<int>(polls.size() - 1));
}

bool SlirpEthernetConnection::PollsPoll(uint32_t timeout_ms)
{
	// sentinel
	if (polls.empty())
		return false;
	const auto ret = poll(polls.data(), polls.size(),
	                      static_cast<int>(timeout_ms));
	return (ret > -1);
}

int SlirpEthernetConnection::PollGetSlirpRevents(int idx)
{
	assert(idx >= 0 && idx < static_cast<int>(polls.size()));
	const auto real_revents = polls.at(static_cast<size_t>(idx)).revents;
	int slirp_revents = 0;
	if (real_revents & POLLIN)
		slirp_revents |= SLIRP_POLL_IN;
	if (real_revents & POLLOUT)
		slirp_revents |= SLIRP_POLL_OUT;
	if (real_revents & POLLPRI)
		slirp_revents |= SLIRP_POLL_PRI;
	if (real_revents & POLLERR)
		slirp_revents |= SLIRP_POLL_ERR;
	if (real_revents & POLLHUP)
		slirp_revents |= SLIRP_POLL_HUP;
	return slirp_revents;
}

#else

void SlirpEthernetConnection::PollsClear()
{
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
}

int SlirpEthernetConnection::PollAdd(int fd, int slirp_events)
{
	// sentinel
	if (fd < 0)
		return fd;

	// compiler-specific implementation of FD_SET uses a SOCKET type
	// under MSYS2
#ifdef WIN32
	assert(fd >= 0);
	auto fd_socket = static_cast<SOCKET>(fd);
	if (slirp_events & SLIRP_POLL_IN)
		FD_SET(fd_socket, &readfds);
	if (slirp_events & SLIRP_POLL_OUT)
		FD_SET(fd_socket, &writefds);
	if (slirp_events & SLIRP_POLL_PRI)
		FD_SET(fd_socket, &exceptfds);
	fd = check_cast<int>(fd_socket);
#else
	if (slirp_events & SLIRP_POLL_IN)
		FD_SET(fd, &readfds);
	if (slirp_events & SLIRP_POLL_OUT)
		FD_SET(fd, &writefds);
	if (slirp_events & SLIRP_POLL_PRI)
		FD_SET(fd, &exceptfds);
#endif
	return fd;
}

bool SlirpEthernetConnection::PollsPoll(uint32_t timeout_ms)
{
	struct timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;
	const auto ret = select(0, &readfds, &writefds, &exceptfds, &timeout);
	return (ret > -1);
}

int SlirpEthernetConnection::PollGetSlirpRevents(int idx)
{
	// sentinel
	if (idx < 0)
		return idx;
	/* Windows does not support poll(). It has WSAPoll() but this is
	 * reported as broken by libcurl and other projects, and Microsoft
	 * doesn't seem to want to fix this any time soon.
	 * glib provides g_poll() but that doesn't seem to work either.
	 * The solution I've made uses plain old select(), but checks for
	 * extra conditions and adds those to the flags we pass to libslirp.
	 * There's no one-to-one mapping of poll() flags on Windows, so here's
	 * my definition:
	 * SLIRP_POLL_HUP: The remote closed the socket gracefully.
	 * SLIRP_POLL_ERR: An exception happened or reading failed
	 * SLIRP_POLL_PRI: TCP Out-of-band data available
	 */
	int slirp_revents = 0;
	if (FD_ISSET(idx, &readfds)) {
		/* This code is broken on ReactOS peeking a closed socket
		 * will cause the next recv() to fail instead of acting
		 * normally. See CORE-17425 on their JIRA */
		char buf[8];
		const int read = recv(idx, buf, sizeof(buf), MSG_PEEK);
		const int error = (read == SOCKET_ERROR) ? WSAGetLastError() : 0;
		if (read > 0 || error == WSAEMSGSIZE) {
			slirp_revents |= SLIRP_POLL_IN;
		} else if (read == 0) {
			slirp_revents |= SLIRP_POLL_IN;
			slirp_revents |= SLIRP_POLL_HUP;
		} else {
			slirp_revents |= SLIRP_POLL_IN;
			slirp_revents |= SLIRP_POLL_ERR;
		}
	}
	if (FD_ISSET(idx, &writefds)) {
		slirp_revents |= SLIRP_POLL_OUT;
	}
	if (FD_ISSET(idx, &exceptfds)) {
		u_long atmark = 0;
		if (ioctlsocket(idx, SIOCATMARK, &atmark) == 0 && atmark == 1) {
			slirp_revents |= SLIRP_POLL_PRI;
		} else {
			slirp_revents |= SLIRP_POLL_ERR;
		}
	}
	return slirp_revents;
}

#endif

#endif
