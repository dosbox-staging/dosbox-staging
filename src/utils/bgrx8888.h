// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_BGRX8888_H
#define DOSBOX_BGRX8888_H

#include <cstdint>

// A class that holds the colour channels as an array of four 8-bit values
// in byte order (also known as memory order), regardless of host
// endianness. The bytes in memory are always Blue, Green, Red, and finally
// a placeholder.
//
class Bgrx8888 {
public:
	constexpr Bgrx8888() = default;

	Bgrx8888(const uint8_t r, const uint8_t g, const uint8_t b)
	{
		colour.components = {b, g, r, 0};
	}

	constexpr uint8_t Blue8() const
	{
		return colour.components.b8;
	}

	constexpr uint8_t Green8() const
	{
		return colour.components.g8;
	}

	constexpr uint8_t Red8() const
	{
		return colour.components.r8;
	}

	// Cast operator to read-only uint32_t
	constexpr operator uint32_t() const
	{
		return colour.bgrx8888;
	}

private:
	union {
		struct {
			// byte order
			uint8_t b8 = 0;
			uint8_t g8 = 0;
			uint8_t r8 = 0;
			uint8_t x8 = 0;
		} components;
		uint32_t bgrx8888 = 0;
	} colour = {};
};

static_assert(sizeof(Bgrx8888) == sizeof(uint32_t));

#endif
