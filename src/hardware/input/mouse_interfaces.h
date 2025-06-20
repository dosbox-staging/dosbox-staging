/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2025  The DOSBox Staging Team
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

#ifndef DOSBOX_MOUSE_INTERFACES_H
#define DOSBOX_MOUSE_INTERFACES_H

#include "mouse_common.h"

#include "../serialport/serialmouse.h"

// ***************************************************************************
// Main mouse module
// ***************************************************************************

void MOUSE_StartupIfReady();

void MOUSE_NotifyDisconnect(const MouseInterfaceId interface_id);

void MOUSE_UpdateGFX();
bool MOUSE_IsCaptured();
bool MOUSE_IsRawInput();
bool MOUSE_IsProbeForMappingAllowed();

// ***************************************************************************
// DOS mouse driver
// ***************************************************************************

void MOUSEDOS_Init();
void MOUSEDOS_SetDelay(const uint8_t delay_ms);
void MOUSEDOS_NotifyInputType(const bool use_relative, const bool is_input_raw);
void MOUSEDOS_NotifyMinRate(const uint16_t value_hz);

uint8_t MOUSEDOS_DoInterrupt();
void MOUSEDOS_DoCallback(const uint8_t mask);
void MOUSEDOS_FinalizeInterrupt();

// - needs relative movements
// - understands up to 3 buttons

void MOUSEDOS_NotifyMoved(const float x_rel, const float y_rel,
                          const float x_abs, const float y_abs);
void MOUSEDOS_NotifyButton(const MouseButtons12S buttons_12S);
void MOUSEDOS_NotifyWheel(const float w_rel);

void MOUSEDOS_NotifyModelChanged();

// ***************************************************************************
// PS/2 mouse
// ***************************************************************************

void MOUSEPS2_Init();
void MOUSEPS2_SetDelay(const uint8_t delay_ms);
void MOUSEPS2_UpdateButtonSquish();

// - needs relative movements
// - understands up to 5 buttons in Intellimouse Explorer mode
// - understands up to 3 buttons in other modes

void MOUSEPS2_NotifyMoved(const float x_rel, const float y_rel);
void MOUSEPS2_NotifyMovedDummy(); // for virtual machine interfaces
void MOUSEPS2_NotifyButton(const MouseButtons12S buttons_12S,
                           const MouseButtonsAll buttons_all);
void MOUSEPS2_NotifyWheel(const float w_rel);

// ***************************************************************************
// BIOS mouse interface for PS/2 mouse
// ***************************************************************************

bool MOUSEBIOS_CheckCallback();
void MOUSEBIOS_DoCallback();
void MOUSEBIOS_FinalizeInterrupt();

// ***************************************************************************
// Virtual Machine Manager (VMware/VirtualBox) PS/2 mouse protocol extensions
// ***************************************************************************

void MOUSEVMM_NotifyInputType(const bool use_relative, const bool is_input_raw);
void MOUSEVMM_NewScreenParams(const float x_abs, const float y_abs);
void MOUSEVMM_Deactivate();

// - needs absolute mouse position
// - understands up to 3 buttons

void MOUSEVMM_NotifyMoved(const float x_rel, const float y_rel,
                          const float x_abs, const float y_abs);
void MOUSEVMM_NotifyButton(const MouseButtons12S buttons_12S);
void MOUSEVMM_NotifyWheel(const float w_rel);

// ***************************************************************************
// Serial mouse
// ***************************************************************************

// - needs relative movements
// - understands up to 3 buttons
// - needs index of button which changed state

// Nothing to declare

// ***************************************************************************
// Base mouse interface
// ***************************************************************************

class MouseInterface {
public:
	MouseInterface()                                 = delete;
	MouseInterface(const MouseInterface&)            = delete;
	MouseInterface& operator=(const MouseInterface&) = delete;
	virtual ~MouseInterface()                        = default;

