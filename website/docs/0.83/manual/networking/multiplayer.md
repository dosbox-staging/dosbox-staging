# Multiplayer

DOS multiplayer games used two main networking approaches: **serial
connections** (modem dial-up or direct null modem cable) for head-to-head
play, and **IPX** for LAN parties. DOSBox Staging emulates both over modern
internet connections.

!!! tip

    For per-game setup instructions and troubleshooting, see the
    [Multiplayer & Serial Ports](https://github.com/dosbox-staging/dosbox-staging/wiki/Multiplayer-&-Serial-Ports)
    wiki page.


## Serial multiplayer

Games like
[Doom](https://www.mobygames.com/game/1068/doom/),
[Rise of the Triad](https://www.mobygames.com/game/418/rise-of-the-triad-dark-war/),
and [Terminal Velocity](https://www.mobygames.com/game/635/terminal-velocity/)
supported modem or null modem play. DOSBox Staging's
[serial port](serial-ports.md) emulation handles both.


### Modem mode

Modem mode emulates a Hayes-compatible dial-up modem. One player listens for
incoming calls; the other dials their IP address (or hostname) using the
game's built-in dial-up interface --- the same `ATDT` commands real modems
used, just routed over the internet instead of a phone line.

The **host** listens on a port:

``` ini
[serial]
serial1 = modem listenport:5000 sock:1
```

The **client** dials the host's address from within the game, e.g.,
`ATDT 203.0.113.5:5000`.

Some games have restrictive phone number input fields that won't accept IP
addresses. You can work around this with a **phonebook file** --- map fake
phone numbers to real addresses in `phonebook.txt` (or whatever filename
you've set via [`phonebookfile`](serial-ports.md#phonebookfile)):

```
5551234 203.0.113.5:5000
```

Now dialling `5551234` in the game connects to your friend's machine.


### Nullmodem mode

Nullmodem mode creates a direct serial link between two DOSBox instances,
skipping the modem handshake entirely. This is the simplest option when both
players can coordinate outside the game.

The **host** listens on a port:

``` ini
[serial]
serial1 = nullmodem port:1250 sock:1
```

The **client** connects to the host:

``` ini
[serial]
serial1 = nullmodem server:203.0.113.5 port:1250 sock:1
```

The connection is established as soon as both sides start DOSBox --- no
dialling required. The game sees a standard serial connection on COM1.


### ENet reliable UDP transport

By default, both modem and nullmodem modes use TCP. Adding `sock:1` switches
to **ENet**, a reliable UDP-based transport. For internet play, ENet is
strongly preferred --- it has lower latency than TCP and is more resilient on
unreliable connections. Both sides **must** use the same transport.


## IPX multiplayer

LAN-party games like
[Duke Nukem 3D](https://www.mobygames.com/game/365/duke-nukem-3d/),
[WarCraft II](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/),
and [Command & Conquer](https://www.mobygames.com/game/338/command-conquer/)
used the IPX protocol. DOSBox Staging tunnels IPX over modern UDP/IP. One
player starts a server, others connect to it:

```
IPXNET STARTSERVER
IPXNET CONNECT 203.0.113.5
```

See the [IPX](ipx.md) page for configuration details.


## Port forwarding and firewalls

In both serial and IPX multiplayer, the host needs the chosen port to be
reachable from the internet. If the host is behind a router or NAT, this
typically means setting up **port forwarding** for that single port (UDP when
using `sock:1` or IPX, TCP otherwise).

If neither player can forward a port, a **VPN** such as
[ZeroTier](https://www.zerotier.com/) or [Tailscale](https://tailscale.com/)
is the easiest alternative --- both players join the same virtual network and
connect using their VPN-assigned addresses, with no port forwarding needed.
