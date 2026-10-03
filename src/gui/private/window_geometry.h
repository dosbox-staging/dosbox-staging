// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WINDOW_GEOMETRY_H
#define DOSBOX_WINDOW_GEOMETRY_H

#include <optional>
#include <string_view>

#include <SDL3/SDL_rect.h>

// Window and desktop geometry helpers.
//
// The `window_size` and `window_position` settings can be specified in logical
// units, pixels, or as percentages of the desktop size. Parsed setting values
// keep the unit they were specified in. They're converted to logical units
// (the unit we use for window sizes and positions internally) when they're
// applied, using the desktop size and display scale at that time.
namespace WindowGeometry {

enum class Unit { LogicalUnits, Pixels, Percentage };

// A `window_size` setting value
struct SizeSetting {
	float w   = 0.0f;
	float h   = 0.0f;
	Unit unit = Unit::LogicalUnits;
};

// A `window_position` setting value
struct PositionSetting {
	float x   = 0.0f;
	float y   = 0.0f;
	Unit unit = Unit::LogicalUnits;
};

struct Desktop {
	// Desktop size in logical units
	float width  = 0.0f;
	float height = 0.0f;

	// Number of pixels per logical unit
	float display_scale = 1.0f;
};

// Parses window sizes in 'WxH', 'WxHpx', or 'WxH%' format (case
// insensitive). Returns nullopt if the value is invalid or not positive.
std::optional<SizeSetting> ParseWindowSizeSetting(const std::string_view value);

// Parses window positions in 'X,Y', 'X,Ypx', or 'X,Y%' format (case
// insensitive). Returns nullopt if the value is invalid or negative.
std::optional<PositionSetting> ParseWindowPositionSetting(const std::string_view value);

// Converts a window size setting to logical units. Both percentage values are
// relative to the desktop height. Only the width and height of the returned
// rectangle are set.
SDL_Rect SizeToLogicalUnits(const SizeSetting& size, const Desktop& desktop);

// Converts a window position setting to logical units. The X percentage value
// is relative to the desktop width, the Y value to the desktop height.
SDL_Point PositionToLogicalUnits(const PositionSetting& position,
                                 const Desktop& desktop);

} // namespace WindowGeometry

#endif // DOSBOX_WINDOW_GEOMETRY_H
