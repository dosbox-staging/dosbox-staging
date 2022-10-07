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
#include "mouse_config.h"
#include "mouse_interfaces.h"
#include "mouse_manymouse.h"
#include "mouse_queue.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "callback.h"
#include "checks.h"
#include "cpu.h"
#include "pic.h"
#include "video.h"

CHECK_NARROWING();


bool mouse_seamless_driver = false;

static Bitu int74_ret_callback = 0;

static MouseQueue    &queue     = MouseQueue::GetInstance();
static ManyMouseGlue &manymouse = ManyMouseGlue::GetInstance();

// ***************************************************************************
// Interrupt 74 implementation
// ***************************************************************************

static Bitu int74_exit()
{
    SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
    reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));

    return CBRET_NONE;
}

static Bitu int74_handler()
{
    MouseEvent ev;
    queue.FetchEvent(ev);

    // Handle DOS events
    if (ev.request_dos) {

        uint8_t mask = 0;
        if (ev.dos_moved) {
            mask = MOUSEDOS_UpdateMoved();

            // Taken from DOSBox X: HERE within the IRQ 12 handler is the
            // appropriate place to redraw the cursor. OSes like Windows 3.1
            // expect real-mode code to do it in response to IRQ 12, not
            // "out of the blue" from the SDL event handler like the
            // original DOSBox code did it. Doing this allows the INT 33h
            // emulation to draw the cursor while not causing Windows 3.1 to
            // crash or behave erratically.
            if (mask)
                MOUSEDOS_DrawCursor();
        }
        if (ev.dos_button)
            mask = static_cast<uint8_t>(mask | MOUSEDOS_UpdateButtons(ev.dos_buttons));
        if (ev.dos_wheel)
            mask = static_cast<uint8_t>(mask | MOUSEDOS_UpdateWheel());

        // If DOS driver's client is not interested in this particular
        // type of event - skip it
        if (!MOUSEDOS_HasCallback(mask))
            return int74_exit();

        CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
        CPU_Push16(RealOff(static_cast<RealPt>(CALLBACK_RealPointer(int74_ret_callback)) + 7));

        return MOUSEDOS_DoCallback(mask, ev.dos_buttons);
    }

    // Handle PS/2 and BIOS mouse events
    if (ev.request_ps2 && mouse_shared.active_bios) {
        CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
        CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));

        MOUSEPS2_UpdatePacket();
        return MOUSEBIOS_DoCallback();
    }

    // No mouse emulation module is interested in event
    return int74_exit();
}

Bitu int74_ret_handler()
{
    queue.StartTimerIfNeeded();
    return CBRET_NONE;
}

// ***************************************************************************
// External notifications
// ***************************************************************************

void MOUSE_NewScreenParams(const uint16_t clip_x, const uint16_t clip_y,
                           const uint16_t res_x, const uint16_t res_y,
                           const bool fullscreen, const uint16_t x_abs,
                           const uint16_t y_abs)
{
    mouse_video.clip_x = clip_x;
    mouse_video.clip_y = clip_y;

    // Protection against strange window sizes,
    // to prevent division by 0 in some places
    mouse_video.res_x = std::max(res_x, static_cast<uint16_t>(2));
    mouse_video.res_y = std::max(res_y, static_cast<uint16_t>(2));

    mouse_video.fullscreen = fullscreen;

    MOUSEVMM_NewScreenParams(x_abs, y_abs);
    MOUSE_NotifyStateChanged();
}

void MOUSE_NotifyResetDOS()
{
    queue.ClearEventsDOS();
}

void MOUSE_NotifyStateChanged()
{
    const auto old_seamless_driver  = mouse_seamless_driver;

    // Prepare suggestions to the GUI
    mouse_seamless_driver = mouse_shared.active_vmm && !mouse_video.fullscreen;

    // If state has really changed, update the GUI
    if (mouse_seamless_driver != old_seamless_driver)
        GFX_UpdateMouseState();
}

void MOUSE_NotifyDisconnect(const MouseInterfaceId interface_id)
{
    auto interface = MouseInterface::Get(interface_id);
    if (interface)
        interface->NotifyDisconnect();
}

