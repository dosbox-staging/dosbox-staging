// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_AUTO_IMAGE_SETTINGS_H
#define DOSBOX_AUTO_IMAGE_SETTINGS_H

#include <optional>

#include "gui/render/render.h"
#include "misc/video.h"

struct AutoImageSettings {
	CrtColorProfile crt_color_profile = {};
	float white_point                 = {};
};

class ImageSettingsManager {
public:
	static ImageSettingsManager& GetInstance()
	{
		static ImageSettingsManager instance;
		return instance;
	}

	std::optional<AutoImageSettings> GetSettings(const VideoMode& video_mode);

private:
	ImageSettingsManager()  = default;
	~ImageSettingsManager() = default;

	AutoImageSettings GetAutoMachineSettings(const VideoMode& video_mode) const;

	AutoImageSettings GetAutoGraphicsStandardSettings(const VideoMode& video_mode) const;
};

#endif // DOSBOX_AUTO_IMAGE_SETTINGS_H
