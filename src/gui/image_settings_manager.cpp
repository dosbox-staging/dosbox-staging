// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/image_settings_manager.h"

#include "private/shader_manager.h"

#include "utils/checks.h"

CHECK_NARROWING();

static AutoImageAdjustmentSettings monochrome_settings = {CrtColorProfile::None, 6500};
static AutoImageAdjustmentSettings composite_settings = {CrtColorProfile::SmpteC,
                                                         6500};

static AutoImageAdjustmentSettings cga_settings = {CrtColorProfile::P22, 9300};
static AutoImageAdjustmentSettings ega_settings = {CrtColorProfile::P22, 9300};
static AutoImageAdjustmentSettings vga_settings = {CrtColorProfile::Trinitron, 6500};

static AutoImageAdjustmentSettings arcade_settings = {CrtColorProfile::Philips, 6500};

std::optional<AutoImageAdjustmentSettings> AutoImageAdjustmentSettingsManager::GetSettings(
        const VideoMode& video_mode)
{
	using enum ShaderMode;

	switch (ShaderManager::GetInstance().GetCurrentShaderMode()) {
	case Single: return {};

	case AutoGraphicsStandard:
		return GetAutoGraphicsStandardSettings(video_mode);

	case AutoMachine: return GetAutoMachineSettings(video_mode);

	case AutoArcade:
	case AutoArcadeSharp: return arcade_settings;

	default: assertm(false, "Invalid ShaderMode value"); return {};
	}
}

AutoImageAdjustmentSettings AutoImageAdjustmentSettingsManager::GetAutoMachineSettings(
        const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return composite_settings;
	}

	using enum MachineType;

	switch (machine) {
	case Hercules: return monochrome_settings;

	case CgaMono:
	case CgaColor:
	case Pcjr: return cga_settings;

	case Tandy:
	case Ega: return ega_settings;

	case Vga: return vga_settings;
	default: assertm(false, "Invalid MachineType value"); return {};
	};
}

AutoImageAdjustmentSettings AutoImageAdjustmentSettingsManager::GetAutoGraphicsStandardSettings(
        const VideoMode& video_mode) const
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return composite_settings;
	}

	using enum GraphicsStandard;

	switch (video_mode.graphics_standard) {
	case Hercules: return monochrome_settings;

	case Cga:
	case Pcjr: return cga_settings;

	case Tga: return ega_settings;

	case Ega:
		// Use VGA settings for VGA games that use EGA modes with an
		// 18-bit VGA palette.
		return video_mode.has_vga_colors ? vga_settings : ega_settings;

	case Vga:
	case Svga:
	case Vesa: return vga_settings;

	default: assertm(false, "Invalid GraphicsStandard value"); return {};
	}
}
