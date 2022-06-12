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

// Microsoft Serial Mouse emulation originally written by Jonathan Campbell
// Wheel, Logitech, and Mouse Systems mice added by Roman Standzikowski
// (FeralChild64)

// Reference:
// - https://roborooter.com/post/serial-mice
// - https://www.cpcwiki.eu/index.php/Serial_RS232_Mouse

#include "serialmouse.h"

#include "checks.h"
#include "mouse.h"

CHECK_NARROWING();

static constexpr uint16_t DIV_1200 = 96; // port clock divider for 1200 bauds

CSerialMouse::CSerialMouse(const uint8_t id, CommandLine *cmd)
        : CSerial(id, cmd),
          port_num(static_cast<uint16_t>(id + 1))
{
    std::string type_string;
    bool use_default = !cmd->FindStringBegin("type:", type_string, false);

    if (type_string == "2btn") {
        config_type = MouseType::Microsoft;
    } else if (type_string == "2btn+msm") {
        config_type = MouseType::Microsoft;
        config_auto = true;
    } else if (type_string == "3btn") {
        config_type = MouseType::Logitech;
    } else if (type_string == "3btn+msm") {
        config_type = MouseType::Logitech;
        config_auto = true;
    } else if (type_string == "wheel") {
        config_type = MouseType::Wheel;
    } else if (type_string == "wheel+msm" || use_default) {
        config_type = MouseType::Wheel;
        config_auto = true;
    } else if (type_string == "msm") {
        config_type = MouseType::MouseSystems;
    } else {
        LOG_ERR("MOUSE (COM%d): Invalid type '%s'",
                port_num,
                type_string.c_str());
        return;
    }

    std::string rate_string;
    use_default = !cmd->FindStringBegin("rate:", rate_string, false);

    if (rate_string == "smooth" || use_default) {
        // This is roughly equivalent of PS/2 mouse with sampling rate
        // between 120 and 250 Hz, depends on mouse protocol, concrete
        // events (reporting middle button state change or wheel
        // movement needs more bandwidth for Logitech and wheel mice),
        // and sometimes even driver sophistication (for Mouse Systems
        // mouse a badly written driver can waste half of the movement
        // sampling rate)
        smooth_div = 5;
    } else if (rate_string != "normal") {
        LOG_ERR("MOUSE (COM%d): Invalid rate '%s'",
                port_num,
                rate_string.c_str());
        return;
    }

    SetType(config_type);

    CSerial::Init_Registers();
    setRI(false);
    setDSR(false);
    setCD(false);
    setCTS(false);

    InstallationSuccessful = true;

    MOUSESERIAL_RegisterListener(*this);
}

CSerialMouse::~CSerialMouse()
{
    MOUSESERIAL_UnRegisterListener(*this);
    removeEvent(SERIAL_TX_EVENT); // clear events
    SetType(MouseType::NoMouse);
}

void CSerialMouse::SetType(const MouseType new_type)
{
    if (type != new_type) {
        type             = new_type;
        const char *name = nullptr;
        switch (new_type) {
        case MouseType::NoMouse: // just to print out log in the destructor
            name = "(none)";
            break;
        case MouseType::Microsoft:
            name           = "Microsoft, 2 buttons";
            byte_len       = 7;
            has_3rd_button = false;
            has_wheel      = false;
            break;
        case MouseType::Logitech:
            name           = "Logitech, 3 buttons";
            byte_len       = 7;
            has_3rd_button = true;
            has_wheel      = false;
            break;
        case MouseType::Wheel:
            name           = "wheel, 3 buttons";
            byte_len       = 7;
            has_3rd_button = true;
            has_wheel      = true;
            break;
        case MouseType::MouseSystems:
            name           = "Mouse Systems, 3 buttons";
            byte_len       = 8;
            has_3rd_button = true;
            has_wheel      = false;
            break;
        default: LogUnimplemented(); break;
        }

        if (name)
            LOG_MSG("MOUSE (COM%d): %s", port_num, name);
    }
}

