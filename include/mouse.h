/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#ifndef DOSBOX_MOUSE_H
#define DOSBOX_MOUSE_H

#include "dosbox.h"

#include <regex>
#include <string>
#include <vector>

// ***************************************************************************
// Initialization, configuration
// ***************************************************************************

void MOUSE_Init(Section *);
void MOUSE_AddConfigSection(const config_ptr_t &);

// ***************************************************************************
// Data types
// ***************************************************************************

enum class MouseInterfaceId : uint8_t {
	DOS,  // emulated DOS mouse driver
	PS2,  // PS/2 mouse (this includes VMware mouse protocol)
	COM1, // serial mouse
	COM2,
	COM3,
	COM4,

	First = DOS,
	Last  = COM4,
	None  = UINT8_MAX
};

constexpr uint8_t num_mouse_interfaces = static_cast<uint8_t>(MouseInterfaceId::Last) +
                                         1;

enum class MouseMapStatus : uint8_t {
	HostPointer,
	Mapped,       // single physical mouse mapped to emulated port
	Disconnected, // physical mouse used to be mapped, but got unplugged
	Disabled
};

// ***************************************************************************
// Notifications from external subsystems - all should go via these methods
// ***************************************************************************

void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const uint16_t x_abs, const uint16_t y_abs);
void MOUSE_EventMoved(const float x_rel, const float y_rel,
                      const MouseInterfaceId device_id);

void MOUSE_EventButton(const uint8_t idx, const bool pressed);
void MOUSE_EventButton(const uint8_t idx, const bool pressed,
                       const MouseInterfaceId device_id);

void MOUSE_EventWheel(const int16_t w_rel);
void MOUSE_EventWheel(const int16_t w_rel, const MouseInterfaceId device_id);

void MOUSE_NotifyBooting();

void MOUSE_SetConfigSeamless(const bool seamless);
void MOUSE_SetConfigNoMouse();

void MOUSE_NewScreenParams(const uint16_t clip_x, const uint16_t clip_y,
                           const uint16_t res_x, const uint16_t res_y,
                           const bool fullscreen, const uint16_t x_abs,
                           const uint16_t y_abs);

// ***************************************************************************
// Information for the GFX subsystem
// ***************************************************************************

bool MOUSE_IsUsingSeamlessDriver(); // if driver with seamless pointer support
                                    // is running
bool MOUSE_IsUsingSeamlessSetting(); // if user selected seamless mode is in effect

// ***************************************************************************
// BIOS mouse interface for PS/2 mouse
// ***************************************************************************

bool MOUSEBIOS_SetState(const bool use);
void MOUSEBIOS_SetCallback(const uint16_t pseg, const uint16_t pofs);
void MOUSEBIOS_Reset();
bool MOUSEBIOS_SetPacketSize(const uint8_t packet_size);
bool MOUSEBIOS_SetSampleRate(const uint8_t rate_id);
void MOUSEBIOS_SetScaling21(const bool enable);
bool MOUSEBIOS_SetResolution(const uint8_t res_id);
uint8_t MOUSEBIOS_GetProtocol();
uint8_t MOUSEBIOS_GetStatus();
uint8_t MOUSEBIOS_GetResolution();
uint8_t MOUSEBIOS_GetSampleRate();

// ***************************************************************************
// DOS mouse driver
// ***************************************************************************

void MOUSEDOS_BeforeNewVideoMode();
void MOUSEDOS_AfterNewVideoMode(const bool setmode);

// ***************************************************************************
// MOUSECTL.COM / GUI configurator interface
// ***************************************************************************

class MouseInterface;
class MousePhysical;

class MouseInterfaceInfoEntry final {
public:
	bool IsEmulated() const;
	bool IsMapped() const;
	bool IsMapped(const uint8_t device_idx) const;
	bool IsMappedDeviceDisconnected() const;

	MouseInterfaceId GetInterfaceId() const;
	MouseMapStatus GetMapStatus() const;
	const std::string &GetMappedDeviceName() const;
	int8_t GetSensitivityX() const; // -99 to +99
	int8_t GetSensitivityY() const; // -99 to +99
	uint16_t GetMinRate() const;    // 10-500, 0 for none
	uint16_t GetRate() const;       // current rate, 10-500, 0 for N/A

private:
	friend class MouseInterface;
	MouseInterfaceInfoEntry(const MouseInterfaceId interface_id);

	const uint8_t idx;
	const MouseInterface &Interface() const;
	const MousePhysical &MappedPhysical() const;
};

class MousePhysicalInfoEntry final {
public:
	bool IsMapped() const;
	bool IsDeviceDisconnected() const;
	const std::string &GetDeviceName() const;

private:
	friend class ManyMouseGlue;
	MousePhysicalInfoEntry(const uint8_t idx);

	const uint8_t idx;
	const MousePhysical &Physical() const;
};

class MouseControlAPI final {
public:
	// Always destroy the object once it is not needed anymore
	// (configuration tool finishes it's job) and we are returning
	// to normal code execution!

	MouseControlAPI();
	~MouseControlAPI();

	// Empty list = performs operation on all emulated interfaces
	typedef std::vector<MouseInterfaceId> ListIDs;

	// Do not use the references after object gets destroyed
	const std::vector<MouseInterfaceInfoEntry> &GetInfoInterfaces() const;
	const std::vector<MousePhysicalInfoEntry> &GetInfoPhysical();

	static bool IsNoMouseMode();
	static bool CheckInterfaces(const ListIDs &list_ids);
	static bool PatternToRegex(const std::string &pattern, std::regex &regex);

	bool ProbeForMapping(uint8_t &device_id); // For interactive mapping in
	                                          // MOUSECTL.COM only!

	bool Map(const MouseInterfaceId interface_id, const uint8_t device_idx);
	bool Map(const MouseInterfaceId interface_id, const std::regex &regex);
	bool UnMap(const ListIDs &list_ids);

	bool OnOff(const ListIDs &list_ids, const bool enable);
	bool Reset(const ListIDs &list_ids);

	// Valid sensitivity values are from -99 to +99
	bool SetSensitivity(const ListIDs &list_ids, const int8_t sensitivity_x,
	                    const int8_t sensitivity_y);
	bool SetSensitivityX(const ListIDs &list_ids, const int8_t sensitivity_x);
	bool SetSensitivityY(const ListIDs &list_ids, const int8_t sensitivity_y);

	bool ResetSensitivity(const ListIDs &list_ids);
	bool ResetSensitivityX(const ListIDs &list_ids);
	bool ResetSensitivityY(const ListIDs &list_ids);

	static const std::vector<uint16_t> &GetValidMinRateList();
	static const std::string &GetValidMinRateStr();
	bool SetMinRate(const MouseControlAPI::ListIDs &list_ids,
	                const uint16_t value_hz);
	bool ResetMinRate(const MouseControlAPI::ListIDs &list_ids);

private:
	MouseControlAPI(const MouseControlAPI &)            = delete;
	MouseControlAPI &operator=(const MouseControlAPI &) = delete;
};

#endif // DOSBOX_MOUSE_H
