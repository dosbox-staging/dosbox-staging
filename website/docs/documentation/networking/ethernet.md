# Ethernet


## Configuration settings

You can set the Ethernet parameters in the `[ethernet]` configuration
section.


##### macaddr

:   The MAC address of the NE2000 card (`AC:DE:48:88:99:AA` by default).


##### ne2000

:   Enable emulation of a Novell NE2000 network card on a software-based
    network with the following properties:

    <div class="compact" markdown>

    - `255.255.255.0` -- Subnet mask of the 10.0.2.0 virtual LAN.
    - `10.0.2.2` -- IP of the gateway and DHCP service.
    - `10.0.2.3` -- IP of the virtual DNS server.
    - `10.0.2.15` -- First IP provided by DHCP (this is your IP).

    </div>

    Possible values: `on`, `off` *default*{ .default }

    !!! note

        Using this feature requires an NE2000 packet driver, a DHCP client,
        and a TCP/IP stack set up in DOS. You might need port-forwarding from
        your host OS into DOSBox, and from your router to your host OS when
        acting as the server in multiplayer games.


##### nicbase

:   Base address of the NE2000 card (`300` by default).

    !!! note

        Addresses 220 and 240 might not be available as they're assigned to
        the Sound Blaster and Gravis UltraSound by default.


##### nicirq

:   The interrupt used by the NE2000 card (`3` by default).

    !!! note

        IRQs 3 and 5 might not be available as they're assigned to
        `serial2` and the Gravis UltraSound by default.


##### tcp_port_forwards

:   Forward one or more TCP ports from the host into the DOS guest (unset by
    default). The format is:

    - `port1 port2 port3 ...` -- Forward specific ports (e.g., `21 80 443`
      for FTP, HTTP, and HTTPS).
    - `host:guest ...` -- Map host port to guest port (e.g., `8021:21
      8080:80`). Useful for privileged ports.
    - `start-end ...` -- Forward a range of adjacent ports (e.g.,
      `27910-27960`).
    - `hstart-hend:gstart-gend ...` -- Map port ranges (e.g.,
      `8040-8080:20-60`).

    !!! note

        - If mapped ranges differ, the shorter range is extended to fit.
        - If conflicting host ports are given, only the first one is set up.
        - If conflicting guest ports are given, the latter rule takes
          precedence.


##### udp_port_forwards

:   Forward one or more UDP ports from the host into the DOS guest (unset by
    default). The format is the same as for
    [tcp_port_forwards](#tcp_port_forwards).