void MOUSE_NotifyFakePS2()
{
    auto interface = MouseInterface::GetPS2();

    if (interface && interface->IsUsingEvents()) {
        MouseEvent ev;
        ev.request_ps2 = true;
        queue.AddEvent(ev);
    }
}

void MOUSE_NotifyBooting()
{
    for (auto &interface : mouse_interfaces)
        interface->NotifyBooting();
}


void MOUSE_EventMoved(const float x_rel,
                      const float y_rel,
                      const uint16_t x_abs,
                      const uint16_t y_abs)
{
    // From the GUI we are getting mouse movement data in two
    // distinct formats:
    //
    // - relative; this one has a chance to be raw movements,
    //   it has to be fed to PS/2 mouse emulation, serial port
    //   mouse emulation, etc.; any guest side software accessing
    //   these mouse interfaces will most likely implement it's
    //   own mouse acceleration/smoothing/etc.
    // - absolute; this follows host OS mouse behavior and should
    //   be fed to VMware seamless mouse emulation and similar
    //   interfaces
    //
    // Our DOS mouse driver (INT 33h) is a bit special, as it can
    // act both ways (seamless and non-seamless mouse pointer),
    // so it needs data in both formats.

    // Notify mouse interfaces

    MouseEvent ev;
    for (auto &interface : mouse_interfaces)
        if (interface->IsUsingHostPointer())
            interface->NotifyMoved(ev, x_rel, y_rel, x_abs, y_abs);
    queue.AddEvent(ev);
}

void MOUSE_EventMoved(const float x_rel,
                      const float y_rel,
                      const MouseInterfaceId interface_id)
{
    auto interface = MouseInterface::Get(interface_id);
    if (interface && interface->IsUsingEvents()) {
        MouseEvent ev;
        interface->NotifyMoved(ev, x_rel, y_rel, 0, 0);
        queue.AddEvent(ev);
    }
}

void MOUSE_EventButton(const uint8_t idx,
                       const bool pressed)
{
    MouseEvent ev;
    for (auto &interface : mouse_interfaces)
        if (interface->IsUsingHostPointer())
            interface->NotifyButton(ev, idx, pressed);
    queue.AddEvent(ev);
}

void MOUSE_EventButton(const uint8_t idx,
                       const bool pressed,
                       const MouseInterfaceId interface_id)
{
    auto interface = MouseInterface::Get(interface_id);
    if (interface && interface->IsUsingEvents()) {
        MouseEvent ev;
        interface->NotifyButton(ev, idx, pressed);
        queue.AddEvent(ev);
    }
}

void MOUSE_EventWheel(const int16_t w_rel)
{
    MouseEvent ev;
    for (auto &interface : mouse_interfaces)
        if (interface->IsUsingHostPointer())
            interface->NotifyWheel(ev, w_rel);
    queue.AddEvent(ev);
}

void MOUSE_EventWheel(const int16_t w_rel,
                      const MouseInterfaceId interface_id)
{
    auto interface = MouseInterface::Get(interface_id);
    if (interface && interface->IsUsingEvents()) {
        MouseEvent ev;
        interface->NotifyWheel(ev, w_rel);
        queue.AddEvent(ev);
    }
}

// ***************************************************************************
// MOUSECTL.COM / GUI configurator interface
// ***************************************************************************

void get_relevant_interfaces(std::vector<MouseInterface *> &list_out,
                             const std::vector<MouseInterfaceId> &list_ids)
{
    list_out.clear();
    auto list_tmp = list_out;

    if (list_ids.empty())
        // If command does not specify interfaces,
        // assume we are interested in all of them
        list_tmp = mouse_interfaces;
    else
        for (const auto &interface_id : list_ids) {
            auto interface = MouseInterface::Get(interface_id);
            if (interface)
                list_tmp.push_back(interface);
        }

    // Filter out not emulated ones
    for (const auto &interface : list_tmp)
        if (interface->IsEmulated())
            list_out.push_back(interface);
}

MouseConfigAPI::MouseConfigAPI()
{
    manymouse.StartConfigAPI();
}

