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
    CSerialMouse(uintptr_t id, CommandLine* cmd);
    virtual ~CSerialMouse();

    void onMouseEventMoved(int16_t delta_x, int16_t delta_y);
    void onMouseEventButton(uint8_t buttons, uint8_t idx); // idx - index of changed button, staring from 0
    void onMouseEventWheel(int8_t delta_w);

    void setRTSDTR(bool rts, bool dtr);
    void setRTS(bool val);
    void setDTR(bool val);

    void updatePortConfig(uint16_t divider, uint8_t lcr);
    void updateMSR();
    void transmitByte(uint8_t val, bool first);
    void setBreak(bool value);
    void handleUpperEvent(uint16_t type);

private:

    enum MouseType {
        NO_MOUSE,
        MICROSOFT,
        LOGITECH,
        WHEEL,
        MOUSE_SYSTEMS
    };

    void setType(MouseType type);
    void abortPacket();
    void clearCounters();
    void mouseReset();
    void startPacketId();
    void startPacketData(bool extended = false);
    void startPacketPart2();
    void unimplemented();

    const int port_num;

    MouseType config_type;         // mouse type as in the configuration file
    bool      config_auto;         // true = autoswitch between config_type and Mouse Systems Mouse

    MouseType mouse_type;          // currently emulated mouse type
    uint8_t   mouse_bytelen;
    bool      mouse_has_3rd_button;
    bool      mouse_has_wheel;
    bool      mouse_port_valid;    // false = port settings incompatible with selected mouse
    uint8_t   smooth_div;

    bool      send_ack;
    uint8_t   packet[6] = {};
    uint8_t   packet_len;
    uint8_t   xmit_idx;            // index of byte to send, if >= packet_len it means transmission ended
    bool      xmit_2part;          // true = packet has a second part, which could not be evaluated yet
    bool      xmit_another_move;   // true = while transmitting a packet we received mouse move event
    bool      xmit_another_button; // true = while transmitting a packet we received mouse button event
    uint8_t   mouse_buttons;       // bit 0 = left, bit 1 = right, bit 2 = middle
    int32_t   mouse_delta_x, mouse_delta_y, mouse_delta_w;
};

#endif // DOSBOX_SERIALMOUSE_H
