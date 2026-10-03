// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WINDOW_GEOMETRY_H
#define DOSBOX_WINDOW_GEOMETRY_H

#include <optional>
#include <string_view>

#include <SDL3/SDL_rect.h>

// Parsing and unit conversion of 'window_size' and 'window_position' setting
// values. Window sizes and positions are stored in logical units internally
// (pixels divided by the OS-level DPI scale factor).
namespace WindowGeometry {

enum class Unit { LogicalUnits, Pixels, Percentage };

// A window size (width & height) or window position (x & y) setting value
struct Setting {
	float x   = 0.0f;
	float y   = 0.0f;
	Unit unit = Unit::LogicalUnits;
};

struct Desktop {
	// Desktop size in logical units
	int width  = 0;
	int height = 0;

	// Number of pixels per logical unit
	float dpi_scale = 1.0f;
};

// Parse window sizes in 'WxH', 'WxHpx', or 'WxH%' format. Returns an empty
// optional if the value is invalid or not positive.
std::optional<Setting> ParseSize(const std::string_view value);

// Parse window positions in 'X,Y', 'X,Ypx', or 'X,Y%' format. Returns an
// empty optional if the value is invalid or negative.
std::optional<Setting> ParsePosition(const std::string_view value);

// Percentage sizes are relative to the desktop height, so the aspect ratio of
// the window is independent from the aspect ratio of the desktop.
SDL_Point SizeToLogicalUnits(const Setting& size, const Desktop& desktop);

// Percentage positions are relative to the desktop width and height.
SDL_Point PositionToLogicalUnits(const Setting& position, const Desktop& desktop);

} // namespace WindowGeometry

#endif // DOSBOX_WINDOW_GEOMETRY_H