MouseConfigAPI::~MouseConfigAPI()
{
    manymouse.StopConfigAPI();
}

const std::vector<MouseInterfaceInfoEntry> &MouseConfigAPI::GetInfoInterfaces() const
{
    return mouse_info.interfaces;
}

const std::vector<MousePhysicalInfoEntry> &MouseConfigAPI::GetInfoPhysical()
{
    manymouse.RescanIfSafe();
    return mouse_info.physical;
}

bool MouseConfigAPI::CheckInterfaces(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    if (list_ids.empty() && list.empty())
        return false; // no emulated mouse interfaces
    if (list_ids.empty())
        return true;  // OK, requested all emulated interfaces
    if (list_ids.size() != list.size())
        return false; // at least one requested interface is not emulated

    return true;
}

bool MouseConfigAPI::PatternToRegex(const std::string &pattern,
                                    std::regex &regex)
{
    // Convert DOS wildcard pattern to a regular expression
    std::stringstream pattern_regex;
    pattern_regex << std::hex;
    for (const auto character : pattern) {
        if (character < 0x20 || character > 0x7E) {
            return false;
        }
        if (character == '?')
            pattern_regex << ".";
        else if (character == '*')
            pattern_regex << ".*";
        else if ((character >= '0' && character <= '9') ||
                 (character >= 'a' && character <= 'z') ||
                 (character >= 'A' && character <= 'Z'))
            pattern_regex << character;
        else
            pattern_regex << "\\x" << static_cast<int>(character);
    }

    // Return a case-insensitive regular expression
    regex = std::regex(pattern_regex.str(), std::regex_constants::icase);
    return true;
}

bool MouseConfigAPI::ProbeForMapping(uint8_t &device_id)
{
    manymouse.RescanIfSafe();
    return manymouse.ProbeForMapping(device_id);
}

bool MouseConfigAPI::Map(const MouseInterfaceId interface_id,
                         const uint8_t device_idx)
{
    auto mouse_interface = MouseInterface::Get(interface_id);
    if (!mouse_interface)
        return false;

    return mouse_interface->ConfigMap(device_idx);
}

bool MouseConfigAPI::Map(const MouseInterfaceId interface_id,
                         const std::regex &regex)
{
    manymouse.RescanIfSafe();
    const auto idx = manymouse.GetIdx(regex);
    if (idx >= mouse_info.physical.size())
        return false;

    return Map(interface_id, idx);
}

bool MouseConfigAPI::UnMap(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigUnMap();

    return !list.empty();
}

bool MouseConfigAPI::OnOff(const MouseConfigAPI::ListIDs &list_ids,
                           const bool enable)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigOnOff(enable);

    return !list.empty();
}

bool MouseConfigAPI::Reset(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigReset();

    return !list.empty();
}

bool MouseConfigAPI::SetSensitivity(const MouseConfigAPI::ListIDs &list_ids,
                                    const uint8_t sensitivity_x,
                                    const uint8_t sensitivity_y)
{
    if (!sensitivity_x || sensitivity_x > mouse_predefined.sensitivity_user_max ||
        !sensitivity_y || sensitivity_y > mouse_predefined.sensitivity_user_max)
        return false;

    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigSetSensitivity(sensitivity_x, sensitivity_y);

    return !list.empty();
}

bool MouseConfigAPI::SetSensitivityX(const MouseConfigAPI::ListIDs &list_ids,
                                     const uint8_t sensitivity_x)
{
    if (!sensitivity_x || sensitivity_x > mouse_predefined.sensitivity_user_max)
        return false;

    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigSetSensitivityX(sensitivity_x);

    return !list.empty();
}

bool MouseConfigAPI::SetSensitivityY(const MouseConfigAPI::ListIDs &list_ids,
                                     const uint8_t sensitivity_y)
{
    if (!sensitivity_y || sensitivity_y > mouse_predefined.sensitivity_user_max)
        return false;

    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigSetSensitivityY(sensitivity_y);

    return !list.empty();
}

bool MouseConfigAPI::ResetSensitivity(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigResetSensitivity();

    return !list.empty();
}

