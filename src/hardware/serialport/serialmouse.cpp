// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Microsoft Serial Mouse emulation originally written by Jonathan Campbell
// Wheel, Logitech, and Mouse Systems mice added by Roman Standzikowski
// (FeralChild64)

// Reference:
// - https://roborooter.com/post/serial-mice
// - https://www.cpcwiki.eu/index.php/Serial_RS232_Mouse

#include "serialmouse.h"

#include "utils/checks.h"
#include "shell/command_line.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

// Port clock divider for 1200 baud transmission
static constexpr uint16_t divider_1200_baud = 96;
// 1200 baud serial mice is limited to about 40 Hz sampling rate
// due to serial port transmission constraints
static constexpr uint16_t rate_1200_baud = 40;

CSerialMouse::CSerialMouse(const uint8_t id, CommandLine *cmd)
        : CSerial(id, cmd),
          port_num(id + 1),
          interface_id(static_cast<MouseInterfaceId>(enum_val(MouseInterfaceId::COM1) + id))
{
	if (interface_id != MouseInterfaceId::COM1 &&
	    interface_id != MouseInterfaceId::COM2 &&
	    interface_id != MouseInterfaceId::COM3 &&
	    interface_id != MouseInterfaceId::COM4) {

		LOG_ERR("MOUSE (COM%d): Port not supported", port_num);
		return;
	}

	// Get the parameters from the configuration file

	param_model    = MOUSECOM_GetConfiguredModel();
	param_auto_msm = MOUSECOM_GetConfiguredAutoMsm();

	// Handle deprecated parameters

	HandleDeprecatedOptions(cmd);

	// Override with parameters from command line or [serial] section

	std::string model_string = {};
	if (cmd->FindStringBegin("model:", model_string, false) &&
	    !MOUSECOM_ParseComModel(model_string, param_model, param_auto_msm)) {

		LOG_ERR("MOUSE (COM%d): Invalid model '%s'",
		        port_num,
		        model_string.c_str());
	}

	CSerial::Init_Registers();
	setRI(false);
	setDSR(false);
	setCD(false);
	setCTS(false);

	MOUSECOM_RegisterListener(interface_id, *this);
	MOUSECOM_NotifyInterfaceRate(interface_id, rate_1200_baud);

	InstallationSuccessful = true;
}

CSerialMouse::~CSerialMouse()
{
	MOUSECOM_UnregisterListener(interface_id);

	removeEvent(SERIAL_TX_EVENT); // clear events
	LOG_MSG("MOUSE (COM%d): Disconnected", port_num);
}

void CSerialMouse::HandleDeprecatedOptions(CommandLine *cmd)
{
	using enum MouseModelCom;

	std::string option;
	if (cmd->FindStringBeginCaseSensitive("rate:", option, false))
		LOG_WARNING("MOUSE (COM%d): Deprecated option 'rate:' - ignored",
		            port_num);

	const bool found_deprecated = cmd->FindStringBeginCaseSensitive("type:", option, false);

	if (found_deprecated) {
		LOG_WARNING("MOUSE (COM%d): Deprecated option 'type:'", port_num);

		if (option == "2btn") {
			param_model    = Microsoft;
			param_auto_msm = false;
		} else if (option == "2btn+msm") {
			param_model    = Microsoft;
			param_auto_msm = true;
		} else if (option == "3btn") {
			param_model    = Logitech;
			param_auto_msm = false;
		} else if (option == "3btn+msm") {
			param_model    = Logitech;
			param_auto_msm = true;
		} else if (option == "wheel") {
			param_model    = Wheel;
			param_auto_msm = false;
		} else if (option == "wheel+msm") {
			param_model    = Wheel;
			param_auto_msm = true;
		} else if (option == "msm") {
			param_model    = MouseSystems;
			param_auto_msm = false;
		} else {
			LOG_ERR("MOUSE (COM%d): Invalid type '%s'",
			        port_num,
			        option.c_str());
			return;
		}
	}
}

void CSerialMouse::BoostRate(const uint16_t rate_hz)
{
	using enum MouseModelCom;

	if (!rate_hz || model == NoMouse) {
		rate_coeff = 1.0f;
		return;
	}

	// Estimate current sampling rate, as precisely as possible
	auto estimate = [this](const uint16_t bauds) {
		// In addition to byte_len, the mouse has to send
		// 3 more bits per each byte: start, parity, stop

		if (model == Microsoft || model == Logitech ||  model == Wheel) {
			// Microsoft-style protocol
			// single movement needs exactly 3 bytes to be reported
			return bauds / (static_cast<float>(port_byte_len + 3) * 3.0f);
		} else if (model == MouseSystems) {
			// Mouse Systems protocol
			// single movement needs per average 2.5 bytes to be
			// reported
			return bauds / (static_cast<float>(port_byte_len + 3) * 2.5f);
		}

		assert(false); // unimplemented
		return static_cast<float>(rate_1200_baud);
	};

	// Calculate coefficient to match requested rate
	rate_coeff = estimate(1200) / rate_hz;
}

