// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/mouse_manymouse.h"

#include "private/mouse_common.h"
#include "private/mouse_config.h"

#include <algorithm>
#include <initializer_list>

#include "cpu/callback.h"
#include "dos/dos.h"
#include "hardware/pic.h"
#include "misc/unicode.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

void manymouse_tick(uint32_t)
{
#if C_MANYMOUSE
	ManyMouseGlue::GetInstance().Tick();
#endif // C_MANYMOUSE
}

MousePhysical::MousePhysical(const std::string &name) : name(name) {}

bool MousePhysical::IsDisconnected() const
{
	return disconnected;
}

bool MousePhysical::IsMapped() const
{
	return mapped_id.has_value();
}

std::optional<MouseInterfaceId> MousePhysical::GetMappedInterfaceId() const
{
	return mapped_id;
}

const std::string &MousePhysical::GetName() const
{
	return name;
}

ManyMouseGlue &ManyMouseGlue::GetInstance()
{
	static ManyMouseGlue manymouse_glue;
	return manymouse_glue;
}

#if C_MANYMOUSE

ManyMouseGlue::~ManyMouseGlue()
{
	PIC_RemoveEvents(manymouse_tick);
	ManyMouse_Quit();
}

void ManyMouseGlue::InitIfNeeded()
{
	if (initialized || malfunction)
		return;

	// Initialize ManyMouse library, fetch number of mice

	const auto result = ManyMouse_Init();

	if (result < 0) {
		malfunction = true;
		num_mice    = 0;

		LOG_ERR("MOUSE: ManyMouse initialization failed");
		ManyMouse_Quit();
		return;
	} else if (result > max_mice) {
		num_mice = max_mice;

		static bool already_warned = false;
		if (!already_warned) {
			already_warned = true;
			LOG_ERR("MOUSE: Up to %d simultaneously connected mice supported",
			        max_mice);
		}
	} else
		num_mice = static_cast<uint8_t>(result);

	initialized = true;

	// Get and log ManyMouse driver name

	const auto new_driver_name = std::string(ManyMouse_DriverName());
	if (new_driver_name != driver_name) {
		driver_name = new_driver_name;
		LOG_INFO("MOUSE: ManyMouse driver '%s'", driver_name.c_str());
	}

	// Scan for the physical mice

	Rescan();
}

void ManyMouseGlue::ShutdownIfSafe()
{
	if (is_mapping_in_effect || config_api_counter)
		return;

	ShutdownForced();
}

void ManyMouseGlue::ShutdownForced()
{
	if (!initialized)
		return;

	PIC_RemoveEvents(manymouse_tick);
	ManyMouse_Quit();
	ClearPhysicalMice();
	num_mice    = 0;
	initialized = false;
}

void ManyMouseGlue::StartConfigAPI()
{
	// Config API object is being created
	++config_api_counter;
	assert(config_api_counter > 0);
}

void ManyMouseGlue::StopConfigAPI()
{
	// Config API object is being destroyed
	assert(config_api_counter > 0);
	config_api_counter--;
	ShutdownIfSafe();
	if (!config_api_counter)
		rescan_blocked_config = false;
}

void ManyMouseGlue::ClearPhysicalMice()
{
	mouse_info.physical.clear();
	physical_devices.clear();
	rel_x.clear();
	rel_y.clear();
}

void ManyMouseGlue::Rescan()
{
	if (config_api_counter)
		// Do not allow another rescan until MouseConfigAPI
		// stops being used, it would be unsafe due to possible
		// changes of physical device list size/indices
		rescan_blocked_config = true;

	ClearPhysicalMice();

	for (uint8_t idx = 0; idx < num_mice; idx++) {
		// We want the mouse name to be the same regardless of the code
		// page set - so use 7-bit ASCII characters only
		auto name = utf8_to_dos(ManyMouse_DeviceName(idx),
		                        DosStringConvertMode::NoSpecialCharacters,
		                        UnicodeFallback::Simple,
		                        0);

		// Replace non-breaking space with a regular space
		const char character_nbsp  = 0x7f;
		const char character_space = 0x20;
		std::replace(name.begin(), name.end(), character_nbsp, character_space);

		// Remove non-ASCII and control characters
		for (auto pos = name.size(); pos > 0; pos--) {
			if (name[pos - 1] < character_space ||
			    name[pos - 1] >= character_nbsp)
				name.erase(pos - 1, 1);
		}

		// Try to rework into something useful name if we receive
		// something with double manufacturer name, for example change:
		// 'FooBar Corp FooBar Corp Incredible Mouse' into
		// 'FooBar Corp Incredible Mouse'
		size_t pos = name.size() / 2 + 1;
		while (--pos > 2) {
			if (name[pos - 1] != ' ')
				continue;
			const std::string tmp = name.substr(0, pos);
			if (name.rfind(tmp + tmp, 0) == std::string::npos)
				continue;
			name = name.substr(pos);
			break;
		}

		// ManyMouse should limit device names to 64 characters,
		// but make sure name is indeed limited in length
		constexpr size_t max_size = 64;
		if (name.size() > max_size)
			name.resize(max_size);

		// Strip trailing spaces, newlines, etc.
		trim(name);

		physical_devices.emplace_back(name);
		mouse_info.physical.emplace_back(MousePhysicalInfoEntry(
		        static_cast<uint8_t>(physical_devices.size() - 1)));
	}
}