void CSerialMouse::AbortPacket()
{
    packet_len     = 0;
    xmit_idx       = 0xff;
    xmit_2part     = false;
    another_move   = false;
    another_button = false;
}

void CSerialMouse::ClearCounters()
{
    delta_x = 0;
    delta_y = 0;
    delta_w = 0;
}

void CSerialMouse::MouseReset()
{
    AbortPacket();
    ClearCounters();
    buttons  = 0;
    send_ack = true;

    SetEventRX();
}

void CSerialMouse::OnMouseEventMoved(const int16_t new_delta_x,
                                     const int16_t new_delta_y)
{
    delta_x += new_delta_x;
    delta_y += new_delta_y;

    // Initiate data transfer and form the packet to transmit. If another
    // packet is already transmitting now then wait for it to finish before
    // transmitting ours, and let the mouse motion accumulate in the meantime

    if (xmit_idx >= packet_len)
        StartPacketData();
    else
        another_move = true;
}

void CSerialMouse::OnMouseEventButton(const uint8_t new_buttons, const uint8_t idx)
{
    if (!has_3rd_button && idx > 1)
        return;

    buttons = new_buttons;

    if (xmit_idx >= packet_len)
        StartPacketData(idx > 1);
    else
        another_button = true;
}

void CSerialMouse::OnMouseEventWheel(const int8_t new_delta_w)
{
    if (!has_wheel)
        return;

    delta_w += new_delta_w;

    if (xmit_idx >= packet_len)
        StartPacketData(true);
    else
        another_button = true;
}

void CSerialMouse::StartPacketId() // send the mouse identifier
{
    if (!port_valid)
        return;
    AbortPacket();
    ClearCounters();

    packet_len = 0;
    switch (type) {
    case MouseType::Microsoft: packet[packet_len++] = 'M'; break;
    case MouseType::Logitech:
        packet[packet_len++] = 'M';
        packet[packet_len++] = '3';
        break;
    case MouseType::Wheel:
        packet[packet_len++] = 'M';
        packet[packet_len++] = 'Z';
        packet[packet_len++] = '@'; // for some reason 86Box sends more
                                    // than just 'MZ'
        packet[packet_len++] = 0;
        packet[packet_len++] = 0;
        packet[packet_len++] = 0;
        break;
    case MouseType::MouseSystems: packet[packet_len++] = 'H'; break;
    default: LogUnimplemented(); break;
    }

    // send packet
    xmit_idx = 0;
    SetEventRX();
}

void CSerialMouse::StartPacketData(const bool extended)
{
    if (!port_valid)
        return;

    if (type == MouseType::Microsoft || type == MouseType::Logitech ||
        type == MouseType::Wheel) {
        //          -- -- -- -- -- -- -- --
        // Byte 0:   X  1 LB RB Y7 Y6 X7 X6
        // Byte 1:   X  0 X5 X4 X3 X2 X1 X0
        // Byte 2:   X  0 Y5 Y4 Y3 Y2 Y1 Y0
        // Byte 3:   X  0 MB 00 W3 W2 W1 W0  - only sent if needed

        // Do NOT set bit 7. It confuses CTMOUSE.EXE (CuteMouse) serial
        // support. Leaving it clear is the only way to make mouse
        // movement possible. Microsoft Windows on the other hand
        // doesn't care if bit 7 is set.

        const auto dx = ClampDelta(delta_x);
        const auto dy = ClampDelta(delta_y);
        const auto bt = has_3rd_button ? (buttons & 7) : (buttons & 3);

        packet[0] = static_cast<uint8_t>(
                0x40 | ((bt & 1) << 5) | ((bt & 2) << 3) |
                (((dy >> 6) & 3) << 2) | ((dx >> 6) & 3));
        packet[1] = static_cast<uint8_t>(0x00 | (dx & 0x3f));
        packet[2] = static_cast<uint8_t>(0x00 | (dy & 0x3f));
        if (extended) {
            uint8_t dw = std::clamp(delta_w, -0x10, 0x0f) & 0x0f;
            packet[3] = static_cast<uint8_t>(((bt & 4) ? 0x20 : 0) | dw);
            packet_len = 4;
        } else {
            packet_len = 3;
        }
        xmit_2part = false;

    } else if (type == MouseType::MouseSystems) {
        //          -- -- -- -- -- -- -- --
        // Byte 0:   1  0  0  0  0 LB MB RB
        // Byte 1:  X7 X6 X5 X4 X3 X2 X1 X0
        // Byte 2:  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0

        const auto bt = has_3rd_button ? ((~buttons) & 7)
                                       : ((~buttons) & 3);

        packet[0]  = static_cast<uint8_t>(0x80 | ((bt & 1) << 2) |
                                                 ((bt & 2) >> 1) | ((bt & 4) >> 1));
        packet[1]  = ClampDelta(delta_x);
        packet[2]  = ClampDelta(-delta_y);
        packet_len = 3;
        xmit_2part = true; // next part contains mouse movement since
                           // the start of the 1st part

    } else {
        LogUnimplemented();
    }

    ClearCounters();

    // send packet
    xmit_idx       = 0;
    another_button = false;
    another_move   = false;
    SetEventRX();
}

