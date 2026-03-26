# IPX

## Overview

IPX was *the* LAN protocol for DOS multiplayer gaming. If you ever hauled
your PC to a friend's house for a LAN party --- pizza, CRTs, and a tangle of
coaxial cable --- IPX is what made
[Doom](https://www.mobygames.com/game/1068/doom/),
[Duke Nukem 3D](https://www.mobygames.com/game/365/duke-nukem-3d/),
[Warcraft II](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/),
and [Command & Conquer](https://www.mobygames.com/game/338/command-conquer/)
talk to each other.

DOSBox Staging tunnels IPX packets over modern UDP/IP, so you can play these
games over the internet or any local network. One player starts a server
with `IPXNET STARTSERVER` at the DOS prompt; others connect with
`IPXNET CONNECT <address>`.

Enable IPX support here, then use the `IPXNET` command inside DOS to manage
connections.


## Configuration settings

You can set the IPX parameters in the `[ipx]` configuration section.


##### ipx

:   Enable IPX over UDP/IP emulation.

    Possible values: `on`, `off` *default*{ .default }
