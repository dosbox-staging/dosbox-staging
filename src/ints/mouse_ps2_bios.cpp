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

#include "mouse.h"
#include "mouse_core.h"

#include <algorithm>
#include <cmath>

#include "bios.h"
#include "bitops.h"
#include "callback.h"
#include "checks.h"
#include "cpu.h"
#include "int10.h"
#include "pic.h"
#include "regs.h"

using namespace bit::literals;

CHECK_NARROWING();

// Here the BIOS abstraction layer for the PS/2 AUX port mouse is implemented.
// PS/2 direct hardware access is not supported yet.

// Reference:
// - https://www.digchip.com/datasheets/parts/datasheet/196/HT82M30A-pdf.php
// - https://isdaman.com/alsos/hardware/mouse/ps2interface.htm
// - https://wiki.osdev.org/Mouse_Input

enum MouseType : uint8_t {   // mouse type visible via PS/2 interface
    None         = 0xff, // dummy, just to trigger a log during startup
    Standard     = 0x00, // standard 2 or 3 button mouse
    IntelliMouse = 0x03, // Microsoft IntelliMouse (3 buttons, wheel)
#ifdef ENABLE_EXPLORER_MOUSE
    Explorer = 0x04, // Microsoft IntelliMouse Explorer (5 buttons, wheel)
#endif // ENABLE_EXPLORER_MOUSE
};

static MouseButtonsAll buttons;     // currently visible button state
static MouseButtonsAll buttons_all; // state of all 5 buttons as on the host side
static MouseButtons12S buttons_12S; // buttons with 3/4/5 quished together

static float delta_x = 0.0f; // accumulated mouse movement since last reported
static float delta_y = 0.0f;
static int8_t wheel  = 0; // NOTE: only fetch using 'GetResetWheel*'!

static MouseType type = MouseType::None; // NOTE: only change using 'SetType'!
static uint8_t unlock_idx_im = 0; // sequence index for unlocking extended protocol
#ifdef ENABLE_EXPLORER_MOUSE
static uint8_t unlock_idx_xp = 0;
#endif // ENABLE_EXPLORER_MOUSE

static uint8_t packet[4] = {0}; // packet to be transferred via BIOS interface

static uint8_t rate_hz = 0;     // maximum rate at which the mouse state is updated
static bool scaling_21 = false; // NOTE: scaling only works for stream mode,
                                // not when reading data manually!
                                // https://www3.tuhh.de/osg/Lehre/SS21/V_BSB/doc/ps2mouse.html

static uint8_t counts_mm = 0;    // counts per mm
static float counts_rate = 0.0f; // 1.0 is 4 counts per mm

// ***************************************************************************
// PS/2 hardware mouse implementation
// ***************************************************************************

void MOUSEPS2_UpdateButtonSquish()
{
    // - if VMware compatible driver is enabled, never try to report
    //   mouse buttons 4 and 5, that would be asking for trouble
    // - for PS/2 modes other than IntelliMouse Explorer there is
    //   no standard way to report buttons 4 and 5

#ifdef ENABLE_EXPLORER_MOUSE
    const bool squish = mouse_shared.active_vmm || (type != MouseType::Explorer);
    buttons.data      = squish ? buttons_12S.data : buttons_all.data;
#else
    buttons.data = buttons_12S.data;
#endif
}

static void TerminateUnlockSequence()
{
    unlock_idx_im = 0;
#ifdef ENABLE_EXPLORER_MOUSE
    unlock_idx_xp = 0;
#endif // ENABLE_EXPLORER_MOUSE
}

static void SetType(const MouseType new_type)
{
    TerminateUnlockSequence();

    if (type != new_type) {
        type                  = new_type;
        const char *type_name = nullptr;
        switch (type) {
        case MouseType::Standard:
            type_name = "Standard, 3 buttons";
            break;
        case MouseType::IntelliMouse:
            type_name = "IntelliMouse, wheel, 3 buttons";
            break;
#ifdef ENABLE_EXPLORER_MOUSE
        case MouseType::Explorer:
            type_name = "IntelliMouse Explorer, wheel, 5 buttons";
            break;
#endif // ENABLE_EXPLORER_MOUSE
        default: break;
        }

        LOG_MSG("MOUSE (PS/2): %s", type_name);

        packet[0] = 0;
        packet[1] = 0;
        packet[2] = 0;
        packet[3] = 0;

        MOUSEPS2_UpdateButtonSquish();
    }
}

