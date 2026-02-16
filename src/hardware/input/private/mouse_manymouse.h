// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_MOUSE_MANYMOUSE_H
#define DOSBOX_MOUSE_MANYMOUSE_H

#include "hardware/input/mouse.h"
#include "mouse_interfaces.h"

#include <optional>
#include <string>
#include <vector>

#if C_MANYMOUSE
#	include "manymouse/manymouse.h"
#endif // C_MANYMOUSE

class MousePhysical {
public:
	MousePhysical(const std::string &name);

	bool IsDisconnected() const;
	bool IsMapped() const;

	std::optional<MouseInterfaceId> GetMappedInterfaceId() const;
	const std::string &GetName() const;

private:
	friend class ManyMouseGlue;

	const std::string name = "";
	bool disconnected      = false;

	std::optional<MouseInterfaceId> mapped_id = {};
};

class ManyMouseGlue final {
public:
	static ManyMouseGlue &GetInstance();

	void RescanIfSafe();
	void ShutdownIfSafe();
	void StartConfigAPI();
	void StopConfigAPI();

	bool ProbeForMapping(uint8_t &physical_device_idx);
	uint8_t GetIdx(const std::regex &regex);

	void Map(const uint8_t physical_device_idx,
	         const MouseInterfaceId interface_id);

	bool IsMappingInEffect() const;

private:
	friend class MouseInterfaceInfoEntry;
	friend class MousePhysicalInfoEntry;

	ManyMouseGlue() = default;
	~ManyMouseGlue();
	ManyMouseGlue(const ManyMouseGlue &)            = delete;
	ManyMouseGlue &operator=(const ManyMouseGlue &) = delete;

	void Tick();
	friend void manymouse_tick(uint32_t);

#if C_MANYMOUSE

	void InitIfNeeded();
	void ShutdownForced();
	void ClearPhysicalMice();
	void Rescan();

	void UnMap(const MouseInterfaceId interface_id);
	void MapFinalize();

	void HandleEvent(const ManyMouseEvent &event,
	                 const bool critical_only = false);

	bool initialized = false;
	bool malfunction = false; // once set to false, will stay false forever
	bool is_mapping_in_effect  = false;
	bool rescan_blocked_config = false; // true = rescan blocked due to
	                                    // config API usage
	uint32_t config_api_counter = 0;

	uint8_t num_mice = 0;

	std::string driver_name = "";

	std::vector<int> rel_x = {}; // not yet reported accumulated movements
	std::vector<int> rel_y = {};

	// Limit our handling to what Settlers 1 and 2 can use, which is the
	// only known DOS game that support multiple mice.
	static constexpr auto manymouse_max_button_id = MouseButtonId::Middle;

	static constexpr uint8_t max_mice     = UINT8_MAX - 1;
	static constexpr double tick_interval = 5.0;

#endif // C_MANYMOUSE

	std::vector<MousePhysical> physical_devices = {};
};

#endif // DOSBOX_MOUSE_MANYMOUSE_H