void CSerialMouse::LogMouseModel()
{
	using enum MouseModelCom;

	std::string model_name = {};
	switch (model) {
	case Microsoft:
		model_name     = "2 buttons (Microsoft)";
		has_3rd_button = false;
		has_wheel      = false;
		break;
	case Logitech:
		model_name     = "3 buttons (Logitech)";
		has_3rd_button = true;
		has_wheel      = false;
		break;
	case Wheel:
		model_name     = "3 buttons + wheel";
		has_3rd_button = true;
		has_wheel      = true;
		break;
	case MouseSystems:
		model_name     = "3 buttons (Mouse Systems)";
		has_3rd_button = true;
		has_wheel      = false;
		break;
	case NoMouse:
		LOG_MSG("MOUSE (COM%d): Disabled", port_num);
		break;
	default:
		assertm(false, "unknown mouse model (COM)");
		break;
	}

	if (!model_name.empty()) {
		LOG_MSG("MOUSE (COM%d): Using a %s model protocol", port_num,
		        model_name.c_str());
	}
}

void CSerialMouse::SetModel(const MouseModelCom new_model)
{
	if (model != new_model) {
		model = new_model;
		
		LogMouseModel();
	}

	// So far all emulated mice are 1200 bauds, but report anyway
	// to trigger rate_coeff recalculation
	MOUSECOM_NotifyInterfaceRate(interface_id, rate_1200_baud);
}

void CSerialMouse::AbortPacket()
{
	packet_len         = 0;
	xmit_idx           = 0xff;
	need_xmit_part2    = false;
	got_another_move   = false;
	got_another_button = false;
}

void CSerialMouse::ClearCounters()
{
	counter_x     = 0;
	counter_y     = 0;
	counter_wheel = 0;
}

void CSerialMouse::MouseReset()
{
	AbortPacket();
	ClearCounters();
	buttons  = 0;
	send_ack = true;

	SetEventRX();
}

void CSerialMouse::NotifyMoved(const float x_rel, const float y_rel)
{
	delta_x = MOUSE_ClampRelativeMovement(delta_x + x_rel);
	delta_y = MOUSE_ClampRelativeMovement(delta_y + y_rel);

	if (!MOUSE_HasAccumulatedInt(delta_x) && !MOUSE_HasAccumulatedInt(delta_y)) {
		return; // movement not significant enough
	}

	counter_x = clamp_to_int8(counter_x + MOUSE_ConsumeInt16(delta_x));
	counter_y = clamp_to_int8(counter_y + MOUSE_ConsumeInt16(delta_y));

	// Initiate data transfer and form the packet to transmit. If another
	// packet is already transmitting now then wait for it to finish before
	// transmitting ours, and let the mouse motion accumulate in the meantime

	if (xmit_idx >= packet_len) {
		StartPacketData();
	} else {
		got_another_move = true;
	}
}

void CSerialMouse::NotifyButton(const uint8_t new_buttons, const MouseButtonId button_id)
{
	const auto is_middle_or_greater = (button_id >= MouseButtonId::Middle);

	if (!has_3rd_button && is_middle_or_greater) {
		return;
	}

	buttons = new_buttons;

	if (xmit_idx >= packet_len) {
		StartPacketData(is_middle_or_greater);
	} else {
		got_another_button = true;
	}
}

void CSerialMouse::NotifyWheel(const float w_rel)
{
	if (!has_wheel) {
		return;
	}

	delta_wheel = MOUSE_ClampWheelMovement(delta_wheel + w_rel);
	if (!MOUSE_HasAccumulatedInt(delta_wheel)) {
		return; // movement not significant enough
	}

	counter_wheel = clamp_to_int8(counter_wheel + MOUSE_ConsumeInt16(delta_wheel));

	if (xmit_idx >= packet_len) {
		StartPacketData(true);
	} else {
		got_another_move = true;
	}
}

