// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SERIALMOUSE_H
#define DOSBOX_SERIALMOUSE_H

#include "serialport.h"

#include "hardware/input/mouse.h"

class CSerialMouse : public CSerial {
public:
	CSerialMouse(const uint8_t id, CommandLine *cmd);
	~CSerialMouse() override;

	void NotifyMoved(const float x_rel, const float y_rel);
	void NotifyButton(const uint8_t new_buttons,
	                  const MouseButtonId id); // changed button
	void NotifyWheel(const float w_rel);

	void BoostRate(const uint16_t rate_hz); // 0 = standard rate

	void setRTSDTR(const bool rts, const bool dtr) override;
	void setRTS(const bool val) override;
	void setDTR(const bool val) override;

	void updatePortConfig(const uint16_t divider, const uint8_t lcr) override;
	void updateMSR() override;
	void transmitByte(const uint8_t val, const bool first) override;
	void setBreak(const bool value) override;
	void handleUpperEvent(const uint16_t event_type) override;

private:
	void LogMouseModel();

	void HandleDeprecatedOptions(CommandLine* cmd);
	void SetModel(const MouseModelCom new_type);
	void AbortPacket();
	void ClearCounters();
	void MouseReset();
	void StartPacketId();
	void StartPacketData(const bool extended = false);
	void StartPacketPart2();
	void SetEventTX();
	void SetEventRX();
	void SetEventTHR();
	uint8_t ClampCounter(const int32_t counter) const;

	const int port_num = 0; // mainly for logging purposes

	const MouseInterfaceId interface_id = {};

	// Mouse model as specified in the parameter
	MouseModelCom param_model = MouseModelCom::NoMouse;
	// If true = autoswitch between param_model and Mouse Systems mouse
	bool param_auto_msm = false;

	MouseModelCom model = MouseModelCom::NoMouse; // currently emulated model

	uint8_t port_byte_len = 0; // how many bits the port transmits in a byte

	bool has_3rd_button = false;
	bool has_wheel      = false;
	float rate_coeff    = 1.0f; // coefficient for boosted sampling rate
	bool send_ack       = true;
	uint8_t packet[6]   = {};
	uint8_t packet_len  = 0;
	uint8_t xmit_idx = UINT8_MAX;    // index of byte to send, if >= packet_len
	                                 // it means transmission ended
	bool need_xmit_part2 = false;    // true = packet has a second part, which
	                                 // could not be evaluated yet
	bool got_another_move = false;   // true = while transmitting a packet we
	                                 // received mouse move event
	bool got_another_button = false; // true = while transmitting a packet
	                                 // we received mouse button event
	uint8_t buttons = 0; // bit 0 = left, bit 1 = right, bit 2 = middle

	// Accumulated mouse movement, waiting to be reported
	float delta_x     = 0.0f;
	float delta_y     = 0.0f;
	float delta_wheel = 0.0f;

	// Position counters, as visible on the guest size
	int8_t counter_x     = 0;
	int8_t counter_y     = 0;
	int8_t counter_wheel = 0;
};

#endif // DOSBOX_SERIALMOUSE_H