void ManyMouseGlue::RescanIfSafe()
{
	if (rescan_blocked_config) {
		return;
	}

#if defined(WIN32)
	if (mouse_config.raw_input) {
		return;
	}
#endif

	ShutdownIfSafe();
	InitIfNeeded();
}

bool ManyMouseGlue::ProbeForMapping(uint8_t &physical_device_idx)
{
	// Do not even try if NoMouse is configured
	if (mouse_config.capture == MouseCapture::NoMouse)
		return false;

	// Wait a little to speedup screen update
	constexpr uint32_t ticks_threshold = 50; // time to wait idle in PIC ticks
	const auto pic_ticks_start = PIC_Ticks;
	while (PIC_Ticks >= pic_ticks_start &&
	       PIC_Ticks - pic_ticks_start < ticks_threshold) {
		if (CALLBACK_Idle()) {
			return false;
		}
	}

	// Make sure the module is initialized,
	// but suppress default event handling
	InitIfNeeded();
	if (!initialized)
		return false;
	PIC_RemoveEvents(manymouse_tick);

	// Flush events, handle critical ones
	ManyMouseEvent event;
	while (ManyMouse_PollEvent(&event))
		HandleEvent(event, true); // handle critical events

	bool success = false;
	while (!DOS_IsCancelRequest()) {
		// Poll mouse events, handle critical ones
		if (!ManyMouse_PollEvent(&event))
			continue;
		if (event.device >= max_mice)
			continue;
		HandleEvent(event, true);

		// Wait for mouse button press
		if (event.type != MANYMOUSE_EVENT_BUTTON || !event.value)
			continue;
		// Drop button events if we have no focus, etc.
		if (!MOUSE_IsProbeForMappingAllowed())
			continue;
		physical_device_idx = static_cast<uint8_t>(event.device);

		if (event.item >= 1)
			break; // user cancelled using a mouse button

		// Do not accept already mapped devices
		bool already_mapped = false;
		for (const auto interface_id : AllMouseInterfaceIds) {
			const auto& interface = MouseInterface::GetInstance(interface_id);
			if (interface.IsMapped(physical_device_idx)) {
				already_mapped = true;
				break;
			}
		}
		if (already_mapped) {
			continue;
		}

		// Mouse probed successfully
		physical_device_idx = static_cast<uint8_t>(event.device);
		success   = true;
		break;
	}

	if (is_mapping_in_effect)
		PIC_AddEvent(manymouse_tick, tick_interval);
	return success;
}

uint8_t ManyMouseGlue::GetIdx(const std::regex &regex)
{
	static_assert(max_mice < UINT8_MAX);

	// Try to match the mouse name which is not mapped yet

	for (size_t i = 0; i < physical_devices.size(); i++) {
		const auto &physical_device = physical_devices[i];

		if (physical_device.IsDisconnected() ||
		    !std::regex_match(physical_device.GetName(), regex))
			// mouse disconnected or name does not match
			continue;

		if (physical_device.GetMappedInterfaceId())
			// name matches, mouse not mapped yet - use it!
			return static_cast<uint8_t>(i);
	}

	return max_mice + 1; // return value which will be considered out of range
}

void ManyMouseGlue::Map(const uint8_t physical_device_idx,
                        const MouseInterfaceId interface_id)
{
	if (physical_device_idx >= physical_devices.size()) {
		UnMap(interface_id);
		return;
	}

	auto &physical_device = physical_devices[physical_device_idx];
	if (interface_id == physical_device.GetMappedInterfaceId()) {
		return; // nothing to update
	}
	physical_device.mapped_id = interface_id;

	MapFinalize();
}

void ManyMouseGlue::UnMap(const MouseInterfaceId interface_id)
{
	for (auto &physical_device : physical_devices) {
		if (interface_id != physical_device.GetMappedInterfaceId()) {
			continue; // not a device to unmap
		}
		physical_device.mapped_id = {};
		break;
	}

	MapFinalize();
}

void ManyMouseGlue::MapFinalize()
{
	PIC_RemoveEvents(manymouse_tick);
	is_mapping_in_effect = false;
	for (const auto &entry : mouse_info.physical) {
		if (!entry.IsMapped())
			continue;

		is_mapping_in_effect = true;
		if (mouse_config.capture != MouseCapture::NoMouse)
			PIC_AddEvent(manymouse_tick, tick_interval);
		break;
	}
}

bool ManyMouseGlue::IsMappingInEffect() const
{
	return is_mapping_in_effect;
}

