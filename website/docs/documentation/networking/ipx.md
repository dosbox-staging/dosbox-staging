# IPX

## Overview

IPX was *the* LAN protocol for DOS multiplayer gaming. If you ever hauled
your PC to a friend's house for a LAN party — pizza, CRTs, and a tangle of
coaxial cable — IPX is what made *Doom*, *Duke Nukem 3D*, *Warcraft II*,
and *Command & Conquer* talk to each other.

DOSBox Staging tunnels IPX packets over modern UDP/IP, so you can play these
games over the internet or any local network. One player starts a server
with `IPXNET STARTSERVER` at the DOS prompt; others connect with
`IPXNET CONNECT <address>`.

Enable IPX support here, then use the `IPXNET` command inside DOS to manage
connections.

TODO(CL) mobygames links


## Configuration settings

You can set the IPX parameters in the `[ipx]` configuration section.


##### ipx

:   Enable IPX over UDP/IP emulation.

    Possible values: `on`, `off` *default*{ .default }
