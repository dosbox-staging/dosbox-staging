// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/rgb888.h"

#include <charconv>
#include <string_view>

std::optional<Rgb888> Rgb888::FromHexString(const std::string_view hex)
{
	if (hex.empty() || hex[0] != '#') {
		return {};
	}

	const auto is_hex3 = (hex.size() == 4);
	const auto is_hex6 = (hex.size() == 7);

	if (!(is_hex3 || is_hex6)) {
		return {};
	}

	uint32_t value = 0;

	const auto start = hex.data() + 1;
	const auto end   = hex.data() + hex.size();

	if (std::from_chars(start, end, value, 16).ptr != end) {
		return {};
	}

	if (is_hex3) {
		const auto r4 = static_cast<uint8_t>((value >> 8) & 0xf);
		const auto g4 = static_cast<uint8_t>((value >> 4) & 0xf);
		const auto b4 = static_cast<uint8_t>(value & 0xf);
		return FromRgb444(r4, g4, b4);
	}

	const auto r8 = static_cast<uint8_t>((value >> 16) & 0xff);
	const auto g8 = static_cast<uint8_t>((value >> 8) & 0xff);
	const auto b8 = static_cast<uint8_t>(value & 0xff);
	return Rgb888(r8, g8, b8);
}