void CSerialMouse::StartPacketPart2()
{
    // port settings are valid at this point

    if (type == MouseType::MouseSystems) {
        //          -- -- -- -- -- -- -- --
        // Byte 3:  X7 X6 X5 X4 X3 X2 X1 X0
        // Byte 4:  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0

        packet[0] = ClampDelta(delta_x);
        packet[1] = ClampDelta(-delta_y);

        packet_len = 2;
        xmit_2part = false;
    } else {
        LogUnimplemented();
    }

    ClearCounters();

    // send packet
    xmit_idx     = 0;
    another_move = false;
    SetEventRX();
}

void CSerialMouse::SetEventTX()
{
    setEvent(SERIAL_TX_EVENT, bytetime / smooth_div);
}

void CSerialMouse::SetEventRX()
{
    setEvent(SERIAL_RX_EVENT, bytetime / smooth_div);
}

void CSerialMouse::SetEventTHR()
{
    setEvent(SERIAL_THR_EVENT, bytetime / 10);
}

void CSerialMouse::LogUnimplemented() const
{
    LOG_ERR("MOUSE (COM%d): Missing implementation", port_num);
}

uint8_t CSerialMouse::ClampDelta(const int32_t delta) const
{
    const auto tmp = std::clamp(delta,
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
                if (xmit_idx >= packet_len && xmit_2part)
                    StartPacketPart2();
                else if (xmit_idx >= packet_len &&
                         (another_move || another_button))
                    StartPacketData();
                else
                    SetEventRX();
            }
        } else
            SetEventRX();
    }
}

void CSerialMouse::updatePortConfig(const uint16_t divider, const uint8_t lcr)
{
    AbortPacket();

    // Check whether port settings match mouse protocol,
    // to prevent false device detections by guest software

    port_valid = true;

    const auto port_byte_len = static_cast<uint8_t>((lcr & 0x3) + 5);
    const auto one_stop_bit  = !(lcr & 0x4);
    const auto parity_id     = static_cast<uint8_t>((lcr & 0x38) >> 3);

    if (divider != DIV_1200 || !one_stop_bit || // for mouse we need 1200
                                                // bauds and 1 stop bit
        parity_id == 1 || parity_id == 3 || parity_id == 5 ||
        parity_id == 7) // parity has to be 'N'
        port_valid = false;

    if (port_valid && config_auto) { // auto select mouse type to emulate
        if (port_byte_len == 7) {
            SetType(config_type);
        } else if (port_byte_len == 8) {
            SetType(MouseType::MouseSystems);
        } else
            port_valid = false;
    } else if (port_byte_len != byte_len) // byte length has to match
                                          // between port and protocol
        port_valid = false;
}

void CSerialMouse::updateMSR() {}

void CSerialMouse::transmitByte(const uint8_t, const bool first)
{
    if (first)
        SetEventTHR();
    else
        SetEventTX();
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
