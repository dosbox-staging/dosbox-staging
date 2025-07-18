// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ETHERNET_SLIRP_H
#define DOSBOX_ETHERNET_SLIRP_H

#include "dosbox.h"

#include <map>
#include <deque>
#include <vector>

#include <slirp/libslirp.h>

#include "config.h"
#include "ethernet.h"

/*
 * libslirp really wants a poll() API, so we'll use that when we're
 * not on Windows. When we are on Windows, we'll fall back to using
 * select() as well as some Windows APIs.
 *
 * For reference, QEMU seems to use glib's g_poll() API.
 * This doesn't seem to work on Windows from my tests.
 */
#ifndef WIN32
#include <poll.h>
#endif

/** A libslirp timer
 * libslirp has to simulate periodic tasks such as IPv6 router
 * advertisements. It does this by giving us a callback and expiry
 * time. We have to hold on to it and call the callback when the
 * time is right.
 */
struct slirp_timer {
	int64_t expires_ns = 0; /*!< When to fire the callback, in nanoseconds */
	SlirpTimerCb cb = {}; /*!< The callback to fire */
	void *cb_opaque = nullptr; /*!< Data libslirp wants us to pass to the
	                              callback */
};

/** A libslirp-based Ethernet connection
 * This backend uses a virtual Ethernet device. Only TCP, UDP and some ICMP
 * work over this interface. This is because libslirp terminates guest
 * connections during routing and passes them to sockets created in the host.
 */
class SlirpEthernetConnection : public EthernetConnection {
public:
	/* Boilerplate EthernetConnection interface */
	SlirpEthernetConnection();
	~SlirpEthernetConnection() override;

	/* We can't copy this */
	SlirpEthernetConnection(const SlirpEthernetConnection&) = delete;
	SlirpEthernetConnection& operator=(const SlirpEthernetConnection&) = delete;

	bool Initialize(Section* config) override;
	void SendPacket(const uint8_t* packet, int len) override;
	void GetPackets(std::function<int(const uint8_t*, int)> callback) override;

	/* Called by libslirp when it has a packet for us */
	int ReceivePacket(const uint8_t* packet, int len);

	// Used in callbacks to bounds-check packet lengths
	int GetMTU() const
	{
		return static_cast<int>(config.if_mtu);
	}
	int GetMRU() const
	{
		return static_cast<int>(config.if_mru);
	}

	/* Called by libslirp to create, free and modify timers */
	struct slirp_timer* TimerNew(SlirpTimerCb cb, void* cb_opaque);
	void TimerFree(struct slirp_timer* timer);
	void TimerMod(struct slirp_timer* timer, int64_t expire_time);

	/* Called by libslirp to interact with our polling system */
	int PollAdd(int fd, int slirp_events);
	int PollGetSlirpRevents(int idx);
	void PollRegister(int fd);
	void PollUnregister(int fd);

private:
	/* Runs and clears all the timers*/
	void TimersRun();
	void TimersClear();

	void ClearPortForwards(const bool is_udp, std::map<int, int> &existing_port_forwards);
	std::map<int, int> SetupPortForwards(const bool is_udp, const std::string &port_forward_rules);

	/* Builds a list of descriptors and polls them */
	void PollsAddRegistered();
	void PollsClear();
	bool PollsPoll(uint32_t timeout_ms);

	Slirp *slirp = nullptr;        /*!< Handle to libslirp */
	SlirpConfig config = {};       /*!< Configuration passed to libslirp */
	SlirpCb slirp_callbacks = {};  /*!< Callbacks used by libslirp */
	std::deque<struct slirp_timer *> timers = {}; /*!< Stored timers */

	/** The GetPacket callback
	 * When libslirp has a new packet for us it calls ReceivePacket,
	 * but the EthernetConnection interface requires users to poll
	 * for new packets using GetPackets. We temporarily store the
	 * callback from GetPackets here for ReceivePacket.
	 * This might seem racy, but keep in mind we control when
	 * libslirp sends us packets via our polling system.
	 */
	std::function<int(const uint8_t *, int)> get_packet_callback = nullptr;

	std::deque<int> registered_fds = {}; /*!< File descriptors to watch */

	// keep track of the ports fowarded
	std::map<int, int> forwarded_tcp_ports = {};
	std::map<int, int> forwarded_udp_ports = {};

#ifndef WIN32
	std::vector<struct pollfd> polls = {}; /*!< Descriptors for poll() */
#else
	fd_set readfds = {};   /*!< Read descriptors for select() */
	fd_set writefds = {};  /*!< Write descriptors for select() */
	fd_set exceptfds = {}; /*!< Exceptional descriptors for select() */
#endif
};

#endif
