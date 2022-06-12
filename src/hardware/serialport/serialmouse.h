/*
 *  Copyright (C) 2022       The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_SERIALMOUSE_H
#define DOSBOX_SERIALMOUSE_H

#include "serialport.h"


class CSerialMouse : public CSerial {
public:
	CSerialMouse(const uint8_t id, CommandLine *cmd);
	virtual ~CSerialMouse();

	void OnMouseEventMoved(const int16_t new_delta_x, const int16_t new_delta_y);
	void OnMouseEventButton(const uint8_t new_buttons,
	                        const uint8_t idx); // idx - index of changed
	                                            // button, staring from 0
	void OnMouseEventWheel(const int8_t new_delta_w);

	void setRTSDTR(const bool rts, const bool dtr) override;
	void setRTS(const bool val) override;
	void setDTR(const bool val) override;

	void updatePortConfig(const uint16_t divider, const uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(const uint8_t val, const bool first) override;
	void setBreak(const bool value) override;
	void handleUpperEvent(const uint16_t event_type) override;

private:
	enum class MouseType {
		NoMouse,
		Microsoft,
		Logitech,
		Wheel,
		MouseSystems
	};

	void SetType(const MouseType new_type);
	void AbortPacket();
	void ClearCounters();
	void MouseReset();
	void StartPacketId();
	void StartPacketData(const bool extended = false);
	void StartPacketPart2();
	void SetEventTX();
	void SetEventRX();
	void SetEventTHR();
	void LogUnimplemented() const;
	uint8_t ClampDelta(const int32_t delta) const;

	const uint16_t port_num = 0;

	MouseType config_type = MouseType::NoMouse; // mouse type as in the
	                                            // configuration file
	bool config_auto = false; // true = autoswitch between config_type and
	                          // Mouse Systems Mouse

	MouseType type   = MouseType::NoMouse; // currently emulated mouse type
	uint8_t byte_len = 0; // how many bits the emulated mouse transmits in a
	                      // byte (serial port setting)
	bool has_3rd_button = false;
	bool has_wheel      = false;
	bool port_valid     = false; // false = port settings incompatible with
	                             // selected mouse
	uint8_t smooth_div = 1; // time divider value, if > 1 mouse is more
	                        // smooth than with real HW
	bool send_ack      = true;
	uint8_t packet[6]  = {};
	uint8_t packet_len = 0;
	uint8_t xmit_idx = UINT8_MAX; // index of byte to send, if >= packet_len
	                              // it means transmission ended
	bool xmit_2part = false; // true = packet has a second part, which could
	                         // not be evaluated yet
	bool another_move = false; // true = while transmitting a packet we
	                           // received mouse move event
	bool another_button = false; // true = while transmitting a packet we
	                             // received mouse button event
	uint8_t buttons = 0; // bit 0 = left, bit 1 = right, bit 2 = middle
	int32_t delta_x = 0; // movement since last transmitted package
	int32_t delta_y = 0;
	int32_t delta_w = 0;
};

#endif // DOSBOX_SERIALMOUSE_H