void CSerialMouse::StartPacketId() // send the mouse identifier
{
	using enum MouseModelCom;

	if (model == NoMouse) {
		return;
	}

	AbortPacket();
	ClearCounters();

	packet_len = 0;
	switch (model) {
	case Microsoft: packet[packet_len++] = 'M'; break;
	case Logitech:
		packet[packet_len++] = 'M';
		packet[packet_len++] = '3';
		break;
	case Wheel:
		packet[packet_len++] = 'M';
		packet[packet_len++] = 'Z';
		packet[packet_len++] = '@'; // for some reason 86Box sends more
		                            // than just 'MZ'
		packet[packet_len++] = 0;
		packet[packet_len++] = 0;
		packet[packet_len++] = 0;
		break;
	case MouseSystems: packet[packet_len++] = 'H'; break;
	default:
		assert(false); // unimplemented
		break;
	}

	// send packet
	xmit_idx = 0;
	SetEventRX();
}

void CSerialMouse::StartPacketData(const bool extended)
{
	using enum MouseModelCom;

	if (model == NoMouse) {
		return;
	}

	if (model == Microsoft || model == Logitech || model == Wheel) {
		//          -- -- -- -- -- -- -- --
		// Byte 0:   X  1 LB RB Y7 Y6 X7 X6
		// Byte 1:   X  0 X5 X4 X3 X2 X1 X0
		// Byte 2:   X  0 Y5 Y4 Y3 Y2 Y1 Y0
		// Byte 3:   X  0 MB 00 W3 W2 W1 W0  - only sent if needed

		// Do NOT set bit 7. It confuses CTMOUSE.EXE (CuteMouse) serial
		// support. Leaving it clear is the only way to make mouse
		// movement possible. Microsoft Windows on the other hand
		// doesn't care if bit 7 is set.

		const auto dx = ClampCounter(counter_x);
		const auto dy = ClampCounter(counter_y);
		const auto bt = has_3rd_button ? (buttons & 7) : (buttons & 3);

		packet[0] = static_cast<uint8_t>(
		        0x40 | ((bt & 1) << 5) | ((bt & 2) << 3) |
		        (((dy >> 6) & 3) << 2) | ((dx >> 6) & 3));
		packet[1] = static_cast<uint8_t>(0x00 | (dx & 0x3f));
		packet[2] = static_cast<uint8_t>(0x00 | (dy & 0x3f));
		if (extended) {
			uint8_t dw = std::clamp(counter_wheel,
			                        static_cast<int8_t>(-0x10),
			                        static_cast<int8_t>(0x0f)) &
			             0x0f;
			packet[3] = static_cast<uint8_t>(((bt & 4) ? 0x20 : 0) | dw);
			packet_len = 4;
		} else {
			packet_len = 3;
		}
		need_xmit_part2 = false;

	} else if (model == MouseSystems) {
		//          -- -- -- -- -- -- -- --
		// Byte 0:   1  0  0  0  0 LB MB RB
		// Byte 1:  X7 X6 X5 X4 X3 X2 X1 X0
		// Byte 2:  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0

		const auto bt = has_3rd_button ? ((~buttons) & 7)
		                               : ((~buttons) & 3);

		packet[0]  = static_cast<uint8_t>(0x80 | ((bt & 1) << 2) |
                                                 ((bt & 2) >> 1) | ((bt & 4) >> 1));
		packet[1]  = ClampCounter(counter_x);
		packet[2]  = ClampCounter(-counter_y);
		packet_len = 3;

		need_xmit_part2 = true; // next part contains mouse movement
		                        // since the start of the 1st part
	} else {
		assert(false); // unimplemented
	}

	ClearCounters();

	// send packet
	xmit_idx           = 0;
	got_another_button = false;
	got_another_move   = false;
	SetEventRX();
}

void CSerialMouse::StartPacketPart2()
{
	// Port settings are valid at this point
	if (model == MouseModelCom::MouseSystems) {
		//          -- -- -- -- -- -- -- --
		// Byte 3:  X7 X6 X5 X4 X3 X2 X1 X0
		// Byte 4:  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0

		packet[0]  = ClampCounter(counter_x);
		packet[1]  = ClampCounter(-counter_y);
		packet_len = 2;

		need_xmit_part2 = false;
	} else {
		assert(false); // unimplemented
	}

	ClearCounters();

	// send packet
	xmit_idx         = 0;
	got_another_move = false;
	SetEventRX();
}

void CSerialMouse::SetEventTX()
{
	setEvent(SERIAL_TX_EVENT, bytetime * rate_coeff);
}

void CSerialMouse::SetEventRX()
{
	setEvent(SERIAL_RX_EVENT, bytetime * rate_coeff);
}

void CSerialMouse::SetEventTHR()
{
	setEvent(SERIAL_THR_EVENT, bytetime / 10);
}

uint8_t CSerialMouse::ClampCounter(const int32_t counter) const
{
	const auto tmp = std::clamp(counter,
	                            static_cast<int32_t>(INT8_MIN),
	                            static_cast<int32_t>(INT8_MAX));
	return static_cast<uint8_t>(tmp);
}

