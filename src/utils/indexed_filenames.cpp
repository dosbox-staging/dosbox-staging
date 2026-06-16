// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indexed_filenames.h"

#include <algorithm>
#include <system_error>

#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

std::string format_indexed_filename(const std::string_view basename,
                                    const int index, const std::string_view suffix)
{
	return std::string(basename) + format_str("%04d", index) +
	       std::string(suffix);
}

int find_highest_file_index(const std_fs::path& dir, const std::string_view basename,
                            const std::string_view suffix)
{
	const auto basename_lc = lowcase(basename);
	const auto suffix_lc   = lowcase(suffix);

	int highest        = 0;
	std::error_code ec = {};

	for (const auto& entry : std_fs::directory_iterator(dir, ec)) {
		if (ec) {
			return 0;
		}
		if (!entry.is_regular_file(ec)) {
			continue;
		}

		auto name = entry.path().filename().string();
		lowcase(name);

		// The middle slice between basename and suffix must be all digits.
		if (name.size() <= basename_lc.size() + suffix_lc.size()) {
			continue;
		}
		if (!name.starts_with(basename_lc) || !name.ends_with(suffix_lc)) {
			continue;
		}

		const auto middle = name.substr(basename_lc.size(),
		                                name.size() - basename_lc.size() -
		                                        suffix_lc.size());

		if (!is_digits(middle)) {
			continue;
		}

		if (const auto index = parse_int(middle)) {
			highest = std::max(highest, *index);
		}
	}

	return highest;
}
