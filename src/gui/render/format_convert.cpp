// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "format_convert.h"

#include <cstring>

#include "utils/checks.h"

CHECK_NARROWING();

void ExpandIndexed8ToBgrx32(const uint8_t* src, uint32_t* dst,
                            const int num_pixels, const uint32_t* palette_lut)
{
	for (auto i = 0; i < num_pixels; ++i) {
		dst[i] = palette_lut[src[i]];
	}
}

void ExpandRgb555ToBgrx32(const uint16_t* src, uint32_t* dst, const int num_pixels)
{
	for (auto i = 0; i < num_pixels; ++i) {
		const uint32_t v = src[i];

		dst[i] = ((v & (31 << 10)) << 9) | ((v & (31 << 5)) << 6) |
		         ((v & 31) << 3) | ((v & (7 << 12)) << 4) |
		         ((v & (7 << 7)) << 1) | ((v & (7 << 2)) >> 2);
	}
}

void ExpandRgb565ToBgrx32(const uint16_t* src, uint32_t* dst, const int num_pixels)
{
	for (auto i = 0; i < num_pixels; ++i) {
		const uint32_t v = src[i];

		dst[i] = ((v & (31 << 11)) << 8) | ((v & (63 << 5)) << 5) |
		         ((v & 0xE01F) << 3) | ((v & (3 << 9)) >> 1) |
		         ((v & (7 << 2)) >> 2);
	}
}

void ExpandBgr24ToBgrx32(const uint8_t* src, uint32_t* dst, const int num_pixels)
{
	for (auto i = 0; i < num_pixels; ++i) {
		const uint32_t b = src[i * 3 + 0];
		const uint32_t g = src[i * 3 + 1];
		const uint32_t r = src[i * 3 + 2];

		dst[i] = (r << 16) | (g << 8) | b;
	}
}

void CopyBgrx32(const uint32_t* src, uint32_t* dst, const int num_pixels)
{
	std::memcpy(dst, src, static_cast<size_t>(num_pixels) * sizeof(uint32_t));
}

void ExpandBgrx32Row(const uint32_t* src, uint32_t* dst, const int src_width_px,
                     const bool double_width)
{
	if (!double_width) {
		CopyBgrx32(src, dst, src_width_px);
		return;
	}

	for (auto x = 0; x < src_width_px; ++x) {
		dst[x * 2 + 0] = src[x];
		dst[x * 2 + 1] = src[x];
	}
}