#ifdef ENABLE_EXPLORER_MOUSE
static uint8_t GetResetWheel4bit()
{
    const int8_t tmp = std::clamp(wheel,
                                  static_cast<int8_t>(-0x08),
                                  static_cast<int8_t>(0x07));
    wheel            = 0; // reading always clears the wheel counter

    // 0x0f for -1, 0x0e for -2, etc.
    return static_cast<uint8_t>((tmp >= 0) ? tmp : 0x10 + tmp);
}
#endif // ENABLE_EXPLORER_MOUSE

static uint8_t GetResetWheel8bit()
{
    const auto tmp = wheel;
    wheel          = 0; // reading always clears the wheel counter

    // 0xff for -1, 0xfe for -2, etc.
    return static_cast<uint8_t>((tmp >= 0) ? tmp : 0x100 + tmp);
}

static float GetScaledValue(const float x)
{
    if (!scaling_21)
        return x;
    else
        return x * MOUSE_GetBallisticsCoeff(x) * 2.0f;
}

static int16_t GetScaledMovement(const float d)
{
    if (iszero(d))
        return 0;

    const auto tmp = static_cast<int32_t>(std::lround(GetScaledValue(d)));
    if (tmp > 0)
        return static_cast<int16_t>(std::min(tmp, INT16_MAX));
    else
        return static_cast<int16_t>(std::max(tmp, INT16_MIN));
}

static void ResetCounters()
{
    delta_x = 0.0f;
    delta_y = 0.0f;
    wheel   = 0;
}

void MOUSEPS2_UpdatePacket()
{
    union {
        uint8_t data = 0x08;

        bit_view<0, 1> left;
        bit_view<1, 1> right;
        bit_view<2, 1> middle;
        bit_view<4, 1> sign_x;
        bit_view<5, 1> sign_y;
        bit_view<6, 1> overflow_x;
        bit_view<7, 1> overflow_y;
    } mdat;

    mdat.left   = buttons.left;
    mdat.right  = buttons.right;
    mdat.middle = buttons.middle;

    auto dx = static_cast<int16_t>(std::round(delta_x));
    auto dy = static_cast<int16_t>(std::round(delta_y));

    delta_x -= dx;
    delta_y -= dy;

    dx = GetScaledMovement(dx);
    dy = GetScaledMovement(-dy);

#ifdef ENABLE_EXPLORER_MOUSE
    if (type == MouseType::Explorer) {
        // There is no overflow for 5-button mouse protocol, see
        // HT82M30A datasheet
        dx = std::clamp(dx,
                        static_cast<int16_t>(-UINT8_MAX),
                        static_cast<int16_t>(UINT8_MAX));
        dy = std::clamp(dy,
                        static_cast<int16_t>(-UINT8_MAX),
                        static_cast<int16_t>(UINT8_MAX));
    } else
#endif // ENABLE_EXPLORER_MOUSE
    {
        if ((dx > 0xff) || (dx < -0xff))
            mdat.overflow_x = 1;
        if ((dy > 0xff) || (dy < -0xff))
            mdat.overflow_y = 1;
    }

    dx %= 0x100;
    if (dx < 0) {
        dx += 0x100;
        mdat.sign_x = 1;
    }

    dy %= 0x100;
    if (dy < 0) {
        dy += 0x100;
        mdat.sign_y = 1;
    }

    packet[0] = mdat.data;
    packet[1] = static_cast<uint8_t>(dx);
    packet[2] = static_cast<uint8_t>(dy);

    if (type == MouseType::IntelliMouse)
        packet[3] = GetResetWheel8bit();
#ifdef ENABLE_EXPLORER_MOUSE
    else if (type == MouseType::Explorer) {
        packet[3] = GetResetWheel4bit();
        if (buttons.extra_1)
            bit::set(packet[3], b4);
        if (buttons.extra_2)
            bit::set(packet[3], b5);
    }
#endif // ENABLE_EXPLORER_MOUSE
    else
        packet[3] = 0;
}

