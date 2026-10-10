// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/window_geometry.h"

#include <cmath>
#include <string>
#include <utility>

#include "misc/support.h"
#include "utils/checks.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace WindowGeometry {

int LogicalToNative(const int logical, const float content_scale)
{
	// Special window position values must be passed to SDL unchanged
	if (SDL_WINDOWPOS_ISUNDEFINED(logical) || SDL_WINDOWPOS_ISCENTERED(logical)) {
		return logical;
	}
	return iroundf(static_cast<float>(logical) * content_scale);
}

int NativeToLogical(const int native, const float content_scale)
{
	return iroundf(static_cast<float>(native) / content_scale);
}

int LogicalSizeToNative(const float logical, const float content_scale)
{
	return iroundf(logical * content_scale);
}

float NativeSizeToLogical(const int native, const float content_scale)
{
	return static_cast<float>(native) / content_scale;
}

Desktop CalcDesktop(const SDL_DisplayMode& desktop_mode, const float content_scale)
{
	assert(content_scale > 0.0f);

	// We don't round the desktop size in logical units so we can convert it
	// back to pixels exactly
	return {static_cast<float>(desktop_mode.w) / content_scale,
	        static_cast<float>(desktop_mode.h) / content_scale,
	        content_scale * desktop_mode.pixel_density};
}

static std::pair<std::string, Unit> split_unit_suffix(const std::string& value)
{
	if (value.ends_with("px")) {
		return {strip_suffix(value, "px"), Unit::Pixels};
	}
	if (value.ends_with('%')) {
		return {strip_suffix(value, "%"), Unit::Percentage};
	}
	return {value, Unit::LogicalUnits};
}

// Two numbers in the same unit, separated by a delimiter
struct NumberPair {
	float first  = 0.0f;
	float second = 0.0f;
	Unit unit    = Unit::LogicalUnits;
};

// Only percentages can have fractional values
static std::optional<float> parse_number(const std::string& s, const Unit unit)
{
	switch (unit) {
	case Unit::Percentage: {
		constexpr auto MaxPercentage = 1000.0f;

		// Also rejects NaN and infinity
		const auto percentage = parse_float(s);

		if (percentage && std::abs(*percentage) <= MaxPercentage) {
			return percentage;
		}
		return {};
	}

	case Unit::LogicalUnits:
	case Unit::Pixels: {
		// Large enough for any display, and small enough to avoid
		// integer overflows when converting between units
		constexpr auto MaxValue = 65535;

		if (const auto number = parse_int(s);
		    number && *number >= -MaxValue && *number <= MaxValue) {
			return static_cast<float>(*number);
		}
		return {};
	}

	default: assertm(false, "Invalid unit"); return {};
	}
}

static std::optional<NumberPair> parse(const std::string_view value,
                                       const std::string_view delimiters)
{
	const auto [numbers, unit] = split_unit_suffix(lowcase(value));

	const auto parts = split_with_empties(numbers, delimiters);
	if (parts.size() != 2) {
		return {};
	}

	const auto first  = parse_number(parts[0], unit);
	const auto second = parse_number(parts[1], unit);
	if (!first || !second) {
		return {};
	}
	return NumberPair{*first, *second, unit};
}

static std::optional<SizeSetting> parse_named_size(const std::string_view name)
{
	const auto make_4x3_size = [](const float height_percentage) {
		return SizeSetting{height_percentage * 4.0f / 3.0f,
		                   height_percentage,
		                   Unit::Percentage};
	};

	if (name == "s" || name == "small") {
		return make_4x3_size(50.0f);
	}
	if (name == "m" || name == "medium" || name == "default") {
		return make_4x3_size(74.0f);
	}
	if (name == "l" || name == "large") {
		return make_4x3_size(90.0f);
	}
	return {};
}

std::optional<SizeSetting> ParseWindowSizeSetting(const std::string_view value)
{
	if (const auto named_size = parse_named_size(lowcase(value)); named_size) {
		return named_size;
	}

	const auto size = parse(value, "x,");
	if (!size || size->first <= 0.0f || size->second <= 0.0f) {
		return {};
	}
	return SizeSetting{size->first, size->second, size->unit};
}

std::optional<PositionSetting> ParseWindowPositionSetting(const std::string_view value)
{
	const auto position = parse(value, ",");
	if (!position || position->first < 0.0f || position->second < 0.0f) {
		return {};
	}
	return PositionSetting{position->first, position->second, position->unit};
}

std::optional<PositionSetting> ParseViewportPositionSetting(const std::string_view value)
{
	const auto position = parse(value, ",");
	if (!position) {
		return {};
	}
	return PositionSetting{position->first, position->second, position->unit};
}

SDL_FRect SizeToLogicalUnits(const SizeSetting& size, const Desktop& desktop)
{
	switch (size.unit) {
	case Unit::LogicalUnits: return {0.0f, 0.0f, size.w, size.h};

	case Unit::Pixels:
		return {0.0f,
		        0.0f,
		        size.w / desktop.display_scale,
		        size.h / desktop.display_scale};

	case Unit::Percentage: {
		const auto one_percent = desktop.height / 100.0f;
		return {0.0f, 0.0f, size.w * one_percent, size.h * one_percent};
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

SDL_Point PositionToLogicalUnits(const PositionSetting& position, const Desktop& desktop)
{
	switch (position.unit) {
	case Unit::LogicalUnits:
		return {iroundf(position.x), iroundf(position.y)};

	case Unit::Pixels:
		return {iroundf(position.x / desktop.display_scale),
		        iroundf(position.y / desktop.display_scale)};

	case Unit::Percentage: {
		const auto one_percent_x = desktop.width / 100.0f;
		const auto one_percent_y = desktop.height / 100.0f;

		return {iroundf(position.x * one_percent_x),
		        iroundf(position.y * one_percent_y)};
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

static std::string format_percentage(const float percentage)
{
	// Two decimal places are precise enough
	return format_str("%g", std::round(percentage * 100.0f) / 100.0f);
}

std::string FormatSize(const SDL_FRect size, const Unit unit, const Desktop& desktop)
{
	switch (unit) {
	case Unit::LogicalUnits:
		return format_str("%dx%d", iroundf(size.w), iroundf(size.h));

	case Unit::Pixels:
		return format_str("%dx%dpx",
		                  iroundf(size.w * desktop.display_scale),
		                  iroundf(size.h * desktop.display_scale));

	case Unit::Percentage: {
		const auto one_percent = desktop.height / 100.0f;

		return format_percentage(size.w / one_percent) + "x" +
		       format_percentage(size.h / one_percent) + "%";
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

std::string FormatPosition(const SDL_Point position, const Unit unit,
                           const Desktop& desktop)
{
	switch (unit) {
	case Unit::LogicalUnits:
		return format_str("%d,%d", position.x, position.y);

	case Unit::Pixels:
		return format_str("%d,%dpx",
		                  iroundf(static_cast<float>(position.x) *
		                          desktop.display_scale),
		                  iroundf(static_cast<float>(position.y) *
		                          desktop.display_scale));

	case Unit::Percentage: {
		const auto one_percent_x = desktop.width / 100.0f;
		const auto one_percent_y = desktop.height / 100.0f;

		return format_percentage(static_cast<float>(position.x) /
		                         one_percent_x) +
		       "," +
		       format_percentage(static_cast<float>(position.y) /
		                         one_percent_y) +
		       "%";
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

} // namespace WindowGeometry
