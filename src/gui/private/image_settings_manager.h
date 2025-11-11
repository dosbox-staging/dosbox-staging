// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMAGE_SETTINGS_MANAGER_H
#define DOSBOX_IMAGE_SETTINGS_MANAGER_H

#include <optional>

#include "gui/render/render.h"
#include "misc/video.h"

struct AutoImageAdjustmentSettings {
	CrtColorProfile crt_color_profile = {};
	int color_temperature_kelvin      = {};
};

class AutoImageAdjustmentSettingsManager {
public:
	static AutoImageAdjustmentSettingsManager& GetInstance()
	{
		static AutoImageAdjustmentSettingsManager instance;
		return instance;
	}

	std::optional<AutoImageAdjustmentSettings> GetSettings(const VideoMode& video_mode);

private:
	AutoImageAdjustmentSettingsManager()  = default;
	~AutoImageAdjustmentSettingsManager() = default;

	AutoImageAdjustmentSettings GetAutoMachineSettings(const VideoMode& video_mode) const;

	AutoImageAdjustmentSettings GetAutoGraphicsStandardSettings(
	        const VideoMode& video_mode) const;
};

#endif // DOSBOX_IMAGE_SETTINGS_MANAGER_H
