// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ETHERNET_H
#define DOSBOX_ETHERNET_H

#include "dosbox.h"

#include <functional>
#include <string>

#include "dosbox_config.h"

/** A virtual Ethernet connection
 * While emulated Ethernet adapters provide the ability for the guest OS to
 * send and receive Ethernet packets, the emulator itself needs to pass these
 * packets to device on the host system.
 * The EthernetConnection class provides a virtual connection for passing
 * packets to and from a backend, intended for use by emulated adapter code.
 */
class EthernetConnection {
public:
	/** Initializes the connection.
	 * After creating an EthernetConnection, it must have its connection
	 * opened and initialized before further use.
	 * This is done in a separate function instead of a constructor so any
	 * error code may be propagated.
	 * Do not call this function twice.
	 * @param config The connection's configuration section
	 * @return True if the connection is initialized, false otherwise
	 */
	virtual bool Initialize(Section *config) = 0;

	/** Closes the connection.
	 * Owners of an EthernetConnection are expected to delete it when
	 * finished with the connection.
	 * The destructor will close the connection and free any
	 * per-connection resources.
	 */
	virtual ~EthernetConnection() {}

	/** Sends a packet through the connection.
	 * This function makes no guarantees as to whether the packet actually
	 * gets stored or sent anywhere. Errors are not propagated from
	 * the concrete implementation of the connection.
	 * This matches the lossy nature of sending packets through hardware.
	 * @param packet A pointer to bytes of an Ethernet frame
	 * @param size The size (in bytes) of the Ethernet frame
	 */
	virtual void SendPacket(const uint8_t *packet, int size) = 0;

	/** Gets all pending packets from the connection.
	 * This function passes each pending packet to the callback function.
	 * The callback provides a pointer to the bytes of an Ethernet frame,
	 * and the size (in bytes) of the Ethernet frame.
	 * Using a lambda for std::function allows easy looping over GetPackets
	 * instead of jumping around between functions and sharing global state.
	 * This function does not define the state of the packet data outside
	 * the callback. Copy the packet data if you need to use it later.
	 * @param callback The function called for each pending packet
	 */
	virtual void GetPackets(std::function<int(const uint8_t *, int)> callback) = 0;
};

/** Opens a virtual Ethernet connection to a backend.
 * This function will try to create a new EthernetConnection based on whichever
 * implementation is most appropriate for the backend requested.
 * Each connection returned acts independently of other connections and is
 * suitable for use with multiple network adapters.
 * It will attempt to initialize the connection and return a pointer to it.
 * On failure (whether after creating a connection or if no backend is found)
 * this function will clean up after itself and return nullptr.
 * @param backend The name of the connection backend
 * @return An initialized Ethernet connection, nullptr otherwise
 */
EthernetConnection *ETHERNET_OpenConnection(const std::string &backend);

#endif
