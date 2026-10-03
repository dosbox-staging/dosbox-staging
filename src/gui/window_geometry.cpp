// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/window_geometry.h"

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

} // namespace WindowGeometry
