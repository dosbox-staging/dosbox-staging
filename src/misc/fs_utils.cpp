/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include "fs_utils.h"

#include <cassert>

#include "checks.h"
#include "std_filesystem.h"

CHECK_NARROWING();

std_fs::path simplify_path(const std_fs::path &original_path) noexcept
{
	auto ec = std::error_code();

	auto simplest_path   = original_path;
	auto simplest_length = simplest_path.string().length();

	auto keep_simplest = [&](const std_fs::path &candidate_path) {
		// If the candidate is faulty, reset the code and try again
		if (ec) {
			ec.clear();
			return;
		}
		if (candidate_path.empty())
			return;

		const auto candidate_length = candidate_path.string().length();
		if (candidate_length < simplest_length) {
			simplest_path   = candidate_path;
			simplest_length = candidate_length;
		}
		assert(!ec); // we expect to depart this function with a clean code
	};

	keep_simplest(std_fs::absolute(original_path, ec));
	keep_simplest(std_fs::canonical(original_path, ec));
	keep_simplest(std_fs::proximate(original_path, ec));

	return simplest_path;
}
