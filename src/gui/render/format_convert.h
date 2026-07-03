// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_FORMAT_CONVERT_H
#define DOSBOX_RENDER_FORMAT_CONVERT_H

#include <cstdint>

// Batch row converters from the VGA-emitted pixel formats to 32-bit BGRX
// pixels (byte order B, G, R, X). They replace the scanout-time scaler's
// macro-generated per-format conversions with bit-identical output.
//
// Each function converts `num_pixels` pixels from `src` into `dst`. Source
// rows are tightly packed (no padding bytes between pixels).

// Builds one palette LUT entry for ExpandIndexed8ToBgrx32(). The unused X
// byte is set to 255, matching what `GFX_MakePixel()` produced for the
// legacy scaler's palette LUT.
constexpr uint32_t MakeBgrx32PaletteEntry(const uint8_t red, const uint8_t green,
                                          const uint8_t blue)
{
	return (static_cast<uint32_t>(255) << 24) |
	       (static_cast<uint32_t>(red) << 16) |
	       (static_cast<uint32_t>(green) << 8) | blue;
}

// `palette_lut` must hold 256 entries built with MakeBgrx32PaletteEntry()
void ExpandIndexed8ToBgrx32(const uint8_t* src, uint32_t* dst,
                            const int num_pixels, const uint32_t* palette_lut);

// xRRRrrGGGggBBBbb -> RRRrrRRRGGGggGGGBBBbbBBB (5-bit channels expanded to
// 8 bits by replicating the top 3 bits; the X byte comes out 0)
void ExpandRgb555ToBgrx32(const uint16_t* src, uint32_t* dst, const int num_pixels);

// RRRrrGGggggBBBbb -> RRRrrRRRGGggggGGBBBbbBBB (channels expanded to 8 bits
// by replicating the top bits; the X byte comes out 0)
void ExpandRgb565ToBgrx32(const uint16_t* src, uint32_t* dst, const int num_pixels);

// Tightly packed B, G, R byte triplets (the X byte comes out 0)
void ExpandBgr24ToBgrx32(const uint8_t* src, uint32_t* dst, const int num_pixels);

void CopyBgrx32(const uint32_t* src, uint32_t* dst, const int num_pixels);

#endif // DOSBOX_RENDER_FORMAT_CONVERT_H
