/*
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include "mouse_interfaces.h"
#include "mouse.h"
#include "mouse_common.h"
#include "mouse_config.h"
#include "mouse_manymouse.h"
#include "mouse_queue.h"

#include "checks.h"

CHECK_NARROWING();

static MouseQueue &mouse_queue = MouseQueue::GetInstance();

std::vector<MouseInterface *> mouse_interfaces = {};

// ***************************************************************************
// Mouse interface information facade
// ***************************************************************************

MouseInterfaceInfoEntry::MouseInterfaceInfoEntry(const MouseInterfaceId interface_id)
        : interface_idx(static_cast<uint8_t>(interface_id))
{}

const MouseInterface &MouseInterfaceInfoEntry::Interface() const
{
	return *mouse_interfaces[interface_idx];
}

const MousePhysical &MouseInterfaceInfoEntry::MappedPhysical() const
{
	const auto mapped_idx = Interface().GetMappedDeviceIdx();
	return ManyMouseGlue::GetInstance().physical_devices[mapped_idx];
}

bool MouseInterfaceInfoEntry::IsEmulated() const
{
	return Interface().IsEmulated();
}

bool MouseInterfaceInfoEntry::IsMapped() const
{
	return Interface().IsMapped();
}

bool MouseInterfaceInfoEntry::IsMapped(const uint8_t device_idx) const
{
	return Interface().IsMapped(device_idx);
}

bool MouseInterfaceInfoEntry::IsMappedDeviceDisconnected() const
{
	if (!IsMapped())
		return false;

	return MappedPhysical().IsDisconnected();
}

MouseInterfaceId MouseInterfaceInfoEntry::GetInterfaceId() const
{
	return Interface().GetInterfaceId();
}

MouseMapStatus MouseInterfaceInfoEntry::GetMapStatus() const
{
	return Interface().GetMapStatus();
}

const std::string &MouseInterfaceInfoEntry::GetMappedDeviceName() const
{
	static const std::string empty = "";
	if (!IsMapped())
		return empty;

	return MappedPhysical().GetName();
}

int16_t MouseInterfaceInfoEntry::GetSensitivityX() const
{
	return Interface().GetSensitivityX();
}

int16_t MouseInterfaceInfoEntry::GetSensitivityY() const
{
	return Interface().GetSensitivityY();
}

uint16_t MouseInterfaceInfoEntry::GetMinRate() const
{
	return Interface().GetMinRate();
}

uint16_t MouseInterfaceInfoEntry::GetRate() const
{
	return Interface().GetRate();
}

// ***************************************************************************
// Physical mouse information facade
// ***************************************************************************

MousePhysicalInfoEntry::MousePhysicalInfoEntry(const uint8_t idx) : idx(idx) {}

const MousePhysical &MousePhysicalInfoEntry::Physical() const
{
	return ManyMouseGlue::GetInstance().physical_devices[idx];
}

bool MousePhysicalInfoEntry::IsMapped() const
{
	return Physical().IsMapped();
}

bool MousePhysicalInfoEntry::IsDeviceDisconnected() const
{
	return Physical().IsDisconnected();
}

const std::string &MousePhysicalInfoEntry::GetDeviceName() const
{
	return Physical().GetName();
}

// ***************************************************************************
// Concrete interfaces - declarations
// ***************************************************************************

class InterfaceDos final : public MouseInterface {
public:
	InterfaceDos();
	~InterfaceDos() = default;

	void NotifyMoved(MouseEvent &ev, const float x_rel, const float y_rel,
	                 const uint32_t x_abs, const uint32_t y_abs) override;
	void NotifyButton(MouseEvent &ev, const uint8_t idx,
	                  const bool pressed) override;
	void NotifyWheel(MouseEvent &ev, const int16_t w_rel) override;
	void NotifyBooting() override;

	void UpdateInputType() override;

private:
	friend class MouseInterface;

	InterfaceDos(const InterfaceDos &)            = delete;
	InterfaceDos &operator=(const InterfaceDos &) = delete;

	void Init() override;

	void UpdateMinRate() override;
	void UpdateRate() override;
};

class InterfacePS2 final : public MouseInterface {
public:
	InterfacePS2();
	~InterfacePS2() = default;

	void NotifyMoved(MouseEvent &ev, const float x_rel, const float y_rel,
	                 const uint32_t x_abs, const uint32_t y_abs) override;
	void NotifyButton(MouseEvent &ev, const uint8_t idx,
	                  const bool pressed) override;
	void NotifyWheel(MouseEvent &ev, const int16_t w_rel) override;

	void UpdateInputType() override;

private:
	friend class MouseInterface;

	InterfacePS2(const InterfacePS2 &other)            = delete;
	InterfacePS2 &operator=(const InterfacePS2 &other) = delete;

	void Init() override;

	void UpdateSensitivity() override;
	void UpdateRate() override;

	float sensitivity_coeff_vmm_x = 1.0f; // cached sensitivity coefficient
	float sensitivity_coeff_vmm_y = 1.0f;
};

class InterfaceCOM final : public MouseInterface {
public:
	InterfaceCOM(const uint8_t port_id);
	~InterfaceCOM() = default;

	void NotifyMoved(MouseEvent &ev, const float x_rel, const float y_rel,
	                 const uint32_t x_abs, const uint32_t y_abs) override;
	void NotifyButton(MouseEvent &ev, const uint8_t idx,
	                  const bool pressed) override;
	void NotifyWheel(MouseEvent &ev, const int16_t w_rel) override;

	void UpdateRate() override;

	void RegisterListener(CSerialMouse &listener_object) override;
	void UnRegisterListener() override;

private:
	friend class MouseInterface;

	InterfaceCOM()                                = delete;
	InterfaceCOM(const InterfaceCOM &)            = delete;
	InterfaceCOM &operator=(const InterfaceCOM &) = delete;

	CSerialMouse *listener = nullptr;
};

// ***************************************************************************
// Base mouse interface
// ***************************************************************************

static InterfaceDos interface_dos;
static InterfacePS2 interface_ps2;
static InterfaceCOM interface_com1(0);
static InterfaceCOM interface_com2(1);
static InterfaceCOM interface_com3(2);
static InterfaceCOM interface_com4(3);

void MouseInterface::InitAllInstances()
{
	if (!mouse_interfaces.empty())
		return; // already initialized

	const auto first = static_cast<uint8_t>(MouseInterfaceId::First);
	const auto last  = static_cast<uint8_t>(MouseInterfaceId::Last);

	for (uint8_t i = first; i <= last; i++)
		switch (static_cast<MouseInterfaceId>(i)) {
		case MouseInterfaceId::DOS:
			mouse_interfaces.push_back(&interface_dos);
			break;
		case MouseInterfaceId::PS2:
			mouse_interfaces.push_back(&interface_ps2);
			break;
		case MouseInterfaceId::COM1:
			mouse_interfaces.push_back(&interface_com1);
			break;
		case MouseInterfaceId::COM2:
			mouse_interfaces.push_back(&interface_com2);
			break;
		case MouseInterfaceId::COM3:
			mouse_interfaces.push_back(&interface_com3);
			break;
		case MouseInterfaceId::COM4:
			mouse_interfaces.push_back(&interface_com4);
			break;
		default: assert(false); break;
		}

	for (auto interface : mouse_interfaces) {
		interface->Init();
		interface->UpdateConfig();
	}
}

MouseInterface *MouseInterface::Get(const MouseInterfaceId interface_id)
{
	const auto idx = static_cast<size_t>(interface_id);
	if (idx < mouse_interfaces.size())
		return mouse_interfaces[idx];

	assert(interface_id == MouseInterfaceId::None);
	return nullptr;
}

MouseInterface *MouseInterface::GetDOS()
{
	const auto idx = static_cast<uint8_t>(MouseInterfaceId::DOS);
	return MouseInterface::Get(static_cast<MouseInterfaceId>(idx));
}

MouseInterface *MouseInterface::GetPS2()
{
	const auto idx = static_cast<uint8_t>(MouseInterfaceId::PS2);
	return MouseInterface::Get(static_cast<MouseInterfaceId>(idx));
}

MouseInterface *MouseInterface::GetSerial(const uint8_t port_id)
{
	if (port_id < SERIAL_MAX_PORTS) {
		const auto idx = static_cast<uint8_t>(MouseInterfaceId::COM1) + port_id;
		return MouseInterface::Get(static_cast<MouseInterfaceId>(idx));
	}

	LOG_ERR("MOUSE: Ports above COM4 not supported");
	assert(false);
	return nullptr;
}

MouseInterface::MouseInterface(const MouseInterfaceId interface_id,
                               const float sensitivity_predefined)
        : interface_id(interface_id),
          sensitivity_predefined(sensitivity_predefined)
{
	mouse_info.interfaces.emplace_back(MouseInterfaceInfoEntry(interface_id));
}

void MouseInterface::Init()
{
	// At this point configuration should already be loaded,
	// so the default sensitivity is known
	ConfigResetSensitivity();
}

uint8_t MouseInterface::GetInterfaceIdx() const
{
	return static_cast<uint8_t>(interface_id);
}

bool MouseInterface::IsMapped() const
{
	return mapped_idx < mouse_info.physical.size();
}

bool MouseInterface::IsMapped(const uint8_t device_idx) const
{
	return mapped_idx == device_idx;
}

bool MouseInterface::IsEmulated() const
{
	return emulated;
}

bool MouseInterface::IsUsingEvents() const
{
	return IsEmulated() && (map_status == MouseMapStatus::HostPointer ||
	                        map_status == MouseMapStatus::Mapped);
}

bool MouseInterface::IsUsingHostPointer() const
{
	return IsEmulated() && (map_status == MouseMapStatus::HostPointer);
}

uint16_t MouseInterface::GetMinRate() const
{
	return min_rate_hz;
}

MouseInterfaceId MouseInterface::GetInterfaceId() const
{
	return interface_id;
}

MouseMapStatus MouseInterface::GetMapStatus() const
{
	return map_status;
}

uint8_t MouseInterface::GetMappedDeviceIdx() const
{
	return mapped_idx;
}

int16_t MouseInterface::GetSensitivityX() const
{
	return sensitivity_user_x;
}

int16_t MouseInterface::GetSensitivityY() const
{
	return sensitivity_user_y;
}

uint16_t MouseInterface::GetRate() const
{
	return rate_hz;
}

void MouseInterface::NotifyInterfaceRate(const uint16_t new_rate_hz)
{
	interface_rate_hz = new_rate_hz;
	UpdateRate();
}

void MouseInterface::NotifyBooting() {}

void MouseInterface::NotifyDisconnect()
{
	SetMapStatus(MouseMapStatus::Disconnected, mapped_idx);
}

void MouseInterface::SetMapStatus(const MouseMapStatus status, const uint8_t device_idx)
{
	MouseMapStatus new_map_status = status;
	uint8_t new_mapped_idx        = device_idx;

	// Change 'mapped to host pointer' to just 'host pointer'
	if (new_map_status == MouseMapStatus::Mapped &&
	    new_mapped_idx >= mouse_info.physical.size())
		new_map_status = MouseMapStatus::HostPointer;

	// if physical device is disconnected, change state from
	// 'mapped' to 'disconnected'
	if (new_map_status == MouseMapStatus::Mapped &&
	    mouse_info.physical[new_mapped_idx].IsDeviceDisconnected())
		new_map_status = MouseMapStatus::Disconnected;

	// Perform necessary updates for mapping change
	if (map_status != new_map_status || mapped_idx != new_mapped_idx)
		ResetButtons();
	if (map_status != new_map_status)
		UpdateInputType();
	if (mapped_idx != new_mapped_idx)
		ManyMouseGlue::GetInstance().Map(new_mapped_idx, interface_id);

	// Apply new mapping
	mapped_idx = new_mapped_idx;
	map_status = new_map_status;
}

bool MouseInterface::ConfigMap(const uint8_t device_idx)
{
	if (!IsEmulated())
		return false;

	SetMapStatus(MouseMapStatus::Mapped, device_idx);
	return true;
}

void MouseInterface::ConfigUnMap()
{
	SetMapStatus(MouseMapStatus::Mapped, idx_host_pointer);
}

void MouseInterface::ConfigOnOff(const bool enable)
{
	if (!IsEmulated())
		return;

	if (!enable)
		SetMapStatus(MouseMapStatus::Disabled);
	else if (map_status == MouseMapStatus::Disabled)
		SetMapStatus(MouseMapStatus::HostPointer);
}

void MouseInterface::ConfigReset()
{
	ConfigUnMap();
	ConfigOnOff(true);
	ConfigResetSensitivity();
	ConfigResetMinRate();
}

void MouseInterface::ConfigSetSensitivity(const int16_t value_x, const int16_t value_y)
{
	sensitivity_user_x = value_x;
	sensitivity_user_y = value_y;
	UpdateSensitivity();
}

void MouseInterface::ConfigSetSensitivityX(const int16_t value)
{
	sensitivity_user_x = value;
	UpdateSensitivity();
}

void MouseInterface::ConfigSetSensitivityY(const int16_t value)
{
	sensitivity_user_y = value;
	UpdateSensitivity();
}

void MouseInterface::ConfigResetSensitivity()
{
	ConfigSetSensitivity(mouse_config.sensitivity_x, mouse_config.sensitivity_y);
}

void MouseInterface::ConfigResetSensitivityX()
{
	ConfigSetSensitivityX(mouse_config.sensitivity_x);
}

void MouseInterface::ConfigResetSensitivityY()
{
	ConfigSetSensitivityY(mouse_config.sensitivity_y);
}

void MouseInterface::ConfigSetMinRate(const uint16_t value_hz)
{
	min_rate_hz = value_hz;
	UpdateMinRate();
}

void MouseInterface::ConfigResetMinRate()
{
	ConfigSetMinRate(0);
}

void MouseInterface::RegisterListener(CSerialMouse &)
{
	assert(false); // should never be called for unsupported interface
}

void MouseInterface::UnRegisterListener()
{
	assert(false); // should never be called for unsupported interface
}

void MouseInterface::UpdateConfig()
{
	UpdateInputType();
	UpdateSensitivity();
}

void MouseInterface::UpdateInputType() {}

void MouseInterface::UpdateSensitivity()
{
	auto calculate = [this](const int16_t setting) {
		if (setting == 0)
			return 0.0f;
		const float user_value = static_cast<float>(setting);
		const float scaling    = sensitivity_predefined / 100.0f;
		return user_value * scaling;
	};

	sensitivity_coeff_x = calculate(sensitivity_user_x);
	sensitivity_coeff_y = calculate(sensitivity_user_y);
}

void MouseInterface::UpdateMinRate()
{
	UpdateRate();
}

void MouseInterface::UpdateRate()
{
	rate_hz = MOUSE_ClampRateHz(std::max(interface_rate_hz, min_rate_hz));
}

void MouseInterface::UpdateButtons(const uint8_t idx, const bool pressed)
{
	old_buttons_12  = buttons_12;
	old_buttons_345 = buttons_345;

	switch (idx) {
	case 0: // left button
		buttons_12.left = pressed ? 1 : 0;
		break;
	case 1: // right button
		buttons_12.right = pressed ? 1 : 0;
		break;
	case 2: // middle button
		buttons_345.middle = pressed ? 1 : 0;
		break;
	case 3: // extra button #1
		buttons_345.extra_1 = pressed ? 1 : 0;
		break;
	case 4: // extra button #2
		buttons_345.extra_2 = pressed ? 1 : 0;
		break;
	default: // button not supported
		return;
	}
}

void MouseInterface::ResetButtons()
{
	buttons_12  = 0;
	buttons_345 = 0;
}

bool MouseInterface::ChangedButtonsJoined() const
{
	return (old_buttons_12.data != buttons_12.data) ||
	       (old_buttons_345.data != buttons_345.data);
}

bool MouseInterface::ChangedButtonsSquished() const
{
	if (GCC_LIKELY(old_buttons_12.data != buttons_12.data))
		return true;

	return (old_buttons_345.data == 0 && buttons_345.data != 0) ||
	       (old_buttons_345.data != 0 && buttons_345.data == 0);
}

MouseButtonsAll MouseInterface::GetButtonsJoined() const
{
	MouseButtonsAll buttons_all;
	buttons_all.data = buttons_12.data | buttons_345.data;

	return buttons_all;
}

MouseButtons12S MouseInterface::GetButtonsSquished() const
{
	MouseButtons12S buttons_12S;

	// Squish buttons 3/4/5 into single virtual middle button
	buttons_12S.data = buttons_12.data;
	if (buttons_345.data)
		buttons_12S.middle = 1;

	return buttons_12S;
}

// ***************************************************************************
// Concrete interfaces - implementation
// ***************************************************************************

InterfaceDos::InterfaceDos()
        : MouseInterface(MouseInterfaceId::DOS, mouse_predefined.sensitivity_dos)
{}

void InterfaceDos::Init()
{
	MouseInterface::Init();
	if (mouse_config.dos_driver) {
		emulated = true;
		MOUSEDOS_Init();
	}
	MOUSEDOS_NotifyMinRate(min_rate_hz);
}

void InterfaceDos::NotifyMoved(MouseEvent &ev, const float x_rel, const float y_rel,
                               const uint32_t x_abs, const uint32_t y_abs)
{
	ev.dos_moved = MOUSEDOS_NotifyMoved(x_rel * sensitivity_coeff_x,
	                                    y_rel * sensitivity_coeff_y,
	                                    x_abs,
	                                    y_abs);

	ev.request_dos = ev.dos_moved;
}

void InterfaceDos::NotifyButton(MouseEvent &ev, const uint8_t idx, const bool pressed)
{
	UpdateButtons(idx, pressed);
	if (GCC_UNLIKELY(!ChangedButtonsSquished()))
		return;

	ev.dos_button  = true;
	ev.dos_buttons = GetButtonsSquished();
	ev.request_dos = true;
}

void InterfaceDos::NotifyWheel(MouseEvent &ev, const int16_t w_rel)
{
	ev.dos_wheel   = MOUSEDOS_NotifyWheel(w_rel);
	ev.request_dos = ev.dos_wheel;
}

void InterfaceDos::NotifyBooting()
{
	// DOS virtual mouse driver gets unavailable
	// if guest OS is booted so do not waste time
	// emulating this interface

	ConfigReset();
	emulated = false;
	ManyMouseGlue::GetInstance().ShutdownIfSafe();
}

void InterfaceDos::UpdateInputType()
{
	const bool use_relative = IsMapped() || MOUSE_IsCaptured();
	const bool is_input_raw = IsMapped() || mouse_config.raw_input;

	MOUSEDOS_NotifyInputType(use_relative, is_input_raw);
}

void InterfaceDos::UpdateMinRate()
{
	MOUSEDOS_NotifyMinRate(min_rate_hz);
}

void InterfaceDos::UpdateRate()
{
	MouseInterface::UpdateRate();
	mouse_queue.SetRateDOS(rate_hz);
}

InterfacePS2::InterfacePS2()
        : MouseInterface(MouseInterfaceId::PS2, mouse_predefined.sensitivity_ps2)
{}

void InterfacePS2::Init()
{
	MouseInterface::Init();
	emulated = true;
	MOUSEPS2_Init();
	MOUSEVMM_Init();
}

void InterfacePS2::NotifyMoved(MouseEvent &ev, const float x_rel, const float y_rel,
                               const uint32_t x_abs, const uint32_t y_abs)
{
	const bool request_ps2 = MOUSEPS2_NotifyMoved(x_rel * sensitivity_coeff_x,
	                                              y_rel * sensitivity_coeff_y);
	const bool request_vmm = MOUSEVMM_NotifyMoved(x_rel * sensitivity_coeff_vmm_x,
	                                              y_rel * sensitivity_coeff_vmm_y,
	                                              x_abs,
	                                              y_abs);

	ev.request_ps2 = request_ps2 || request_vmm;
}

void InterfacePS2::NotifyButton(MouseEvent &ev, const uint8_t idx, const bool pressed)
{
	UpdateButtons(idx, pressed);
	if (GCC_UNLIKELY(!ChangedButtonsJoined()))
		return;

	const bool request_ps2 = MOUSEPS2_NotifyButton(GetButtonsSquished(),
	                                               GetButtonsJoined());
	const bool request_vmm = MOUSEVMM_NotifyButton(GetButtonsSquished());

	ev.request_ps2 = request_ps2 || request_vmm;
}

void InterfacePS2::NotifyWheel(MouseEvent &ev, const int16_t w_rel)
{
	const bool request_ps2 = MOUSEPS2_NotifyWheel(w_rel);
	const bool request_vmm = MOUSEVMM_NotifyWheel(w_rel);

	ev.request_ps2 = request_ps2 || request_vmm;
}

void InterfacePS2::UpdateInputType()
{
	const bool use_relative = IsMapped() || MOUSE_IsCaptured();
	const bool is_input_raw = IsMapped() || mouse_config.raw_input;

	MOUSEVMM_NotifyInputType(use_relative, is_input_raw);
}

void InterfacePS2::UpdateSensitivity()
{
	MouseInterface::UpdateSensitivity();

	const float tmp = mouse_predefined.sensitivity_vmm /
	                  mouse_predefined.sensitivity_ps2;
	sensitivity_coeff_vmm_x = sensitivity_coeff_x * tmp;
	sensitivity_coeff_vmm_y = sensitivity_coeff_y * tmp;
}

void InterfacePS2::UpdateRate()
{
	MouseInterface::UpdateRate();
	mouse_queue.SetRatePS2(rate_hz);
}

InterfaceCOM::InterfaceCOM(const uint8_t port_id)
        : MouseInterface(static_cast<MouseInterfaceId>(
                                 static_cast<uint8_t>(MouseInterfaceId::COM1) + port_id),
                         mouse_predefined.sensitivity_com)
{}

void InterfaceCOM::NotifyMoved(MouseEvent &, const float x_rel, const float y_rel,
                               const uint32_t, const uint32_t)
{
	assert(listener);

	listener->NotifyMoved(x_rel * sensitivity_coeff_x,
	                      y_rel * sensitivity_coeff_y);
}

void InterfaceCOM::NotifyButton(MouseEvent &, const uint8_t idx, const bool pressed)
{
	assert(listener);

	UpdateButtons(idx, pressed);
	if (GCC_UNLIKELY(!ChangedButtonsSquished()))
		return;

	listener->NotifyButton(GetButtonsSquished().data, idx);
}

void InterfaceCOM::NotifyWheel(MouseEvent &, const int16_t w_rel)
{
	assert(listener);

	listener->NotifyWheel(w_rel);
}

void InterfaceCOM::UpdateRate()
{
	MouseInterface::UpdateRate();

	if (!listener)
		return;

	if (interface_rate_hz >= rate_hz || !interface_rate_hz)
		listener->BoostRate(0);
	else
		// Ask serial mouse emulation code to cheat on transmission
		// speed to simulate higher sampling rate
		listener->BoostRate(rate_hz);
}

void InterfaceCOM::RegisterListener(CSerialMouse &listener_object)
{
	listener = &listener_object;
	emulated = true;
}

void InterfaceCOM::UnRegisterListener()
{
	// Serial mouse gets unavailable when listener object disconnects

	ConfigReset();
	listener = nullptr;
	emulated = false;
	ManyMouseGlue::GetInstance().ShutdownIfSafe();
}