static void CmdSetResolution(const uint8_t new_counts_mm)
{
    TerminateUnlockSequence();

    if (new_counts_mm != 1 && new_counts_mm != 2 && new_counts_mm != 4 &&
        new_counts_mm != 8)
        // Invalid parameter, set default
        counts_mm = 4;
    else
        counts_mm = new_counts_mm;

    counts_rate = counts_mm / 4.0f;
}

static void CmdSetSampleRate(const uint8_t new_rate_hz)
{
    ResetCounters();

    if (new_rate_hz != 10 && new_rate_hz != 20 && new_rate_hz != 40 &&
        new_rate_hz != 60 && new_rate_hz != 80 && new_rate_hz != 100 &&
        new_rate_hz != 200) {
        // Invalid parameter, set default
        TerminateUnlockSequence();
        rate_hz = 100;
    } else
        rate_hz = new_rate_hz;

    // Update event queue settings
    MOUSE_NotifyRatePS2(rate_hz);

    // Handle extended mouse protocol unlock sequences
    auto unlock = [](const std::vector<uint8_t> &sequence,
                     uint8_t &idx,
                     const MouseType type) {
        if (sequence[idx] != rate_hz)
            idx = 0;
        else if (sequence.size() == ++idx) {
            SetType(type);
        }
    };

    static const std::vector<uint8_t> seq_im = {200, 100, 80};
#ifdef ENABLE_EXPLORER_MOUSE
    static const std::vector<uint8_t> seq_xp = {200, 200, 80};
#endif // ENABLE_EXPLORER_MOUSE

    unlock(seq_im, unlock_idx_im, MouseType::IntelliMouse);
#ifdef ENABLE_EXPLORER_MOUSE
    unlock(seq_xp, unlock_idx_xp, MouseType::Explorer);
#endif // ENABLE_EXPLORER_MOUSE
}

static void CmdSetDefaults()
{
    CmdSetResolution(4);
    CmdSetSampleRate(100);

#ifdef ENABLE_EXPLORER_MOUSE
    MOUSEPS2_UpdateButtonSquish();
#endif // ENABLE_EXPLORER_MOUSE
}

static void CmdReset()
{
    CmdSetDefaults();
    SetType(MouseType::Standard);
    ResetCounters();
}

static void CmdSetScaling21(const bool enable)
{
    TerminateUnlockSequence();

    scaling_21 = enable;
}

bool MOUSEPS2_NotifyMoved(const float x_rel, const float y_rel)
{
    delta_x = MOUSE_ClampRelMov(delta_x + x_rel);
    delta_y = MOUSE_ClampRelMov(delta_y + y_rel);

    return (std::fabs(GetScaledValue(delta_x)) >= 0.5f) ||
           (std::fabs(GetScaledValue(delta_y)) >= 0.5f);
}

bool MOUSEPS2_NotifyPressedReleased(const MouseButtons12S new_buttons_12S,
                                    const MouseButtonsAll new_buttons_all)
{
    const auto buttons_old = buttons;

    buttons_12S = new_buttons_12S;
    buttons_all = new_buttons_all;
    MOUSEPS2_UpdateButtonSquish();

    return (buttons_old.data != buttons.data);
}

bool MOUSEPS2_NotifyWheel(const int16_t w_rel)
{
#ifdef ENABLE_EXPLORER_MOUSE
    if (type != MouseType::IntelliMouse && type != MouseType::Explorer)
        return false;
#else
    if (type != MouseType::IntelliMouse)
        return false;
#endif

    wheel = static_cast<int8_t>(std::clamp(w_rel + wheel, INT8_MIN, INT8_MAX));
    return true;
}

// ***************************************************************************
// BIOS interface implementation
// ***************************************************************************

// TODO: Once the the physical PS/2 mouse is implemented, BIOS has to be changed
// to interact with I/O ports, not to call PS/2 hardware implementation routines
// directly (no Cmd* calls should be present in BIOS) - otherwise the
// complicated Windows 3.x mouse/keyboard support will get confused. See:
// https://www.os2museum.com/wp/jumpy-ps2-mouse-in-enhanced-mode-windows-3-x/
// Other solution might be to put interrupt lines low ion BIOS implementation,
// like this is done in DOSBox X.

static bool packet_4bytes = false;

static bool callback_init    = false;
static uint16_t callback_seg = 0;
static uint16_t callback_ofs = 0;
static RealPt ps2_callback   = 0;