void ManyMouseGlue::HandleEvent(const ManyMouseEvent &event, const bool critical_only)
{
	if (event.device >= mouse_info.physical.size()) {
		return; // device ID out of supported range
	}
	if (mouse_config.capture == MouseCapture::NoMouse &&
	    event.type != MANYMOUSE_EVENT_DISCONNECT) {
		return; // mouse control disabled in GUI
	}

	const auto device_idx = static_cast<uint8_t>(event.device);
	const auto interface_id = physical_devices[device_idx].GetMappedInterfaceId(); 
	const bool no_interface = !interface_id.has_value();

	switch (event.type) {
	case MANYMOUSE_EVENT_ABSMOTION:
		// LOG_INFO("MANYMOUSE #%u ABSMOTION axis %d, %d", event.device,
		// event.item, event.value);
		break;

	case MANYMOUSE_EVENT_RELMOTION:
		// LOG_INFO("MANYMOUSE #%u RELMOTION axis %d, %d", event.device,
		// event.item, event.value);
		if (no_interface || critical_only)
			break; // movements not relevant at this moment
		if (event.item != 0 && event.item != 1)
			break; // only movements related to x and y axis are
			       // relevant

		if (rel_x.size() <= device_idx) {
			rel_x.resize(static_cast<size_t>(device_idx + 1), 0);
			rel_y.resize(static_cast<size_t>(device_idx + 1), 0);
		}

		if (event.item)
			rel_y[event.device] += event.value; // event.item 1
		else
			rel_x[event.device] += event.value; // event.item 0
		break;

	case MANYMOUSE_EVENT_BUTTON:
		// LOG_INFO("MANYMOUSE #%u BUTTON %u %s", event.device,
		// event.item, event.value ? "press" : "release");
		if (no_interface || (critical_only && !event.value) ||
		    (event.item > static_cast<uint8_t>(manymouse_max_button_id))) {
			// TODO: Consider supporting extra mouse buttons
			// in the future. On Linux event items 3-7 are for
			// scroll wheel(s), 8 is for SDL button X1, 9 is
			// for X2, etc. - but I don't know yet if this
			// is consistent across various platforms
			break;
		}
		MOUSE_EventButton(static_cast<MouseButtonId>(event.item),
		                  event.value,
		                  *interface_id);
		break;

	case MANYMOUSE_EVENT_SCROLL:
		// LOG_INFO("MANYMOUSE #%u WHEEL #%u %d", event.device,
		// event.item, event.value);
		if (no_interface || critical_only || (event.item != 0)) {
			break; // only the 1st wheel is supported
		}
		MOUSE_EventWheel(clamp_to_int16(-event.value), *interface_id);
		break;

	case MANYMOUSE_EVENT_DISCONNECT:
		// LOG_INFO("MANYMOUSE #%u DISCONNECT", event.device);

		if (no_interface) {
			break;
		}

		physical_devices[event.device].disconnected = true;

		for (const auto button_id : {MouseButtonId::Left,
		                             MouseButtonId::Right,
		                             MouseButtonId::Middle}) {
			MOUSE_EventButton(button_id, false, *interface_id);
		}
		MOUSE_NotifyDisconnect(*interface_id);
		break;

	default:
		// LOG_INFO("MANYMOUSE #%u (other event)", event.device);
		break;
	}
}

void ManyMouseGlue::Tick()
{
	assert(mouse_config.capture != MouseCapture::NoMouse);

	// Handle all the events from the queue
	ManyMouseEvent event;
	while (ManyMouse_PollEvent(&event)) {
		HandleEvent(event);
	}

	// Report accumulated mouse movements
	assert(rel_x.size() < UINT8_MAX);
	for (uint8_t idx = 0; idx < rel_x.size(); idx++) {
		if (rel_x[idx] == 0 && rel_y[idx] == 0) {
			continue;
		}

		const auto interface_id = physical_devices[idx].GetMappedInterfaceId();
		if (interface_id) {
			MOUSE_EventMoved(static_cast<float>(rel_x[idx]),
			                 static_cast<float>(rel_y[idx]),
			                 *interface_id);
		}

		rel_x[idx] = 0;
		rel_y[idx] = 0;
	}

	if (is_mapping_in_effect) {
		PIC_AddEvent(manymouse_tick, tick_interval);
	}
}

#else

// ManyMouse is not available

ManyMouseGlue::~ManyMouseGlue() {}

void ManyMouseGlue::RescanIfSafe()
{
	static bool already_warned = false;
	if (!already_warned) {
		LOG_ERR("MOUSE: This build has no ManyMouse support");
		already_warned = true;
	}
}

void ManyMouseGlue::ShutdownIfSafe() {}

void ManyMouseGlue::StartConfigAPI() {}

void ManyMouseGlue::StopConfigAPI() {}

bool ManyMouseGlue::ProbeForMapping(uint8_t &)
{
	return false;
}

uint8_t ManyMouseGlue::GetIdx(const std::regex &)
{
	return UINT8_MAX;
}

void ManyMouseGlue::Map(const uint8_t, const MouseInterfaceId) {}

bool ManyMouseGlue::IsMappingInEffect() const
{
	return false;
}

#endif // C_MANYMOUSE
