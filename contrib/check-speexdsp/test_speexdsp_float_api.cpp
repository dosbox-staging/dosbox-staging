// SPDX-FileCopyrightText:  2022-2022 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Tests SpeexDSP's floating-point API for value-wrapping
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Usage:
//    g++ or clang++ \
//    test_speexdsp_float_api.cpp -lspeexdsp -o test_speexdsp_float_api
//
//    ./test_speexdsp_float_api && echo "passed"
//
// No output is provided because it's typically run by the build system.
//

#ifdef NDEBUG
#	undef NDEBUG
#endif // Ensure asserts are enabled
#include <cassert>

#include <array>
#include <cmath>
#include <cstdint>

#include <speex/speex_resampler.h>

// Resampling quality can range from 0 to 10
constexpr auto quality = SPEEX_RESAMPLER_QUALITY_DESKTOP;

// Latency rises in proportion to the quality, so we scale the number of frames
// to ensure we pass enough data through the resampler to get output
constexpr spx_uint32_t num_frames = 100 * (quality + 1);

// The container type used to hold the input and output array of values
using frame_array_t = std::array<float, num_frames>;

// Generates a filled array
static frame_array_t generate_frame_array(const float fill_value)
{
	// Ensure our array only has values well-beyond the 16-bit range
	constexpr auto fifty_percent_beyond_16_bit = 1.5f * INT16_MAX;
	assert(fabs(fill_value) > fifty_percent_beyond_16_bit);

	frame_array_t frames = {};
	for (auto &f : frames)
		f = fill_value;
	return frames;
}

int main()
{
	// Check our globals
	static_assert(quality >= SPEEX_RESAMPLER_QUALITY_MIN,
	              "Quality needs to be >= 0");
	static_assert(quality <= SPEEX_RESAMPLER_QUALITY_MAX,
	              "Quality needs to be <= 10");
	static_assert(num_frames > 0,
	              "Number of frames needs to be greater than zero");

	// Resampler initialization constants
	constexpr spx_uint32_t in_channels = 1;
	constexpr spx_uint32_t in_rate     = 1;
	constexpr spx_uint32_t out_rate    = 1;

	// Initialize the resampler
	auto rcode     = static_cast<int>(RESAMPLER_ERR_MAX_ERROR);
	auto resampler = speex_resampler_init(
	        in_channels, in_rate, out_rate, quality, &rcode);
	assert(rcode == RESAMPLER_ERR_SUCCESS);
	assert(resampler);

	// Prime the resampler with fill content
	speex_resampler_skip_zeros(resampler);

	// Populate the input buffer with floats larger than 16-bit ints
	constexpr auto fill_value = 75000.0f;
	const auto in = generate_frame_array(fill_value);
	auto in_frames = static_cast<spx_uint32_t>(in.size());

	// Create the output buffer
	auto out = frame_array_t();
	auto out_frames = static_cast<spx_uint32_t>(out.size());

	// Resample
	assert(resampler);
	rcode = speex_resampler_process_interleaved_float(
	        resampler, in.data(), &in_frames, out.data(), &out_frames);
	assert(rcode == RESAMPLER_ERR_SUCCESS);

	// Ensure it produced frames
	assert(out_frames && out_frames <= out.size());

	// Cleanup
	speex_resampler_destroy(resampler);
	resampler = nullptr;

	// Are the resampled values within expected bounds?
	constexpr auto within_percent = 25.0f;
	constexpr auto lower_bound = fill_value * (1.0f - within_percent / 100.0f);
	constexpr auto upper_bound = fill_value * (1.0f + within_percent / 100.0f);

	for (std::size_t i = 0; i < out_frames; ++i)
		if (out[i] < lower_bound || out[i] > upper_bound)
			return 1; // Don't trust the floating-point API

	// All values in bounds; floating-point API is reliable
	return 0;
}