void CSerialMouse::handleUpperEvent(const uint16_t event_type)
{
	if (event_type == SERIAL_TX_EVENT) {
		ByteTransmitted(); // tx timeout
	} else if (event_type == SERIAL_THR_EVENT) {
		ByteTransmitting();
		SetEventTX();
	} else if (event_type == SERIAL_RX_EVENT) {
		// check for bytes to be sent to port
		if (CSerial::CanReceiveByte()) {
			if (send_ack) {
				send_ack = false;
				StartPacketId();
			} else if (xmit_idx < packet_len) {
				CSerial::receiveByte(packet[xmit_idx++]);
				if (xmit_idx >= packet_len && need_xmit_part2) {
					StartPacketPart2();
				} else if (xmit_idx >= packet_len &&
				           (got_another_move || got_another_button)) {
					StartPacketData();
				} else {
					SetEventRX();
				}
			}
		} else {
			SetEventRX();
		}
	}
}

void CSerialMouse::updatePortConfig(const uint16_t divider, const uint8_t lcr)
{
	using enum MouseModelCom;

	AbortPacket();

	// We have to select between Microsoft-style protocol (this includes
	// Logitech and wheel mice) and Mouse Systems Mouse protocol, or decide
	// the port settings are not valid for any mouse

	port_byte_len           = static_cast<uint8_t>((lcr & 0x3) + 5);
	const auto one_stop_bit = !(lcr & 0x4);
	const auto parity_id    = static_cast<uint8_t>((lcr & 0x38) >> 3);

	// LOG_MSG("MOUSE (COM%d): lcr 0x%04x, divider %d, byte_len %d, stop %d,
	// parity %d",
	//         port_num, lcr, divider, port_byte_len, one_stop_bit, parity_id);

	if (divider != divider_1200_baud) {
		// We need 1200 bauds for a mouse; TODO:support faster serial
		// mice, see https://man7.org/linux/man-pages/man4/mouse.4.html
		SetModel(NoMouse);
		return;
	}

	// Require 1 sop bit
	if (!one_stop_bit) {
		SetModel(NoMouse);
		return;
	}

	// Require parity 'N'
	if (parity_id == 1 || parity_id == 3 || parity_id == 5 || parity_id == 7) {
		SetModel(NoMouse);
		return;
	}

	// Check protocol compatibility with byte length
	bool ok_microsoft     = (param_model != MouseSystems);
	bool ok_mouse_systems = param_auto_msm || (param_model == MouseSystems);

	// NOTE: It seems some software (at least The Settlers) tries to use
	// Microsoft-style protocol by setting port to 8 bits per byte;
	// we allow this if autodetection is not enabled, otherwise it is
	// impossible to guess which protocol the guest software expects

	if (port_byte_len != 7 && !(!param_auto_msm && port_byte_len == 8)) {
		ok_microsoft = false;
	}
	if (port_byte_len != 8) {
		ok_mouse_systems = false;
	}

	// Set the mouse protocol
	if (ok_microsoft) {
		SetModel(param_model);
	} else if (ok_mouse_systems) {
		SetModel(MouseSystems);
	} else {
		SetModel(NoMouse);
	}
}

void CSerialMouse::updateMSR() {}

void CSerialMouse::transmitByte(const uint8_t, const bool first)
{
	if (first) {
		SetEventTHR();
	} else {
		SetEventTX();
	}
}

void CSerialMouse::setBreak(const bool) {}

void CSerialMouse::setRTSDTR(const bool rts, const bool dtr)
{
	if (rts && dtr && !getRTS() && !getDTR()) {
		// The serial mouse driver turns on the mouse by bringing up
		// RTS and DTR. Not just for show, but to give the serial mouse
		// a power source to work from. Likewise, drivers "reset" the
		// mouse by bringing down the lines, then bringing them back
		// up. And most drivers turn off the mouse when not in use by
		// bringing them back down and leaving them that way.
		//
		// We're expected to transmit ASCII character 'M' when first
		// initialized, so that the driver knows we're a Microsoft
		// compatible serial mouse attached to a COM port.

		MouseReset();
	}

	setRTS(rts);
	setDTR(dtr);
}

void CSerialMouse::setRTS(const bool val)
{
	if (val && !getRTS() && getDTR()) {
		MouseReset();
	}

	setCTS(val);
}

void CSerialMouse::setDTR(const bool val)
{
	if (val && !getDTR() && getRTS()) {
		MouseReset();
	}

	setDSR(val);
	setRI(val);
	setCD(val);
}