void MOUSEBIOS_Reset()
{
    CmdReset();
    PIC_SetIRQMask(12, false); // lower IRQ line
    MOUSEVMM_Deactivate();     // VBADOS seems to expect this
}

void MOUSEBIOS_SetCallback(const uint16_t pseg, const uint16_t pofs)
{
    if ((pseg == 0) && (pofs == 0)) {
        callback_init = false;
    } else {
        callback_init = true;
        callback_seg  = pseg;
        callback_ofs  = pofs;
    }
}

bool MOUSEBIOS_SetPacketSize(const uint8_t packet_size)
{
    if (packet_size == 3)
        packet_4bytes = false;
    else if (packet_size == 4)
        packet_4bytes = true;
    else
        return false; // unsupported packet size

    return true;
}

bool MOUSEBIOS_SetSampleRate(const uint8_t rate_id)
{
    switch (rate_id) {
    case 0: CmdSetSampleRate(10);  break;
    case 1: CmdSetSampleRate(20);  break;
    case 2: CmdSetSampleRate(40);  break;
    case 3: CmdSetSampleRate(60);  break;
    case 4: CmdSetSampleRate(80);  break;
    case 5: CmdSetSampleRate(100); break;
    case 6: CmdSetSampleRate(200); break;
    default: return false;
    }

    return true;
}

bool MOUSEBIOS_SetResolution(const uint8_t res_id)
{
    switch (res_id) {
    case 0: CmdSetResolution(1); break;
    case 1: CmdSetResolution(2); break;
    case 2: CmdSetResolution(4); break;
    case 3: CmdSetResolution(8); break;
    default: return false;
    }

    return true;
}

void MOUSEBIOS_SetScaling21(const bool enable)
{
    CmdSetScaling21(enable);
}

bool MOUSEBIOS_SetState(const bool use)
{
    if (use && !callback_init) {
        mouse_shared.active_bios = false;
        MOUSE_NotifyStateChanged();
        return false;
    } else {
        mouse_shared.active_bios = use;
        MOUSE_NotifyStateChanged();
        return true;
    }
}

uint8_t MOUSEBIOS_GetResolution()
{
    return counts_mm;
}

uint8_t MOUSEBIOS_GetSampleRate()
{
    return rate_hz;
}

uint8_t MOUSEBIOS_GetStatus()
{
    union {
        uint8_t data = 0;

        bit_view<0, 1> left;
        bit_view<1, 1> right;
        bit_view<2, 1> middle;
        // bit 3 - reserved
        bit_view<4, 1> scaling_21;
        bit_view<5, 1> reporting;
        bit_view<6, 1> mode_remote;
        // bit 7 - reserved
    } ret;

    ret.left   = buttons.left;
    ret.right  = buttons.right;
    ret.middle = buttons.middle;

    ret.scaling_21 = scaling_21;
    ret.reporting  = 1;

    return ret.data;
}

uint8_t MOUSEBIOS_GetType()
{
    return static_cast<uint8_t>(type);
}

static Bitu CallbackRet()
{
    CPU_Pop16();
    CPU_Pop16();
    CPU_Pop16();
    CPU_Pop16(); // remove 4 words
    return CBRET_NONE;
}

Bitu MOUSEBIOS_DoCallback()
{
    if (!packet_4bytes) {
        CPU_Push16(packet[0]);
        CPU_Push16(packet[1]);
        CPU_Push16(packet[2]);
    } else {
        CPU_Push16(static_cast<uint16_t>((packet[0] + packet[1] * 0x100)));
        CPU_Push16(packet[2]);
        CPU_Push16(packet[3]);
    }
    CPU_Push16((uint16_t)0);

    CPU_Push16(RealSeg(ps2_callback));
    CPU_Push16(RealOff(ps2_callback));
    SegSet16(cs, callback_seg);
    reg_ip = callback_ofs;

    return CBRET_NONE;
}

void MOUSEPS2_Init()
{
    // Callback for ps2 user callback handling
    auto call_ps2 = CALLBACK_Allocate();
    CALLBACK_Setup(call_ps2, &CallbackRet, CB_RETF, "ps2 bios callback");
    ps2_callback = CALLBACK_RealPointer(call_ps2);

    MOUSEBIOS_Reset();
}
