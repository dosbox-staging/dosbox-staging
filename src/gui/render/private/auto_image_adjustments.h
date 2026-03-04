// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_AUTO_IMAGE_ADJUSTMENTS_H
#define DOSBOX_AUTO_IMAGE_ADJUSTMENTS_H

#include <optional>

#include "gui/render/render.h"
#include "misc/video.h"

struct AutoImageAdjustments {
	float black_level                 = 0.0f;
	CrtColorProfile crt_color_profile = CrtColorProfile::None;
	float color_temperature_kelvin    = 6500.0f;
};

// Image adjustment settings manager that works together with the adaptive CRT
// shaders to apply automatic CRT colour profile and colour temperature
// settings based on the current video mode or machine type.
//
class AutoImageAdjustmentsManager {
public:
	static AutoImageAdjustmentsManager& GetInstance()
	{
		static AutoImageAdjustmentsManager instance;
		return instance;
	}

	std::optional<AutoImageAdjustments> GetSettings(const VideoMode& video_mode) const;

private:
	AutoImageAdjustmentsManager()  = default;
	~AutoImageAdjustmentsManager() = default;

	AutoImageAdjustments GetAutoMachineSettings(const VideoMode& video_mode) const;

	AutoImageAdjustments GetAutoGraphicsStandardSettings(const VideoMode& video_mode) const;
};

#endif // DOSBOX_AUTO_IMAGE_ADJUSTMENTS_H