	static void InitAllInstances();
	static MouseInterface* Get(const MouseInterfaceId interface_id);
	static MouseInterface* GetDOS();
	static MouseInterface* GetPS2();
	static MouseInterface* GetSerial(const uint8_t port_id);

	virtual void NotifyMoved(const float x_rel, const float y_rel,
	                         const float x_abs, const float y_abs) = 0;
	virtual void NotifyButton(const MouseButtonId id, const bool pressed) = 0;
	virtual void NotifyWheel(const float w_rel) = 0;

	void NotifyInterfaceRate(const uint16_t rate_hz);
	virtual void NotifyBooting();
	void NotifyDisconnect();

	bool IsMapped() const;
	bool IsMapped(const uint8_t physical_device_idx) const;
	bool IsEmulated() const;
	bool IsUsingEvents() const;
	bool IsUsingHostPointer() const;

	MouseInterfaceId GetInterfaceId() const;
	MouseMapStatus GetMapStatus() const;
	uint8_t  GetMappedDeviceIdx() const;
	int16_t  GetSensitivityX() const;
	int16_t  GetSensitivityY() const;
	uint16_t GetMinRate() const;
	uint16_t GetRate() const;

	bool ConfigMap(const uint8_t physical_device_idx);
	void ConfigUnMap();

	void ConfigOnOff(const bool enable);
	void ConfigReset();
	void ConfigSetSensitivity(const int16_t value_x, const int16_t value_y);
	void ConfigSetSensitivityX(const int16_t value);
	void ConfigSetSensitivityY(const int16_t value);
	void ConfigResetSensitivity();
	void ConfigResetSensitivityX();
	void ConfigResetSensitivityY();
	void ConfigSetMinRate(const uint16_t value_hz);
	void ConfigResetMinRate();

	virtual void UpdateConfig();
	virtual void UpdateInputType();
	virtual void RegisterListener(CSerialMouse& listener_object);
	virtual void UnRegisterListener();

protected:
	static constexpr uint8_t idx_host_pointer = UINT8_MAX;

	MouseInterface(const MouseInterfaceId interface_id,
	               const float sensitivity_predefined);
	virtual void Init();

	uint8_t GetInterfaceIdx() const;

	void SetMapStatus(const MouseMapStatus status,
	                  const uint8_t physical_device_idx = idx_host_pointer);

	virtual void UpdateSensitivity();
	virtual void UpdateMinRate();
	virtual void UpdateRate();
	void UpdateButtons(const MouseButtonId id, const bool pressed);
	void ResetButtons();

	bool ChangedButtonsJoined() const;
	bool ChangedButtonsSquished() const;

	MouseButtonsAll GetButtonsJoined() const;
	MouseButtons12S GetButtonsSquished() const;

	bool emulated = false;

	// cached combined sensitivity coefficients,to reduce calculations
	float sensitivity_coeff_x = 1.0f;
	float sensitivity_coeff_y = 1.0f;

	int16_t sensitivity_user_x = 0;
	int16_t sensitivity_user_y = 0;

	uint16_t rate_hz           = 0;
	uint16_t min_rate_hz       = 0;
	uint16_t interface_rate_hz = 0;

private:
	const MouseInterfaceId interface_id = MouseInterfaceId::None;

	MouseMapStatus map_status   = MouseMapStatus::HostPointer;
	uint8_t mapped_physical_idx = idx_host_pointer;

	MouseButtons12 buttons_12   = 0; // host side buttons 1 (left), 2 (right)
	MouseButtons345 buttons_345 = 0; // host side buttons 3 (middle), 4, and 5

	MouseButtons12 old_buttons_12   = 0; // pre-update values
	MouseButtons345 old_buttons_345 = 0;

	float sensitivity_predefined = 1.0f; // hardcoded for the given interface
};

extern std::vector<std::unique_ptr<MouseInterface>> mouse_interfaces;

#endif // DOSBOX_MOUSE_INTERFACES_H
