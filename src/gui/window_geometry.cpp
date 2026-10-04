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

constexpr auto MaxPercentage = 1000.0f;

static std::pair<std::string, Unit> split_unit_suffix(const std::string& value)
{
	if (value.ends_with("px")) {
		return {value.substr(0, value.size() - 2), Unit::Pixels};
	}
	if (value.ends_with('%')) {
		return {value.substr(0, value.size() - 1), Unit::Percentage};
	}
	return {value, Unit::LogicalUnits};
}

static std::optional<Setting> parse(const std::string_view value,
                                    const std::string_view delimiters)
{
	const auto [numbers, unit] = split_unit_suffix(lowcase(value));

	const auto parts = split(numbers, delimiters);
	if (parts.size() != 2) {
		return {};
	}

	// Only percentages can have fractional values
	const auto parse_number = [&](const std::string& s) -> std::optional<float> {
		if (unit == Unit::Percentage) {
			const auto percentage = parse_float(s);
			if (percentage && *percentage >= 0.0f &&
			    *percentage <= MaxPercentage) {
				return percentage;
			}
			return {};
		}
		if (const auto number = parse_int(s); number) {
			return static_cast<float>(*number);
		}
		return {};
	};

	const auto x = parse_number(parts[0]);
	const auto y = parse_number(parts[1]);
	if (!x || !y) {
		return {};
	}
	return Setting{*x, *y, unit};
}

std::optional<Setting> ParseSize(const std::string_view value)
{
	const auto size = parse(value, "x, ");
	if (!size || size->x <= 0.0f || size->y <= 0.0f) {
		return {};
	}
	return size;
}

std::optional<Setting> ParsePosition(const std::string_view value)
{
	const auto position = parse(value, ", ");
	if (!position || position->x < 0.0f || position->y < 0.0f) {
		return {};
	}
	return position;
}

SDL_Point SizeToLogicalUnits(const Setting& size, const Desktop& desktop)
{
	switch (size.unit) {
	case Unit::LogicalUnits: return {iroundf(size.x), iroundf(size.y)};

	case Unit::Pixels:
		return {iroundf(size.x / desktop.dpi_scale),
		        iroundf(size.y / desktop.dpi_scale)};

	case Unit::Percentage: {
		const auto one_percent = static_cast<float>(desktop.height) / 100.0f;
		return {iroundf(size.x * one_percent), iroundf(size.y * one_percent)};
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

SDL_Point PositionToLogicalUnits(const Setting& position, const Desktop& desktop)
{
	switch (position.unit) {
	case Unit::LogicalUnits:
		return {iroundf(position.x), iroundf(position.y)};

	case Unit::Pixels:
		return {iroundf(position.x / desktop.dpi_scale),
		        iroundf(position.y / desktop.dpi_scale)};

	case Unit::Percentage: {
		const auto one_percent_x = static_cast<float>(desktop.width) / 100.0f;
		const auto one_percent_y = static_cast<float>(desktop.height) / 100.0f;

		return {iroundf(position.x * one_percent_x),
		        iroundf(position.y * one_percent_y)};
	}

	default: assertm(false, "Invalid Unit value"); return {};
	}
}

static std::string format_percentage(const float percentage)
{
	// One decimal place is precise enough
	return format_str("%g", std::round(percentage * 10.0f) / 10.0f);
}

std::string FormatSize(const SDL_Point size, const Unit unit, const Desktop& desktop)
{
	switch (unit) {
	case Unit::LogicalUnits: return format_str("%dx%d", size.x, size.y);

	case Unit::Pixels:
		return format_str(
		        "%dx%dpx",
		        iroundf(static_cast<float>(size.x) * desktop.dpi_scale),
		        iroundf(static_cast<float>(size.y) * desktop.dpi_scale));

	case Unit::Percentage: {
		const auto one_percent = static_cast<float>(desktop.height) / 100.0f;

		return format_percentage(static_cast<float>(size.x) / one_percent) +
		       "x" +
		       format_percentage(static_cast<float>(size.y) / one_percent) +
		       "%";
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
		                          desktop.dpi_scale),
		                  iroundf(static_cast<float>(position.y) *
		                          desktop.dpi_scale));

	case Unit::Percentage: {
		const auto one_percent_x = static_cast<float>(desktop.width) / 100.0f;
		const auto one_percent_y = static_cast<float>(desktop.height) / 100.0f;

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