bool MouseConfigAPI::ResetSensitivityX(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigResetSensitivityX();

    return !list.empty();
}

bool MouseConfigAPI::ResetSensitivityY(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigResetSensitivityY();

    return !list.empty();
}

const std::vector<uint16_t> &MouseConfigAPI::GetValidMinRateList()
{
    return MouseConfig::GetValidMinRateList();
}

const std::string &MouseConfigAPI::GetValidMinRateStr()
{
    static std::string out_str = "";

    if (out_str.empty()) {
        const auto &valid_list = GetValidMinRateList();

        bool first = true;
        for (const auto &rate : valid_list) {
            if (first)
                first = false;
            else
                out_str += std::string(", ");
            out_str += std::to_string(rate);
        }
    }

    return out_str;
}

bool MouseConfigAPI::SetMinRate(const MouseConfigAPI::ListIDs &list_ids,
                                const uint16_t value_hz)
{
    const auto &valid_list = GetValidMinRateList();
    if (std::find(valid_list.begin(), valid_list.end(), value_hz) == valid_list.end())
        return false; // invalid value

    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigSetMinRate(value_hz);

    return !list.empty();
}

bool MouseConfigAPI::ResetMinRate(const MouseConfigAPI::ListIDs &list_ids)
{
    std::vector<MouseInterface *> list;
    get_relevant_interfaces(list, list_ids);

    for (auto &interface : list)
        interface->ConfigResetMinRate();

    return !list.empty();
}

// ***************************************************************************
// Initialization
// ***************************************************************************

void MOUSE_SetConfig(const bool  raw_input,
                     const float sensitivity_x,
                     const float sensitivity_y)
{
    // Called during SDL initialization
    mouse_config.raw_input     = raw_input;
    mouse_config.sensitivity_gui_x = sensitivity_x;
    mouse_config.sensitivity_gui_y = sensitivity_y;

    // Just in case it is also called later
    for (auto &interface : mouse_interfaces)
        interface->UpdateConfig();

    // Start mouse emulation if ready
    mouse_shared.ready_config_sdl = true;
    MOUSE_Startup();
}

void MOUSE_SetNoMouse()
{
    // NOTE: if it is decided to not allow enabling/disabling
    // this during runtime, add button click releases for all
    // the mouse buttons
    mouse_config.no_mouse = true;
}

void MOUSE_Startup()
{
    if (mouse_shared.started ||
        !mouse_shared.ready_startup_sequence ||
        !mouse_shared.ready_config_mouse ||
        !mouse_shared.ready_config_sdl)
        return;

    // Callback for ps2 irq
    auto call_int74 = CALLBACK_Allocate();
    CALLBACK_Setup(call_int74, &int74_handler, CB_IRQ12, "int 74");
    // pseudocode for CB_IRQ12:
    //    sti
    //    push ds
    //    push es
    //    pushad
    //    callback int74_handler
    //        ps2 or user callback if requested
    //        otherwise jumps to CB_IRQ12_RET
    //    push ax
    //    mov al, 0x20
    //    out 0xa0, al
    //    out 0x20, al
    //    pop    ax
    //    cld
    //    retf

    int74_ret_callback = CALLBACK_Allocate();
    CALLBACK_Setup(int74_ret_callback, &int74_ret_handler, CB_IRQ12_RET, "int 74 ret");
    // pseudocode for CB_IRQ12_RET:
    //    cli
    //    mov al, 0x20
    //    out 0xa0, al
    //    out 0x20, al
    //    callback int74_ret_handler
    //    popad
    //    pop es
    //    pop ds
    //    iret

    // (MOUSE_IRQ > 7) ? (0x70 + MOUSE_IRQ - 8) : (0x8 + MOUSE_IRQ);
    RealSetVec(0x74, CALLBACK_RealPointer(call_int74));

    MouseInterface::InitAllInstances();
    mouse_shared.started = true;
}

void MOUSE_Init(Section * /*sec*/)
{
    // Start mouse emulation if ready
    mouse_shared.ready_startup_sequence = true;
    MOUSE_Startup();
}
