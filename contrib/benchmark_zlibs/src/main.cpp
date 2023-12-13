/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <zlib.h>

using namespace std::chrono;

constexpr auto OneMegabyte = 1024 * 1024;

using data_array_t = std::array<Bytef, OneMegabyte>;

static data_array_t generate_easy_data_in()
{
	data_array_t data = {};
	for (size_t i = 0; i < data.size(); ++i) {
		data[i] = static_cast<Bytef>(i % 256);
	}

	return data;
}

static void print_results(const size_t num_bytes, const microseconds elapsed_us_us)
{
	// Calculate and print compression speed in MB/s
	const auto elapsed_us_s = elapsed_us_us.count() / 1000000.0;
	const auto speed_mb_s   = static_cast<double>(num_bytes) / OneMegabyte /
	                        elapsed_us_s;
	printf("%7.1f MB/s\n", speed_mb_s);
}

static void compress_data(data_array_t data_in)
{
	static data_array_t data_out = {};

	// Loop and tally counters
	constexpr auto num_rounds = 200;
	auto remaining_rounds     = num_rounds;
	const auto ten_percent    = num_rounds / 10;

	auto total_bytes_compressed = 0;

	auto elapsed_us = microseconds(0);

	while (remaining_rounds > 0) {
		// Initialize the stream
		z_stream stream             = {};
		[[maybe_unused]] auto rcode = deflateInit(&stream,
		                                          Z_DEFAULT_COMPRESSION);
		assert(rcode == Z_OK);

		// Configure the stream
		stream.avail_in  = data_in.size();
		stream.next_in   = data_in.data();
		stream.avail_out = data_out.size();
		stream.next_out  = data_out.data();

		// Compress and record elapsed
		const auto start = high_resolution_clock::now();
		rcode            = deflate(&stream, Z_FINISH);
		assert(rcode == Z_STREAM_END);
		const auto end = high_resolution_clock::now();

		// Update tallies
		--remaining_rounds;
		total_bytes_compressed += data_in.size();
		elapsed_us += duration_cast<microseconds>(end - start);

		// Close the stream
		rcode = deflateEnd(&stream);
		assert(rcode == Z_OK);

		// Log every ten percent done
		if (remaining_rounds % ten_percent == 0) {
			printf(".");
		}
	}

	print_results(total_bytes_compressed, elapsed_us);
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	printf("easy data:");
	compress_data(generate_easy_data_in());

	return 0;
}
